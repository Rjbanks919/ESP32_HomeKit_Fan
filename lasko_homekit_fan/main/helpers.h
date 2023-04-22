/**
 * @file helpers.h
 * @author Ryan Banks
 * @date 2023
 * @brief File containing helper declarations for the project.
 *
 * Here's some more detailed information...
 */

#ifndef HELPERS_H
#define HELPERS_H

#include <stdint.h>

/**
 * @brief The Reset button GPIO initialization function.
 *
 * The button will be used for resetting Wi-Fi network as well as for resetting
 * to factory settings. This is based on the time for which the button is
 * pressed.
 */
void
reset_key_init(
    uint32_t key_gpio_pin   /**< GPIO pin to use for reset */
);

#endif /* HELPERS_H */
