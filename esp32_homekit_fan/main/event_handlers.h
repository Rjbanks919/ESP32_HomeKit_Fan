/**
 * @file event_handlers.h
 * @author Ryan Banks
 * @date 2023
 * @brief "Component" to handle incoming events from different sources.
 * 
 * @addtogroup EventHandlers
 * @{
 */

#ifndef EVENT_HANDLERS_H
#define EVENT_HANDLERS_H

#include "main.h" /* Fan_event_t */

void handle_homekit(Fan_event_t *event);
void handle_button(Fan_event_t *event);
void handle_remote(Fan_event_t *event);

#endif /* EVENT_HANDLERS_H */

/** @} end EventHandlers */
