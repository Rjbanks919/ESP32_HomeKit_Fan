/**
 * @file led.c
 * @author Ryan Banks
 * @date 2023
 * @brief Component to manage hardware associated with LEDs.
 *
 * For this project, there a few LEDs which are used in order to indicate the
 * current state of the fan. I chose to solder leads for the 4 speed LEDs that
 * are present on the original control board of the fan. I also utilize the
 * in-built LED present on the specific ESP32 board I am using.
 *
 * Utilizing the LEDs is rather simple affair, simple requiring logic low/high
 * writes to the LED GPIO pins. I included the ability to disable LED operation
 * entirely as well, which can be nice for dark room situations where you don't
 * want the blinding blue LEDs shining.
 * 
 * @addtogroup Led
 * @{
 */

#include <stdbool.h>     /* Boolean type */

#include <esp_log.h>     /* ESP logging functions */
#include <driver/gpio.h> /* ESP GPIO-related functions */

#include "main.h"
#include "led.h"

/** Total number of speed-related LEDs */
#define NUM_SPEED_LED 4
/** Total number of LEDs */
#define NUM_TOTAL_LED 5

/** Global boolean to track whether front-fascia LEDs are enabled/disabled */
bool g_Led_enable;

/** Tag used for ESP logging */
static const char *TAG = "Led";

/** Array to help with mass-setting GPIO pins for LEDs */
static gpio_num_t leds[] = {
    FAN_SPEED1_LED_GPIO,
    FAN_SPEED2_LED_GPIO,
    FAN_SPEED3_LED_GPIO,
    FAN_SPEED4_LED_GPIO,
    FAN_BUILTIN_LED_GPIO,
};

/**
 * @brief Enables/disables the front-fascia LEDs.
 * @param enable Whether to enable speed LEDs
 */
void Led_write_enable(bool enable) 
{
    g_Led_enable = enable;

    ESP_LOGI(TAG, "%s front-fascia LEDs", (enable) ? "Enabling" : "Disabling");

    if (!g_Led_enable)
    {
        /* If LEDs were just disabled, clear them all */
        for (int i = 0; i < NUM_SPEED_LED; i++)
        {
            gpio_set_level(leds[i], GPIO_LOW);
        }
    }

    /* If LEDs were just enabled, the event handler will turn them back on */
}

/**
 * @brief   Sets the speed LEDs according to a given speed.
 * @details Starts out by clearing all LEDs. Then, bins the speed in order to
 *          determine which led to activate.
 * @param   speed Speed to write
 */
void Led_write_speed(enum State_speed speed)
{
    if (!g_Led_enable)
    {
        /* Nothing to do! */
        return;
    }

    /* Disable all speed LEDs */
    for (int i = 0; i < NUM_SPEED_LED; i++)
    {
        gpio_set_level(leds[i], GPIO_LOW);
    }

    /* Figure out which LED to turn on */
    gpio_num_t gpio_num;
    switch (speed)
    {
        case SPEED_OFF:
            /* Simply return, don't turn any back on */
            return;
        case SPEED_1:
            gpio_num = FAN_SPEED1_LED_GPIO;
            break;
        case SPEED_2:
            gpio_num = FAN_SPEED2_LED_GPIO;
            break;
        case SPEED_3:
            gpio_num = FAN_SPEED3_LED_GPIO;
            break;
        case SPEED_4:
            gpio_num = FAN_SPEED4_LED_GPIO;
            break;
        default:
            ESP_LOGW(TAG, "Invalid speed provided, leaving all off...");
            return;
    }

    /* Turn on the LED associated with the provided speed */
    gpio_set_level(gpio_num, GPIO_HIGH);
}

/**
 * @brief Turns on/off the builtin led
 * @param state Whether to turn on/off the builtin led
 */
void Led_write_builtin(bool state)
{
    gpio_set_level(FAN_BUILTIN_LED_GPIO, (state) ? GPIO_HIGH : GPIO_LOW);
}

/**
 * @brief   Initialization function for the Led component.
 * @details Initializes the GPIO pins associated with LEDs.
 */
void Led_init(void)
{
    for (int i = 0; i < NUM_TOTAL_LED; i++)
    {
        esp_rom_gpio_pad_select_gpio(leds[i]);
        gpio_set_direction(leds[i], GPIO_MODE_OUTPUT);
        gpio_set_level(leds[i], GPIO_LOW);
    }

    /* Start with LEDs enabled */
    g_Led_enable = true;

    ESP_LOGI(TAG, "Led component init!");
}

/** @} end Led */
