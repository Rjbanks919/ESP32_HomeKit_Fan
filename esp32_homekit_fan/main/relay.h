/**
 * @file relay.h
 * @author Ryan Banks
 * @date 2023
 * @brief Component to manage hardware associated with relays.
 * 
 * @addtogroup Relay
 * @{
 */

#ifndef RELAY_H
#define RELAY_H

#include <stdbool.h> /* Boolean type */

#include "main.h"

/** Public functions for the Relay component */
void Relay_write_speed(enum State_speed speed);
void Relay_write_oscillate(bool oscillate);
void Relay_init(void);

#endif /* RELAY_H */

/** @} end Relay */
