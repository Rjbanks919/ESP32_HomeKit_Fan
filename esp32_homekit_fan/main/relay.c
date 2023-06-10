/**
 * @file relay.c
 * @author Ryan Banks
 * @date 2023
 * @brief Component to manage hardware associated with relays.
 *
 * For this project, I utilize relays in order to handle AC power lines that go
 * to the fan's two motors (oscillation/main-fan). The main fan motor has four
 * power lines that go to it. The fan blade will spin at a different speed based
 * upon which of these power lines has voltage running through it. In order to
 * handle this, I use a 4-relay module. I have an additional relay module that
 * handles the oscillation motor switching.
 *
 * All the relays are configured as active-low, meaning that when they receive a
 * logic low signal they let the electrons flow!
 * 
 * @addtogroup Relay
 * @{
 */

#include <stdbool.h>     /* Boolean type */

#include <esp_log.h>     /* ESP logging functions */
#include <driver/gpio.h> /* ESP GPIO-related functions */

#include "main.h"
#include "relay.h"

/** Total number of speed-related relays */
#define NUM_SPEED_RELAY 4
/** Total number of relays */
#define NUM_TOTAL_RELAY 5

/** Tag used for ESP logging */
static const char *TAG = "Relay";

/** Array to help with mass-setting GPIO pins for speed relays */
static gpio_num_t relays[] = {
    FAN_SPEED1_RELAY_GPIO,
    FAN_SPEED2_RELAY_GPIO,
    FAN_SPEED3_RELAY_GPIO,
    FAN_SPEED4_RELAY_GPIO,
    FAN_OSC_RELAY_GPIO
};

/**
 * @brief   Sets the speed relays according to a given speed.
 * @details Starts out by clearing all relays so the fan turns off. Then, bins
 *          the speed in order to determine which relay to activate.
 * @param   speed Speed to write.
 */
void Relay_write_speed(enum State_speed speed)
{
    if (g_Fan_state.speed == speed && true == g_Fan_state.on)
    {
        /* Nothing to do! */
        return;
    }

    ESP_LOGI(TAG, "Relay writing speed: %d", speed);
    
    /* Disable all speed relays */
    for (int i = 0; i < NUM_SPEED_RELAY; i++)
    {
        gpio_set_level(relays[i], GPIO_HIGH);
    }

    /* Figure out which relay to turn on */
    gpio_num_t gpio_num;
    switch (speed)
    {
        case SPEED_OFF:
            /* Simply return, don't turn any back on */
            return;
        case SPEED_1:
            gpio_num = FAN_SPEED1_RELAY_GPIO;
            break;
        case SPEED_2:
            gpio_num = FAN_SPEED2_RELAY_GPIO;
            break;
        case SPEED_3:
            gpio_num = FAN_SPEED3_RELAY_GPIO;
            break;
        case SPEED_4:
            gpio_num = FAN_SPEED4_RELAY_GPIO;
            break;
        default:
            ESP_LOGW(TAG, "Invalid speed provided, leaving all off...");
            return;
    }
    
    /* Turn on the relay associated with the provided speed */
    gpio_set_level(gpio_num, GPIO_LOW);
}

/**
 * @brief Sets the oscillation relay according to a given bool.
 * @param oscillate Whether to enable oscillation
 */
void Relay_write_oscillate(bool oscillate)
{
    gpio_set_level(FAN_OSC_RELAY_GPIO, (oscillate) ? GPIO_LOW : GPIO_HIGH);
}

/**
 * @brief   Initializer for the Relay component.
 * @details Initializes the GPIO pins associated with relays. Note that all the
 *          relays are active-low, so we start them all out at GPIO_HIGH.
 */
void Relay_init(void)
{
    for (int i = 0; i < NUM_TOTAL_RELAY; i++)
    {
        esp_rom_gpio_pad_select_gpio(relays[i]);
        gpio_set_direction(relays[i], GPIO_MODE_OUTPUT);
        gpio_set_level(relays[i], GPIO_HIGH);
    }

    ESP_LOGI(TAG, "Relay component init!");
}

/** @} end Relay */
