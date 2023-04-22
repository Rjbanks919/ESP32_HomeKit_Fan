/**
 * @file helpers.c
 * @author Ryan Banks
 * @date 2023
 * @brief File containing helper functions for the project.
 *
 * Here's some more detailed information...
 */

#include <hap.h>

#include <iot_button.h>

/* Reset network credentials if button is pressed for more than 3 seconds and then released */
#define RESET_NETWORK_BUTTON_TIMEOUT        3

/* Reset to factory if button is pressed and held for more than 10 seconds */
#define RESET_TO_FACTORY_BUTTON_TIMEOUT     10

/**
 * @brief The network reset button callback handler.
 * Useful for testing the Wi-Fi re-configuration feature of WAC2
 */
static void
reset_network_handler(
    void* arg           /**< [in] (UNUSED) */
    )
{
    hap_reset_network();
}

/**
 * @brief The factory reset button callback handler.
 */
static void
reset_to_factory_handler(
    void* arg           /**< [in] (UNUSED) */
    )
{
    hap_reset_to_factory();
}

/**
 * @brief The Reset button  GPIO initialization function.
 * Same button will be used for resetting Wi-Fi network as well as for reset to
 * factory based on the time for which the button is pressed.
 */
void
reset_key_init(
    uint32_t key_gpio_pin   /**< GPIO pin to initialize */
    )
{
    button_handle_t handle = iot_button_create(key_gpio_pin, BUTTON_ACTIVE_LOW);

    iot_button_add_on_release_cb(handle, RESET_NETWORK_BUTTON_TIMEOUT, reset_network_handler, NULL);
    iot_button_add_on_press_cb(handle, RESET_TO_FACTORY_BUTTON_TIMEOUT, reset_to_factory_handler, NULL);
}
