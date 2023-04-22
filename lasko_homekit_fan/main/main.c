/**
 * @file main.c
 * @author Ryan Banks
 * @date 2023
 * @brief File containing the main RTOS tasks for the Lasko Homekit control.
 *
 * Utilizes an Espressif Homekit Accessory Protocol library to control a dumb
 * Lasko fan via Homekit!
 */

#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_event.h>
#include <esp_log.h>
#include "driver/gpio.h"

#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
#include <hap_fw_upgrade.h>

#include <app_wifi.h>
#include <app_hap_setup_payload.h>

#include "main.h"
#include "helpers.h"
#include "lasko.h"

/* The button on the feather will be used as the Reset button */
#define RESET_GPIO  GPIO_NUM_0

/** Priority for the Lasko RTOS task */
#define LASKO_TASK_PRIORITY  1
#define LASKO_TASK_STACKSIZE 4 * 1024
#define LASKO_TASK_NAME "hap_lasko"

/* Required for server verification during OTA, PEM format as string */
char server_cert[] = {};

/** Global tag used for ESP logging */
const char *g_TAG = "HAP Lasko";

/**
 * @brief Main thread for handling the Lasko fan.
 *
 * Handles setup of the fan using HAP.
 */
static void
lasko_thread_entry(
    void *p         /** [in] (UNUSED) To adhere to RTOS rules */
    )
{
    hap_acc_t *accessory;
    hap_serv_t *service;
    hap_cfg_t hap_cfg;
    hap_fw_upgrade_config_t ota_config = {.server_cert_pem = server_cert,};
    uint8_t product_data[] = {'E','S','P','3','2','H','A','P'};

    /* Details that will show up in Home after adding the accessory */
    hap_acc_cfg_t cfg = {
        .name = "Lasko-Fan",
        .manufacturer = "Lasko",
        .model = "EliteCollection18",
        .serial_num = "69",
        .fw_rev = "1.0.0",
        .hw_rev = "1.0.0",
        .pv = "1.1.0",
        .identify_routine = fan_identify,
        .cid = HAP_CID_FAN,
    };

    /**
     * Configure HomeKit core to make the Accessory name (and thus the WAC SSID)
     * unique, instead of the default configuration wherein only the WAC SSID is
     * made unique.
     */
    hap_get_config(&hap_cfg);
    hap_cfg.unique_param = UNIQUE_NAME;
    hap_set_config(&hap_cfg);

    /* Initialize the HAP core */
    hap_init(HAP_TRANSPORT_WIFI);

    /* Create accessory object */
    accessory = hap_acc_create(&cfg);

    /* Add dummy Product Data */
    hap_acc_add_product_data(accessory, product_data, sizeof(product_data));

    /* Add Wi-Fi Transport service required for HAP Spec R16 */
    hap_acc_add_wifi_transport_service(accessory, 0);


    /* Create the Fan Service. Include the "name" since this is a user visible service  */
    service = hap_serv_fan_create(false);

    /**
     * Build out the service. Give information on what the device supports and
     * the name for the service.
     *
     * Our device supports variable speed as well as swing modes.
     */
    hap_serv_add_char(service, hap_char_name_create("Lasko"));
    hap_serv_add_char(service, hap_char_swing_mode_create(0));
    hap_serv_add_char(service, hap_char_rotation_speed_create(0));

    /* Set the write/read callbacks for the service */
    hap_serv_set_write_cb(service, fan_write);
    hap_serv_set_read_cb(service, fan_read);

    /* Add the Fan Service to the Accessory Object */
    hap_acc_add_serv(accessory, service);


    /* Create the Firmware Upgrade HomeKit Custom Service.
     * Please refer the FW Upgrade documentation under components/homekit/extras/include/hap_fw_upgrade.h
     * and the top level README for more information.
     */
    service = hap_serv_fw_upgrade_create(&ota_config);

    /* Add the service to the Accessory Object */
    hap_acc_add_serv(accessory, service);

    /* Add the Accessory to the HomeKit Database */
    hap_add_accessory(accessory);

    /* Register a button to reset Wi-Fi network and reset to factory */
    reset_key_init(RESET_GPIO);

    /* Query the controller count (just for information) */
    ESP_LOGI(g_TAG, "Accessory is paired with %d controllers",
                hap_get_paired_controller_count());




    /** @todo Do the actual hardware initialization here */







    /* For production accessories, the setup code shouldn't be programmed on to
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

    /* Initialize Wi-Fi */
    app_wifi_init();

    /* Register an event handler for HomeKit specific events.
     * All event handlers should be registered only after app_wifi_init()
     */
    esp_event_handler_register(HAP_EVENT, ESP_EVENT_ANY_ID, &fan_hap_event_handler, NULL);

    /* After all the initialization is done, start the HAP core */
    hap_start();

    /* Start Wi-Fi */
    app_wifi_start(portMAX_DELAY);

    /* The task ends here. The read/write callbacks will be invoked by the HAP Framework */
    vTaskDelete(NULL);
}

/**
 * Required RTOS main app function. Launches the task that will handle the fan
 * accessory.
 */
void
app_main(void)
{
    /* Create the Lasko task */
    xTaskCreate(lasko_thread_entry, LASKO_TASK_NAME, LASKO_TASK_STACKSIZE, NULL, LASKO_TASK_PRIORITY, NULL);
}