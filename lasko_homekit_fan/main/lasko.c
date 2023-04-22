/**
 * @file lasko.c
 * @author Ryan Banks
 * @date 2023
 * @brief File containing functions for interacting with the Lasko fan.
 *
 * Here's some more detailed information...
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <esp_event.h>
#include <esp_log.h>

#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

#include "main.h"
#include "lasko.h"

/* A dummy callback for handling a read on the "Direction" characteristic of Fan.
 * In an actual accessory, this should read from hardware.
 * Read routines are generally not required as the value is available with th HAP core
 * when it is updated from write routines. For external triggers (like fan switched on/off
 * using physical button), accessories should explicitly call hap_char_update_val()
 * instead of waiting for a read request.
 */
int
fan_read(
    hap_char_t *hc,
    hap_status_t *status_code,
    void *serv_priv,
    void *read_priv
    )
{
    if (hap_req_get_ctrl_id(read_priv))
    {
        ESP_LOGI(g_TAG, "Received read from %s", hap_req_get_ctrl_id(read_priv));
    }
    if (!strcmp(hap_char_get_type_uuid(hc), HAP_CHAR_UUID_ROTATION_DIRECTION)) {
       /* Read the current value, toggle it and set the new value.
        * A separate variable should be used for the new value, as the hap_char_get_val()
        * API returns a const pointer
        */
        const hap_val_t *cur_val = hap_char_get_val(hc);

        hap_val_t new_val;
        if (cur_val->i == 1) {
            new_val.i = 0;
        } else {
            new_val.i = 1;
        }
        hap_char_update_val(hc, &new_val);
        *status_code = HAP_STATUS_SUCCESS;
    }
    return HAP_SUCCESS;
}

/**
 * Callback for handling the write for fan characteristics.
 *
 * @todo: Add logic to control the hardware.
 */
int
fan_write(
    hap_write_data_t write_data[],  /**< [in] Buffer with chars for the write */
    int count,                      /**< Number of chars passed in */
    void *serv_priv,                /**< [in] (UNUSED) */
    void *write_priv                /**< [in] (UNUSED) */
    )
{
    int ret = HAP_SUCCESS;
    hap_write_data_t *write;
    int i;
    int speed_value;

    if (hap_req_get_ctrl_id(write_priv))
    {
        ESP_LOGI(g_TAG, "Received write from %s", hap_req_get_ctrl_id(write_priv));
    }

    ESP_LOGI(g_TAG, "Fan Write called with %d chars", count);

    for (i = 0; i < count; i++)
    {
        write = &write_data[i];

        /**
         * Determine the type of write that just came in...
         */
        ESP_LOGI(g_TAG, "hc: 0x%x", (int) write->hc);


        if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ON))
        {
            ESP_LOGI(g_TAG, "Received Write. Fan %s", write->val.b ? "On" : "Off");
            /* TODO: Control Actual Hardware */
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
        }
        else if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ROTATION_DIRECTION))
        {
            if (write->val.i > 1)
            {
                *(write->status) = HAP_STATUS_VAL_INVALID;
                ret = HAP_FAIL;
            }
            else
            {
                ESP_LOGI(g_TAG, "Received Write. Fan %s", write->val.i ? "AntiClockwise" : "Clockwise");
                hap_char_update_val(write->hc, &(write->val));
                *(write->status) = HAP_STATUS_SUCCESS;
            }
        }
        else if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ROTATION_SPEED))
        {
            /**
             * Value comes in as a float, need to convert to an int for easier
             * handling. Note that this rounds
             */


            ESP_LOGI(g_TAG, "Got a speed change!");
            ESP_LOGI(g_TAG, "raw_val: %.6f", (float) write->val.f);


            speed_value = (int) write->val.f;

            /* Bin the speed value from 0-100 -> 1-4 */
            if (0 == speed_value)
            {
                ESP_LOGI(g_TAG, "Speed: 0 (IGNORING)");
                *(write->status) = HAP_STATUS_SUCCESS;
                continue;
            }
            else if (speed_value > 0 && speed_value <= 25)
            {
                ESP_LOGI(g_TAG, "Speed: 1");
            }
            else if (speed_value > 25 && speed_value <= 50)
            {
                ESP_LOGI(g_TAG, "Speed: 2");
            }
            else if (speed_value > 50 && speed_value <= 75)
            {
                ESP_LOGI(g_TAG, "Speed: 3");
            }
            else if (speed_value > 75 && speed_value <= 100)
            {
                ESP_LOGI(g_TAG, "Speed: 4");
            }
            else
            {
                /* Unexpected value */
                ESP_LOGI(g_TAG, "[ERROR] Unexpected speed value");
            }


            ESP_LOGI(g_TAG, "cast_val: %d", speed_value);

            *(write->status) = HAP_STATUS_SUCCESS;
        }
        else /* Unknown HAP char passed in */
        {
            ESP_LOGI(g_TAG, "Received Write. Fan %s", write->val.i ? "AntiClockwise" : "Clockwise");
            *(write->status) = HAP_STATUS_RES_ABSENT;
        }
    }

    return ret;
}

/**
 * @brief Event handler for HomeKit events.
 *
 * Tracks HomeKit-specific events to inform the developer of activity during
 * setup and pairing.
 */
void
fan_hap_event_handler(
    void* arg,                      /**< [in] (UNUSED) Event handler arg */
    esp_event_base_t event_base,    /**< Base ID of the event */
    int32_t event,                  /**< ID of the event */
    void *data                      /**< Data passed for the event */
    )
{
    switch(event)
    {
        case HAP_EVENT_PAIRING_STARTED :
            ESP_LOGI(g_TAG, "Pairing Started");
            break;
        case HAP_EVENT_PAIRING_ABORTED :
            ESP_LOGI(g_TAG, "Pairing Aborted");
            break;
        case HAP_EVENT_CTRL_PAIRED :
            ESP_LOGI(g_TAG, "Controller %s Paired. Controller count: %d",
                        (char *)data, hap_get_paired_controller_count());
            break;
        case HAP_EVENT_CTRL_UNPAIRED :
            ESP_LOGI(g_TAG, "Controller %s Removed. Controller count: %d",
                        (char *)data, hap_get_paired_controller_count());
            break;
        case HAP_EVENT_CTRL_CONNECTED :
            ESP_LOGI(g_TAG, "Controller %s Connected", (char *)data);
            break;
        case HAP_EVENT_CTRL_DISCONNECTED :
            ESP_LOGI(g_TAG, "Controller %s Disconnected", (char *)data);
            break;
        case HAP_EVENT_ACC_REBOOTING : {
            char *reason = (char *)data;
            ESP_LOGI(g_TAG, "Accessory Rebooting (Reason: %s)",  reason ? reason : "null");
            break;
        case HAP_EVENT_PAIRING_MODE_TIMED_OUT :
            ESP_LOGI(g_TAG, "Pairing Mode timed out. Please reboot the device.");
        }
        default:
            /* Silently ignore unknown events */
            break;
    }
}

/**
 * @brief Mandatory identify routine for the accessory.
 *
 * In a real accessory, something like LED blink should be implemented got
 * visual identification. Currently just prints a message.
 */
int
fan_identify(
    hap_acc_t *ha   /**< [in] (UNUSED) Accessory handle */
    )
{
    ESP_LOGI(g_TAG, "Accessory identified");

    return HAP_SUCCESS;
}
