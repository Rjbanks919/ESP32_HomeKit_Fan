/**
 * @file remote.c
 * @author Ryan Banks
 * @date 2023
 * @brief Component to manage hardware associated with the IR remote.
 *
 * For this project, I wanted to provide a way for a user to control the fan
 * without requiring a smart device or the front-fascia buttons. To do so, I
 * implemented support for the original infrared (IR) remote.
 *
 * The IR remote sends out distinct data packets whenever a button is pressed,
 * which are received by the original IR sensor in the fan front-fascia. This
 * receiver is connected to the RMT component within the ESP32. This component
 * is able to pull data from the sensor and pass it off in a reasonably simple
 * format.
 *
 * Whenever new data comes in, a callback is fired which queues that data up to
 * be processed by the remote handler task in this file. This will attempt to
 * parse incoming data and extract a remote command. If one is found, it packed
 * into an event and enqueued for the main event handler to process.
 * 
 * @addtogroup Remote
 * @{
 */

#include <esp_log.h>           /* ESP logging functions */
#include <freertos/FreeRTOS.h> /* Basic FreeRTOS functions */
#include <freertos/task.h>     /* Definitions for creating tasks */
#include <freertos/queue.h>    /* Definitions for inter-task queues */
#include <freertos/projdefs.h> /* Special FreeRTOS definitions */
#include <driver/rmt_rx.h>     /* ESP RMT receiver-related functions */

#include "main.h"
#include "remote.h"

/** Infrared codes for our remote */
#define IR_CODE_POWER          0x13F
#define IR_CODE_OSCILLATE      0x13B
#define IR_CODE_SPEED          0x13D
#define IR_CODE_TIME           0x13E
#define IR_CODE_TEMPERATURE    0x12F

/** Expected number of RMT symbols in a Lasko IR code */
#define NUM_SYMBOL_EXPECTED       11

/** Resolution that the RMT will operate at, 1MHz resolution, 1 tick = 1us */
#define RMT_IR_RESOLUTION_HZ 1000000

/** Amount of RMT symbols that our channel can store at a time */
#define RMT_MAX_MEM_SYMBOLS       48

/** Margin of error for identifying durations */
#define DURATION_ERROR_MARGIN    200

/** Two lengths of durations to identify pulses */
#define DURATION_SHORT           400
#define DURATION_LONG           1200

/** Arguments for creating the main event handler task */
#define REMOTE_HANDLER_NAME      "RemoteHandler"
#define REMOTE_HANDLER_STACKSIZE       4 * 1024
#define REMOTE_HANDLER_PRIORITY               1

/** Tag used for ESP logging */
static const char *TAG = "Remote";

/** Configuration structure for the RMT receiver channel */
static rmt_receive_config_t receive_config;

/** Handle for our RMT receiver channel */
static rmt_channel_handle_t rx_channel;

/** Local queue for passing received RMT symbols to our parsing task */
static QueueHandle_t receive_queue;

/**
 * @brief Check whether a duration is within an expected range.
 * @param signal_duration Duration being range-checked.
 * @param spec_duration   Duration to check against.
 * @return Whether signal_duration was in range of spec_duration.
 */
static inline bool check_in_range(uint32_t signal_duration, uint32_t spec_duration)
{
    return (signal_duration < (spec_duration + DURATION_ERROR_MARGIN)) &&
           (signal_duration > (spec_duration - DURATION_ERROR_MARGIN));
}

/**
 * @brief  Check whether a RMT symbol represents logic zero.
 * @param  rmt_symbols [in] RMT symbol to parse.
 * @return Whether symbol represents a logic 0.
 */
static bool parse_logic0(rmt_symbol_word_t *symbol)
{
    return check_in_range(symbol->duration0, DURATION_LONG) &&
           check_in_range(symbol->duration1, DURATION_SHORT);
}

/**
 * @brief  Check whether a RMT symbol represents logic one.
 * @param  symbol [in] RMT symbol to parse.
 * @return Whether symbol represents a logic 1.
 */
static bool parse_logic1(rmt_symbol_word_t *symbol)
{
    return check_in_range(symbol->duration0, DURATION_SHORT) &&
           check_in_range(symbol->duration1, DURATION_LONG);
}

/**
 * @brief   Checks whether a given command code is a valid command. Sends event.
 * @details There are a set number of IR codes that are recognized as valid IR
 *          commands for the Lasko fan. If the command code matches any of them,
 *          create an event and place it in the main event handler queue.
 * @param   ir_code IR command code to check.
 * @return  Whether the command code was a valid command.
 */
static bool is_command(uint16_t ir_code)
{
    Fan_event_t event = { .source = SOURCE_REMOTE };

    /* Determine the appropriate event ID */

    switch (ir_code)
    {
        case IR_CODE_POWER:
            /* Power state command */
            event.id = ID_POWER;
            break;
        case IR_CODE_OSCILLATE:
            /* Oscillation state command */
            event.id = ID_OSCILLATE;
            break;
        case IR_CODE_SPEED:
            /* Speed state command */
            event.id = ID_SPEED;
            break;
        case IR_CODE_TIME:
            /* Time button command */
            event.id = ID_TIME;
            break;
        case IR_CODE_TEMPERATURE:
            /* Temperature button command */
            event.id = ID_TEMPERATURE;
            break;
        default:
            return false;
    }

    /* Send the event to the main queue, wait 10 ticks if necessary */
    (void) xQueueSend(g_Fan_event_queue, &event, (TickType_t) 10);

    return true;
}

/**
 * @brief   Parse RMT symbols to extract a valid IR code.
 * @details So this method of parsing RMT symbols is rather specific to this use
 *          case. I found through experimentation that whenever I would press a
 *          button on the IR remote, there were 11 RMT symbols received,
 *          followed by a very long duration.
 *
 *          With this, I reasoned that valid codes were 11 symbols long and went
 *          about determining what each was. In order to distinguish symbols
 *          from each other, I defined long and short durations. Long durations
 *          are roughly 1200 uSec, whereas short durations are roughly 400 uSec.
 *
 *          With this, I was able to parse a symbol into either a logic 0, or
 *          logic 1 based upon the durations it contained. They were created as
 *          such:
 *
 *          logic 0: (long , short)
 *          logic 1: (short, long )
 *
 *          With this, I am able to craft an 11-bit IR code which I place into a
 *          16-bit variable. To do so, I bit shift a 1 starting with 10 and then
 *          descending based upon how many bits I have set already.
 *
 *          Upon completing this algorithm, we have an IR code to check the
 *          validity of!
 * @param   rmt_symbols [in] Array of RMT symbols to parse.
 * @param   num_symbols Number of RMT symbols present in the array.
 */
static void parse_ir_code(rmt_symbol_word_t *rmt_symbols, size_t num_symbols)
{
    if (RMT_MAX_MEM_SYMBOLS != num_symbols)
    {
        /* We expect to fill up the buffer with valid presses, all else is noise */
        return;
    }

    /* Iterate through the collected RMT symbols, try to build an IR code */
    uint16_t ir_code   = 0;
    size_t   bit_index = 0;
    for (size_t i = 0; i < num_symbols; i++)
    {
        if (parse_logic1(&rmt_symbols[i]))
        {
            /* Set the bit, since this is logic 1 */
            ir_code |= (1 << (NUM_SYMBOL_EXPECTED - bit_index - 1));
        }
        else if (!parse_logic0(&rmt_symbols[i]))
        {
            /* Bad RMT symbols, exit */
            return;
        }

        bit_index += 1;

        /* Check whether we have a command! */
        if (NUM_SYMBOL_EXPECTED == bit_index && !is_command(ir_code))
        {
            /* If it was not a valid command, reset counters */
            ir_code = 0;
            bit_index = 0;
        }

        /* If IR code was valid, event has been sent! */
    }
}

/**
 * @brief  Receiver callback for the RMT channel.
 * @details This callback will fire any time our RMT channel receives some
 *          amount of data. The received data is passed to us in the form of RMT
 *          symbols that will be parsed later.
 * @param channel
 * @param edata     [in]
 * @param user_data [in]
 */
static bool rmt_rx_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    /* We have not woken a task at the start of the ISR. */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    UNUSED_PARAM(user_data);
    
    /* Send the received RMT symbols to the parsing task */
    xQueueSendFromISR(receive_queue, edata, &xHigherPriorityTaskWoken);

    return (xHigherPriorityTaskWoken == pdTRUE);
}

/**
 * @brief FreeRTOS task for receiving data on the RMT channel. 
 * @param p [in]
 */
static void Remote_task(void *p)
{
    rmt_symbol_word_t raw_symbols[RMT_MAX_MEM_SYMBOLS];
    rmt_rx_done_event_data_t rx_data;
    
    UNUSED_PARAM(p);

    /* Begin receiving RMT data, start the run loop */
    ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));
    while (true) 
    {
        // wait for RX done signal
        if (xQueueReceive(receive_queue, &rx_data, portMAX_DELAY)) 
        {
            // parse the receive symbols and print the result
            parse_ir_code(rx_data.received_symbols, rx_data.num_symbols);
            // start receive again
            ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));
        }
    }
}

/**
 * @brief   Initializer for the Remote component.
 * @details Initializes the GPIO pins associated with buttons. Adds interrupt
 *          handlers that are activated on button presses.
 */
void Remote_init(void)
{
    /* Make the RMT receiver channel */
    rmt_rx_channel_config_t rx_channel_cfg = {
        .clk_src           =  RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = RMT_IR_RESOLUTION_HZ,
        .mem_block_symbols =  RMT_MAX_MEM_SYMBOLS,
        .gpio_num          = FAN_IR_SENSOR_GPIO,
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_channel_cfg, &rx_channel));

    /* Setup a queue to pass RMT data with */
    receive_queue = xQueueCreate(10, sizeof(rmt_rx_done_event_data_t));

    /* Register the ISR to handle RMT receives */
    rmt_rx_event_callbacks_t cbs = { .on_recv_done = rmt_rx_callback };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_channel, &cbs, receive_queue));

    /* Enable the RMT signal */
    ESP_ERROR_CHECK(rmt_enable(rx_channel));

    /**
     * The shortest duration for signal is 560us, 1250ns < 560us, valid
     * signal won't be treated as noise
     *
     * The longest duration for signal is 9000us, 12000000ns > 9000us, the
     * receive won't stop early.
     */
    receive_config.signal_range_min_ns = 1250;
    receive_config.signal_range_max_ns = 12000000;

    /* Create our event handler task */
    (void) xTaskCreate(
        Remote_task, 
        REMOTE_HANDLER_NAME,
        REMOTE_HANDLER_STACKSIZE,
        NULL, 
        REMOTE_HANDLER_PRIORITY,
        NULL);

    ESP_LOGI(TAG, "Remote component init!");
}

/** @} end Remote */
