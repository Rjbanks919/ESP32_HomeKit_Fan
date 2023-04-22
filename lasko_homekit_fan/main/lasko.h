/**
 * @file lasko.h
 * @author Ryan Banks
 * @date 2023
 * @brief File containing declarations for interacting with the Lasko fan.
 *
 * Here's some more detailed information...
 */

#ifndef LASKO_H
#define LASKO_H

#include <stdint.h>

#include <esp_event.h>

#include <hap.h>

/**
 * @brief Mandatory identify routine for the accessory.
 * In a real accessory, something like LED blink should be implemented got
 * visual identification
 */
int
fan_identify(
    hap_acc_t *ha
    );

/*
 * An optional HomeKit Event handler which can be used to track HomeKit
 * specific events.
 */
void
fan_hap_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event,
    void *data
    );

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
    );

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
    );

#endif /* LASKO_H */
