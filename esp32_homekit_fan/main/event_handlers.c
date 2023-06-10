/**
 * @file event_handlers.c
 * @author Ryan Banks
 * @date 2023
 * @brief "Component" to handle incoming events from different sources.
 *
 * We expect events to come from three distinct sources: HomeKit, IR remote
 * interrupts, and front-fascia button interrupts. Each of these operate
 * slightly differently, and thus require separate handling.
 *
 * Each of these handlers will make appropriate alterations to the hardware and
 * internal state. Because the IR remote and button events occur only on the
 * hardware, they have to update the HomeKit state after finishing such that a
 * user can see up-to-date status changes in the Apple Home app.
 * 
 * @addtogroup EventHandlers
 * @{
 */

#include <stdbool.h> /* Boolean type */

#include <esp_log.h> /* ESP logging functions */

#include "main.h"
#include "led.h"
#include "relay.h"
#include "homekit.h"

/** Tag used for ESP logging */
static const char *TAG = "EventHandlers";

/**
 * @brief   Handler for HomeKit-sourced events.
 * @details This function gets called whenever there is a new event with the
 *          HomeKit source type. You'll note that the handling here is a bit
 *          odd, and that is due to how HomeKit handles power on/speed change
 *          events.
 * 
 *          It'll often send power on and speed change events together, in a
 *          semi-random order (race condition?). Thus, I've added some logic to
 *          handle these scenarios. 
 * @param   event [in] HomeKit event to handle
 */
void handle_homekit(Fan_event_t *event)
{
    switch (event->id)
    {
        case ID_POWER:
            if (g_Fan_state.on == (bool) event->arg)
            {
                /* Ignore redundant power events (ON->ON or OFF->OFF) */
                break;
            }

            /* If we are turning on, utilize the stored speed state. If we are
             * turning off, simply use the off speed.
             */
            Relay_write_speed((event->arg) ? g_Fan_state.speed : SPEED_OFF);
            
            /* If we are turning on, utilize the stored oscillation state. If we
             * are turning off, simply use false.
             */
            Relay_write_oscillate((event->arg) ? g_Fan_state.oscillate : false);

            /* Update the global state and finish */
            g_Fan_state.on = (bool) event->arg;
            break;

        case ID_OSCILLATE:
            if (g_Fan_state.on)
            {
                /* If we are on, immediately apply the new state */
                Relay_write_oscillate((bool) event->arg);
            }

            /* If we were off, the new state will be applied the next time the
             * fan powers on.
             */

            /* Update the global state and finish */
            g_Fan_state.oscillate = (bool) event->arg;
            break;

        case ID_SPEED:
            if (SPEED_OFF == (enum State_speed) event->arg)
            {
                /* If we are being turned off, leave handling to the power event
                 * that will come before/after this event.
                 */
                break;
            }

            if (g_Fan_state.on)
            {
                /* If we are on, immediately apply the new state */
                Relay_write_speed((enum State_speed) event->arg);
            }

            /* If we were off, the new state will be applied the next time the
             * fan powers on.
             */
            
            /* Update the global state and finish */
            g_Fan_state.speed = (enum State_speed) event->arg;

            break;

        default:
            ESP_LOGW(TAG, "Unhandled event ID");
            break;
    }
}

/**
 * @brief   Handler for Button-sourced events.
 * @details This function gets called whenever there is a new event with the
 *          Button source type. Handling for the front-fascia buttons is a bit
 *          different as the power button will cycle through speeds. The rest is
 *          fairly simple.
 * @param   event [in] Button event to handle
 */
void handle_button(Fan_event_t *event)
{
    switch (event->id)
    {
        case ID_POWER:
            /**
             * Because the power button cycles through speeds / on-off, we need
             * to calculate the speed to switch to. This is a bit funky as we
             * never want to set our current speed to off. Thus, we do fancy
             * math to ensure the following state changes:
             *
             * (fan off) SPEED_4 -> (fan on)  SPEED_4
             * (fan on)  SPEED_4 ->           SPEED_3
             *           SPEED_3 ->           SPEED_2
             *           SPEED_2 ->           SPEED_1
             *           SPEED_1 -> (fan off) SPEED_4
             */
            enum State_speed next_speed = (((g_Fan_state.speed) - 1) % NUM_SPEED) + !g_Fan_state.on;

            /* Update the speed relays */
            Relay_write_speed(next_speed);

            /* Update the oscillation relay */
            Relay_write_oscillate((SPEED_OFF != next_speed) ? g_Fan_state.oscillate : false);

            /* Update the global state and finish */
            g_Fan_state.speed = (SPEED_OFF != next_speed) ? next_speed : SPEED_4;
            g_Fan_state.on = (SPEED_OFF != next_speed);
            break;

        case ID_OSCILLATE:
            if (g_Fan_state.on)
            {
                /* If we are on, immediately apply the new state */
                Relay_write_oscillate(!g_Fan_state.oscillate);
            }

            /* If we were off, the new state will be applied the next time the
             * fan powers on.
             */

            /* Update the global state and finish */
            g_Fan_state.oscillate = !g_Fan_state.oscillate;
            break;

        default:
            ESP_LOGW(TAG, "Unhandled event ID");
            break;
    }

    /* Event triggered by hardware, report hardware changes to HomeKit */
    HomeKit_update_char();
}

/**
 * @brief   Handler for Remote-sourced events.
 * @details This function gets called whenever there is a new event with the
 *          Remote source type. Handling here differs in that the remote
 *          includes 5 different events that can be triggered. Implementation
 *          for even shared events like power/oscillation/speed differ a bit,
 *          which is why this exists.
 * @param   event [in] Remote event to handle
 */
void handle_remote(Fan_event_t *event)
{
    switch (event->id)
    {
        case ID_POWER:
            /* Either turn off the fan or set it to the previous speed */
            Relay_write_speed((g_Fan_state.on) ? SPEED_OFF : g_Fan_state.speed);

            /* Either turn off oscillation or set it to the previous state */
            Relay_write_oscillate((g_Fan_state.on) ? false : g_Fan_state.oscillate);

            /* Update the global state and finish */
            g_Fan_state.on = !g_Fan_state.on;
            break;

        case ID_OSCILLATE:
            if (g_Fan_state.on)
            {
                /* If we are on, immediately apply the new state */
                Relay_write_oscillate(!g_Fan_state.oscillate);
            }

            /* Update the global state and finish */
            g_Fan_state.oscillate = !g_Fan_state.oscillate;
            break;

        case ID_SPEED:
            if (!g_Fan_state.on)
            {
                /* We only accept speed events when the fan is on */
                break;
            }

            /**
             * More fun math!
             *
             * The goal of this calculation is to get the next lowest speed with
             * wraparound back to SPEED_4. I ran into a problem where negative
             * values weren't properly transitioning back into the enum, which I
             * solved by adding SPEED_4, which is cancelled out by the modulus.
             *
             * Also note that there is a -1/+1 pair to shift the enum to start
             * at 0. This is to ignore the SPEED_OFF value. To summarize:
             *
             * SPEED_4 -> SPEED_3
             * SPEED_3 -> SPEED_2
             * SPEED_2 -> SPEED_1
             * SPEED_1 -> SPEED_4
             */
            enum State_speed next_speed = ((((int) g_Fan_state.speed - 1) - 1 + SPEED_4) % SPEED_4) + 1;
            Relay_write_speed(next_speed);

            /* Update the global state and finish */
            g_Fan_state.speed = next_speed;
            break;

        case ID_TIME:
        case ID_TEMPERATURE:
            /**
             * No current use for these buttons, so using them to enable and
             * disable the front-fascia LEDs.
             */
            Led_write_enable(!g_Led_enable);
            break;

        default:
            ESP_LOGW(TAG, "Unhandled event ID");
            break;
    }

    /* Update HomeKit with whatever happened */
    HomeKit_update_char();
}

/** @} end EventHandlers */
