/**
 * @file led.h
 * @author Ryan Banks
 * @date 2023
 * @brief Component to manage hardware associated with LEDs.
 * 
 * @addtogroup Led
 * @{
 */

#ifndef LED_H
#define LED_H

#include <stdbool.h> /* Boolean type */

#include "main.h"

/** Boolean to track whether front-fascia LEDs are enabled/disabled */
extern bool g_Led_enable;

/** Public functions for the LED component */
void Led_write_enable(bool enable);
void Led_write_speed(enum State_speed speed);
void Led_write_builtin(bool state);
void Led_init(void);

#endif /* LED_H */

/** @} end Led */
