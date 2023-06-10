/**
 * @file main.c
 * @author Ryan Banks
 * @date 2023
 * @brief Initialization and main event handler tasks.
 *
 * This is the center of the project! This file contains a couple FreeRTOS tasks
 * which handle the core functionality of the fan. This includes the initial
 * setup of hardware/software and event handling beyond that.
 * 
 * @addtogroup Main
 * @{
 */

#include <stdbool.h>           /* Boolean type */

#include <esp_log.h>           /* ESP logging functions */
#include <freertos/FreeRTOS.h> /* Basic FreeRTOS functions */
#include <freertos/task.h>     /* Definitions for creating tasks */
#include <freertos/queue.h>    /* Definitions for inter-task queues */

#include "main.h"
#include "relay.h"
#include "led.h"
#include "button.h"
#include "remote.h"
#include "homekit.h"
#include "event_handlers.h"

/** Arguments for creating the main event handler task */
#define EVENT_HANDLER_NAME      "EventHandler"
#define EVENT_HANDLER_STACKSIZE       4 * 1024
#define EVENT_HANDLER_PRIORITY               1

/** Global current state of the fan */
Fan_state_t g_Fan_state;

/** Global queue to hold incoming control events */
QueueHandle_t g_Fan_event_queue;

/** Tag used for ESP logging */
static const char *TAG = "Main";

/**
 * @brief   Event handler FreeRTOS task for the fan.
 * @details The initialization task sets up a FreeRTOS queue that this task
 *          reads from. This task is in charge of polling the queue for incoming
 *          events. When an event is received, it is sorted based on its source
 *          ID and then passed off to an appropriate handler in event_handlers.c
 *
 *          Note that while handling an event the task will light up the built-
 *          in LED on the ESP32.
 * @param   p [in] Required FreeRTOS parameter (UNUSED)
 */
static void Lasko_event_handler(void *p)
{
    UNUSED_PARAM(p);

    /* Enter a while(1) loop to handle things in the queue */
    while (true)
    {
        Fan_event_t event;

        if (!xQueueReceive(g_Fan_event_queue, &event, portMAX_DELAY))
        {
            /* No new events, keep waiting */
            continue;
        }

        /* Received an event! */

        Led_write_builtin(true);

        switch (event.source)
        {
            case SOURCE_HOMEKIT:
                handle_homekit(&event);
                break;
            case SOURCE_BUTTON:
                handle_button(&event);
                break;
            case SOURCE_REMOTE:
                handle_remote(&event);
                break;
            default:
                ESP_LOGW(TAG, "Unknown event source");
                break;
        }

        /* Update the front-fascia LEDs with any changes */
        Led_write_speed((g_Fan_state.on) ? g_Fan_state.speed : SPEED_OFF);

        Led_write_builtin(false);
    }
}

/**
 * @brief   Initialization FreeRTOS task for the project.
 * @details Initializes both hardware devices along with the HomeKit software
 *          library. Ends off by creating the event handler FreeRTOS task which
 *          will continue execution.
 *
 *          Note that the initialization order is intentional. The HomeKit
 *          library starts up the WiFi task which can cause crashes if a button
 *          or remote interrupt fires during startup. Thus, we startup HomeKit
 *          and give some time before initializing the button/remote.
 */
void app_main(void)
{
    /* Initialize the output hardware */
    Relay_init();
    Led_init();

    /* Initialize the HomeKit component */
    HomeKit_init();

    /* Give 10 seconds for the WiFi task to get going */
    vTaskDelay((10 * 1000) / portTICK_PERIOD_MS);

    /* Initialize the input hardware */
    Button_init();
    Remote_init();

    /* Initialize the current state of the fan */
    g_Fan_state.on = false;
    g_Fan_state.oscillate = false;
    g_Fan_state.speed = SPEED_4;

    /* Create a FreeRTOS queue, alloc'ing space for 10 events in the queue */
    g_Fan_event_queue = xQueueCreate(10, sizeof(Fan_event_t));

    /* Create our event handler task */
    (void) xTaskCreate(
        Lasko_event_handler, 
        EVENT_HANDLER_NAME, 
        EVENT_HANDLER_STACKSIZE, 
        NULL, 
        EVENT_HANDLER_PRIORITY, 
        NULL);
}

/** @} end Main */
