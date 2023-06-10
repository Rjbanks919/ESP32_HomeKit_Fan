/**
 * @file homekit.c
 * @author Ryan Banks
 * @date 2023
 * @brief Component to manage software associated with HomeKit.
 *
 * The backbone of this component is the HomeKit Accessory Protocol (HAP)
 * library that is located in ESP32_HomeKit_Fan/components/homekit. The library
 * handles everything related to accessories and their setup/configuration.
 *
 * This component utilizes the HAP library in order to set up our fan accessory
 * and configure it with the proper services and characteristics. It also
 * implements a write callback which fires whenever HomeKit sends an update for
 * a characteristic.
 * 
 * @addtogroup HomeKit
 * @{
 */

#include <string.h>                /* For memory/string-related functions */

#include <esp_log.h>               /* ESP logging functions */
#include <freertos/FreeRTOS.h>     /* Basic FreeRTOS functions */
#include <freertos/queue.h>        /* Definitions for inter-task queues */

#include <hap.h>                   /* HomeKit Accessory Protocol library */
#include <hap_apple_servs.h>       /* HAP service definitions */
#include <hap_apple_chars.h>       /* HAP service characteristic definitions */

#include <app_wifi.h>              /* For setting up WiFi connection */
#include <app_hap_setup_payload.h> /* For setting up the accessory with QR */

#include "main.h"
#include "homekit.h"

/** Tag used for ESP logging */
static const char *TAG = "HomeKit";

/** Characteristics associated with our service */
static hap_char_t *on_char;
static hap_char_t *oscillate_char;
static hap_char_t *speed_char;

/**
 * @brief   Update the stored accessory state within the HAP library.
 * @details In order to avoid needing to utilize a read callback to get the
 *          current fan state, I have chosen to dynamically update the state
 *          instead. This way, whenever HomeKit gets our state no hardware or
 *          global variables need to be checked.
 */
void HomeKit_update_char(void)
{
    hap_val_t new_val;

    ESP_LOGI(TAG, "Sending updated state to HomeKit");

    /* Update the power state */
    memset(&new_val, 0, sizeof(new_val));
    new_val.b = g_Fan_state.on;
    hap_char_update_val(on_char, &new_val);

    /* Update the oscillation state */
    memset(&new_val, 0, sizeof(new_val));
    new_val.b = g_Fan_state.oscillate;
    hap_char_update_val(oscillate_char, &new_val);

    /* Update the speed state, using 25 multiplier as we only have 4 speeds */
    memset(&new_val, 0, sizeof(new_val));
    new_val.f = 25.0f * g_Fan_state.speed;
    hap_char_update_val(speed_char, &new_val);
}

/**
 * @brief   Handle an incoming write from HomeKit.
 * @details Whenever a user changes the fan's state in HomeKit, this callback
 *          will be triggered with the affected characteristics. From there, it
 *          is this function's job to decipher the updates and enqueue the
 *          appropriate event for the event handler to act upon.
 * @param   write_data [in] Buffer with updated characteristics
 * @param   count      Number of characteristics in buffer
 * @param   serv_priv  [in] Private data for the service (UNUSED)
 * @param   write_priv [in] Can be used with hap_is_req_admin() (UNUSED)
 * @retval  HAP_SUCCESS Callback was successful
 * @retval  HAP_FAIL    An error occurred while handling a characteristic
 */
static int HomeKit_write_callback(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv)
{
    UNUSED_PARAM(serv_priv);
    UNUSED_PARAM(write_priv);

    for (int i = 0; i < count; i++)
    {
        Fan_event_t       event = {.source = SOURCE_HOMEKIT};
        hap_write_data_t *write = &write_data[i];

        /* Identify what command was sent by HomeKit */
        if (0 == strcmp(HAP_CHAR_UUID_ON, hap_char_get_type_uuid(write->hc)))
        {
            /* Power state command */
            event.id = ID_POWER;
            event.arg = (uint32_t) write->val.b;

        }
        else if (0 == strcmp(HAP_CHAR_UUID_SWING_MODE, hap_char_get_type_uuid(write->hc)))
        {
            /* Oscillation mode command */
            event.id = ID_OSCILLATE;
            event.arg = (uint32_t) write->val.b;
        }
        else if (0 == strcmp(HAP_CHAR_UUID_ROTATION_SPEED, hap_char_get_type_uuid(write->hc)))
        {
            int integer_speed = (int) write->val.f;

            /* Fan speed command */
            event.id = ID_SPEED;

            /* Bin the speed for easier processing later */
            if (0 == integer_speed)
            {
                event.arg = (uint32_t) SPEED_OFF;
            }
            else if (integer_speed <= 25)
            {
                event.arg = (uint32_t) SPEED_1;
            }
            else if (integer_speed <= 50)
            {
                event.arg = (uint32_t) SPEED_2;
            }
            else if (integer_speed <= 75)
            {
                event.arg = (uint32_t) SPEED_3;
            }
            else
            {
                event.arg = (uint32_t) SPEED_4;
            }
        }
        else
        {
            /* Unknown HAP characteristic passed in */
            *(write->status) = HAP_STATUS_RES_ABSENT;
            return HAP_FAIL;
        }

        /* Update the value in the HAP internals */
        hap_char_update_val(write->hc, &(write->val));
        *(write->status) = HAP_STATUS_SUCCESS;

        /* Send the event to the main queue, wait 10 ticks if necessary */
        (void) xQueueSend(g_Fan_event_queue, &event, (TickType_t) 10);
    }

    return HAP_SUCCESS;
}

/**
 * @brief   Mandatory identify routine for the accessory.
 * @details In a real accessory, something like LED blink should be implemented 
 *          got visual identification. Currently just prints a message.
 * @param   ha [in] Accessory handle
 */
static int HomeKit_identify(hap_acc_t *ha)
{
    ESP_LOGI(TAG, "Accessory identified");
    return HAP_SUCCESS;
}

/**
 * @brief   Initializer for the HomeKit component.
 * @details Handles the setup of the HomeKit Accessory Protocol (HAP) core
 *          library. Includes setting up the accessory and the services/
 *          characteristics associated with it.
 * @note    If interrupts fire while the Wi-Fi task is starting up at the end,
 *          a crash is possible.
 */
void HomeKit_init(void)
{
    /**
     * Configure HomeKit core to make the Accessory name (and thus the WAC SSID)
     * unique, instead of the default configuration wherein only the WAC SSID is
     * made unique.
     */
    hap_cfg_t hap_cfg;
    hap_get_config(&hap_cfg);
    hap_cfg.unique_param = UNIQUE_NAME;
    hap_set_config(&hap_cfg);

    /* Initialize the HAP core */
    hap_init(HAP_TRANSPORT_WIFI);

    /* Create accessory object */
    hap_acc_cfg_t cfg = {
        .name = "HomeKit_Fan",
        .manufacturer = "Ryan_Banks",
        .model = "Lasko_18in_Fan",
        .serial_num = "1",
        .fw_rev = "0.0.0",
        .hw_rev = "0.0.0",
        .pv = "0.0.0",
        .identify_routine = HomeKit_identify,
        .cid = HAP_CID_FAN,
    };
    hap_acc_t *accessory = hap_acc_create(&cfg);

    /* Add dummy Product Data */
    uint8_t product_data[] = {'E','S','P','3','2','H','A','P'};
    hap_acc_add_product_data(accessory, product_data, sizeof(product_data));

    /* Add Wi-Fi Transport service required for HAP Spec R16 */
    hap_acc_add_wifi_transport_service(accessory, 0);

    /* Create the Fan Service. Include the "name" since this is a user visible service  */
    hap_serv_t *service = hap_serv_create(HAP_SERV_UUID_FAN);

    /* Create the characteristics of our fan service */
    on_char = hap_char_on_create(false);
    oscillate_char = hap_char_swing_mode_create(0);
    speed_char = hap_char_rotation_speed_create(0);

    /**
     * Build out the service. Give information on what the device supports and
     * the name for the service.
     *
     * Our device supports variable speed as well as swing modes.
     */
    hap_serv_add_char(service, hap_char_name_create("Lasko"));
    hap_serv_add_char(service, on_char);
    hap_serv_add_char(service, oscillate_char);
    hap_serv_add_char(service, speed_char);

    /* Set the write callback for the service */
    hap_serv_set_write_cb(service, HomeKit_write_callback);

    /* Add the Fan Service to the Accessory Object */
    hap_acc_add_serv(accessory, service);

    /* Add the Accessory to the HomeKit Database */
    hap_add_accessory(accessory);

    /**
     * For production accessories, the setup code shouldn't be programmed on to
     * the device. Instead, the setup info, derived from the setup code must
     * be used. Use the factory_nvs_gen utility to generate this data and then
     * flash it into the factory NVS partition.
     *
     * By default, the setup ID and setup info will be read from the factory_nvs
     * Flash partition and so, is not required to set here explicitly.
     *
     * However, for testing purpose, this can be overridden by using hap_set_setup_code()
     * and hap_set_setup_id() APIs, as has been done here.
     */
#ifdef CONFIG_EXAMPLE_USE_HARDCODED_SETUP_CODE
    /* Unique Setup code of the format xxx-xx-xxx. Default: 111-22-333 */
    hap_set_setup_code(CONFIG_EXAMPLE_SETUP_CODE);
    /* Unique four character Setup Id. Default: ES32 */
    hap_set_setup_id(CONFIG_EXAMPLE_SETUP_ID);
#ifdef CONFIG_APP_WIFI_USE_WAC_PROVISIONING
    app_hap_setup_payload(CONFIG_EXAMPLE_SETUP_CODE, CONFIG_EXAMPLE_SETUP_ID, true, cfg.cid);
#else
    app_hap_setup_payload(CONFIG_EXAMPLE_SETUP_CODE, CONFIG_EXAMPLE_SETUP_ID, false, cfg.cid);
#endif
#endif

    /* Enable Hardware MFi authentication (applicable only for MFi variant of SDK) */
    hap_enable_mfi_auth(HAP_MFI_AUTH_HW);

    /* Initialize Wi-Fi before startup */
    app_wifi_init();

    /* After all the initialization is done, start the HAP core task */
    hap_start();

    /* Start Wi-Fi task */
    app_wifi_start(portMAX_DELAY);

    ESP_LOGI(TAG, "HomeKit component init!");
}

/** @} end HomeKit */
