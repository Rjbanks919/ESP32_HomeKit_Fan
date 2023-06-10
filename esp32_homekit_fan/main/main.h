/**
 * @file main.h
 * @author Ryan Banks
 * @date 2023
 * @brief Initialization and main event handler tasks.
 *
 * @addtogroup Main
 * @{
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>            /* Extended integer types */

#include <freertos/FreeRTOS.h> /* Basic FreeRTOS functions */
#include <freertos/queue.h>    /* Definitions for creating inter-task queues */
#include <hal/gpio_types.h>    /* ESP GPIO-related functions */

/** GPIO logic level low/high */
#define GPIO_LOW  0
#define GPIO_HIGH 1

/** For dealing with unused parameters */
#define UNUSED_PARAM(param) (void) param;

/**
 * GPIO table for the project
 * ``````````````````````````
 * GPIO00 -> (IN)       IR      SENSOR
 * GPIO01 -> (IN)       Power   BUTTON
 * GPIO02 -> (IN)       Oscil   BUTTON
 * GPIO03 -> (OUT)      Speed 1 RELAY
 * GPIO04 -> (OUT)      Speed 2 RELAY
 * GPIO05 -> (OUT)      Speed 3 RELAY
 * GPIO06 -> (OUT)      Speed 4 RELAY
 * GPIO07 -> (OUT)      Oscil   RELAY
 * GPIO08 -> (OUT)      Speed 1 LED
 * GPIO09 -> (OUT)      Speed 2 LED
 * GPIO10 -> (OUT)      Speed 3 LED
 * GPIO11 -> (OUT)      Speed 4 LED
 * GPIO12 -> (OUT)      Built-in LED 1
 * GPIO13 -> (OUT)      Built-in LED 2
 * GPIO14 -> (UNUSED)
 * GPIO15 -> (UNUSED)
 * GPIO16 -> (UNUSED)
 * GPIO17 -> (UNUSED)
 * GPIO18 -> (DONT USE) USB-JTAG 
 * GPIO19 -> (DONT USE) USB-JTAG
 * GPIO20 -> ?
 * GPIO21 -> ?
 */
#define FAN_IR_SENSOR_GPIO    GPIO_NUM_0  /** IR sensor */
#define FAN_PWR_BUTTON_GPIO   GPIO_NUM_1  /** Power button */
#define FAN_OSC_BUTTON_GPIO   GPIO_NUM_2  /** Oscillation button */
#define FAN_SPEED1_RELAY_GPIO GPIO_NUM_3  /** Speed 1 relay */
#define FAN_SPEED2_RELAY_GPIO GPIO_NUM_4  /** Speed 2 relay */
#define FAN_SPEED3_RELAY_GPIO GPIO_NUM_5  /** Speed 3 relay */
#define FAN_SPEED4_RELAY_GPIO GPIO_NUM_6  /** Speed 4 relay */
#define FAN_OSC_RELAY_GPIO    GPIO_NUM_7  /** Oscillation relay */
#define FAN_SPEED1_LED_GPIO   GPIO_NUM_8  /** Speed 1 LED */
#define FAN_SPEED2_LED_GPIO   GPIO_NUM_9  /** Speed 2 LED */
#define FAN_SPEED3_LED_GPIO   GPIO_NUM_10 /** Speed 3 LED */
#define FAN_SPEED4_LED_GPIO   GPIO_NUM_13 /** Speed 4 LED */
#define FAN_BUILTIN_LED_GPIO  GPIO_NUM_12 /** Built-in LED */

/** Enum for sources of incoming events */
enum Event_source
{
    SOURCE_HOMEKIT = 0, /** Event originated from HomeKit */
    SOURCE_REMOTE,      /** Event originated from IR remote */
    SOURCE_BUTTON       /** Event originated from front-fascia button */
};

/** Enum for identifying incoming events */
enum Event_id
{
    ID_POWER       = 0, /** Power state changed */
    ID_OSCILLATE,       /** Oscillation state changed */
    ID_TIME,            /** IR remote clock button pressed */
    ID_SPEED,           /** Fan speed changed */
    ID_TEMPERATURE      /** IR remote temperature button pressed */
};

/** Struct for incoming control events */
typedef struct Fan_event_t
{
    enum Event_source source;   /** Source of the event */
    enum Event_id id;           /** ID of the event */
    uint32_t arg;               /** Argument for the event */
} Fan_event_t;

/** Enum for distinct speed levels for fan */
enum State_speed
{
    SPEED_OFF = 0, /** Fan is off */
    SPEED_1,       /** Lowest speed */
    SPEED_2,       /** Second-lowest speed */
    SPEED_3,       /** Second-highest speed */
    SPEED_4,       /** Highest speed */
    NUM_SPEED      /** For arithmetic */
};

/** Struct to represent current state of the fan */
typedef struct Fan_state_t
{
    bool on;                /** Whether the fan is spinning */
    bool oscillate;         /** Whether oscillation is enabled */
    enum State_speed speed; /** Current speed level */
} Fan_state_t;

/** Current state of the fan */
extern Fan_state_t g_Fan_state;

/** Queue to hold incoming control events */
extern QueueHandle_t g_Fan_event_queue;

#endif /* MAIN_H */

/** @} end Main */
