/**
 * @file button.c
 * @author Ryan Banks
 * @date 2023
 * @brief Component to manage hardware associated with push-buttons.
 *
 * For this project, there are a couple of buttons which are used in order to
 * command the hardware in a straightforward manner. Due to a lack of GPIO, I
 * chose to only solder leads for the power and oscillation buttons on the front
 * fascia of the fan.
 *
 * In order to get input from these buttons, interrupts are registered which are
 * triggered on the positive edge of the button GPIO pin signal. To avoid
 * bouncing, the time of button presses is stored and a minimum wait time of
 * 250 msec between presses is enforced. Internal pulldown resistors are also
 * used to ensure we get a clean logic low/high.
 *
 * Instead of handling hardware changes themselves, the button interrupts will
 * construct event structures which are then enqueued to the main task queue. It
 * will handle the events from there.
 * 
 * @addtogroup Button
 * @{
 */

#include <stdbool.h>            /* Boolean type */
#include <sys/time.h>           /* Time functions */

#include <esp_log.h>            /* ESP logging functions */
#include <esp_attr.h>           /* Extra function attributes (IRAM_ATTR) */
#include <driver/gpio.h>        /* ESP GPIO-related functions */
#include <freertos/FreeRTOS.h>  /* Basic FreeRTOS functions */
#include <freertos/queue.h>     /* Definitions for inter-task queues */

#include "main.h"
#include "button.h"

/** Total number of buttons */
#define NUM_TOTAL_BUTTON 2
/** Number of uSecs in a mSec */
#define NUM_USEC_IN_MSEC 1000
/** Minimum amount of time (in mSec) between button presses */
#define MIN_TIME_BETWEEN_PRESS (250 * NUM_USEC_IN_MSEC)

/** Tag used for ESP logging */
static const char *TAG = "Button";

/** Time of the last button press, for debounce */
static int64_t prev_time_us;

/** Array to help with mass-setting GPIO pins for buttons */
static gpio_num_t buttons[] = {
    FAN_PWR_BUTTON_GPIO,
    FAN_OSC_BUTTON_GPIO,
};

/**
 * @brief   Interrupt for the power button GPIO.
 * @details Builds an event structure for the event, places it in the queue to
 *          be handled by the main task.
 * @param   args [in] Required argument from FreeRTOS
 */
static void IRAM_ATTR power_interrupt(void *args)
{
    (void) args;

    /* Ensure we aren't receiving bounces */
    struct timeval tv_now;
    (void) gettimeofday(&tv_now, NULL);
    int64_t time_us = (int64_t) tv_now.tv_sec * 1000000L + (int64_t) tv_now.tv_usec;
    if (time_us - prev_time_us < MIN_TIME_BETWEEN_PRESS)
    {
        /* If the required amount of time hasn't elapsed, this is a bounce! */
        goto exit;
    }

    /* We have not woken a task at the start of the ISR. */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Simple power event to send to the main task */
    Fan_event_t event = {
        .source = SOURCE_BUTTON,
        .id     = ID_POWER
    };

    (void) xQueueSendFromISR(g_Fan_event_queue, &event, &xHigherPriorityTaskWoken);

    /* Update the previous time now */
    prev_time_us = time_us;

exit:
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief   Interrupt for the oscillate button GPIO.
 * @details Builds an event structure for the event, places it in the queue to
 *          be handled by the main task.
 * @param   args [in] Required argument from FreeRTOS
 */
static void IRAM_ATTR oscillate_interrupt(void *args)
{
    (void) args;

    /* Ensure we aren't receiving bounces */
    struct timeval tv_now;
    (void) gettimeofday(&tv_now, NULL);
    int64_t time_us = (int64_t) tv_now.tv_sec * 1000000L + (int64_t) tv_now.tv_usec;
    if (time_us - prev_time_us < MIN_TIME_BETWEEN_PRESS)
    {
        /* If the required amount of time hasn't elapsed, this is a bounce! */
        goto exit;
    }

    /* We have not woken a task at the start of the ISR. */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Simple oscillation event to send to the main task */
    Fan_event_t event = {
        .source = SOURCE_BUTTON,
        .id     = ID_OSCILLATE
    };

    (void) xQueueSendFromISR(g_Fan_event_queue, &event, &xHigherPriorityTaskWoken);

    /* Update the previous time now */
    prev_time_us = time_us;

exit:
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief   Initialization function for the Button component.
 * @details Initializes the GPIO pins associated with buttons. Adds interrupt
 *          handlers that are activated on button presses.
 */
void Button_init(void)
{
    /* Initialize the basic GPIO attributes for our buttons */
    for (int i = 0; i < NUM_TOTAL_BUTTON; i++)
    {
        esp_rom_gpio_pad_select_gpio(buttons[i]);
        gpio_set_direction(buttons[i], GPIO_MODE_INPUT);
        gpio_pulldown_en(buttons[i]);
        gpio_pullup_dis(buttons[i]);
        gpio_set_intr_type(buttons[i], GPIO_INTR_POSEDGE);
    }
    
    /* Get the ISR service set up for the board */
    gpio_install_isr_service(0);

    /* Install interrupts for our two buttons */
    gpio_isr_handler_add(FAN_PWR_BUTTON_GPIO, power_interrupt, NULL);
    gpio_isr_handler_add(FAN_OSC_BUTTON_GPIO, oscillate_interrupt, NULL);

    /* Get the current time for button debouncing */
    struct timeval tv_now;
    (void) gettimeofday(&tv_now, NULL);
    prev_time_us = (int64_t) tv_now.tv_sec * 1000000L + (int64_t) tv_now.tv_usec;

    ESP_LOGI(TAG, "Button component init!");
}

/** @} end Button */
