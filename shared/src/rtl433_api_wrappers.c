/**
 * @file rtl433_api_wrappers.c
 * @brief Extended wrapper functions for rtl_433 API with RabbitMQ integration
 * 
 * This file contains wrapper functions that extend the original rtl_433 API
 * functions with our custom functionality (msg_id management, RabbitMQ sending)
 * without modifying the original source code.
 */

#include "rtl433_api_wrappers.h"
#include "rtl433_signal_format.h"
#include "r_api.h"
#include "data.h"
#include "decoder.h"
#include <stdio.h>

// Simplified approach: time-based correlation instead of complex msg_id tracking

/**
 * Extended wrapper for run_ook_demods with RabbitMQ integration
 * 
 * This function:
 * 1. Increments msg_id before processing
 * 2. Calls original run_ook_demods function
 * 3. Sends raw signal data to RabbitMQ after processing
 * 
 * @param r_devs List of registered devices for OOK demodulation
 * @param pulse_data Pulse data to be processed
 * @return Number of events detected (same as original run_ook_demods)
 */
int run_ook_demods_ex(list_t *r_devs, pulse_data_t *pulse_data)
{
    if (!r_devs || !pulse_data) {
        return 0;
    }
    
    // Note: Using time-based correlation instead of complex msg_id tracking
    // Just set current pulse_data for decoder access if needed
    rtl433_set_current_pulse_data(pulse_data);
    
    // CALL ORIGINAL: Process with original rtl_433 function
    int result = run_ook_demods(r_devs, pulse_data);
    
    // AFTER: Send raw signal data to RabbitMQ if configured
    rtl433_send_pulse_data_to_rabbitmq(pulse_data);
    
    // Simplified: just return result
    
    return result;
}

/**
 * Extended wrapper for run_fsk_demods with RabbitMQ integration
 * 
 * This function:
 * 1. Increments msg_id before processing
 * 2. Calls original run_fsk_demods function  
 * 3. Sends raw signal data to RabbitMQ after processing
 * 
 * @param r_devs List of registered devices for FSK demodulation
 * @param fsk_pulse_data FSK pulse data to be processed
 * @return Number of events detected (same as original run_fsk_demods)
 */
int run_fsk_demods_ex(list_t *r_devs, pulse_data_t *fsk_pulse_data)
{
    if (!r_devs || !fsk_pulse_data) {
        return 0;
    }
    
    // Note: Using time-based correlation instead of complex msg_id tracking
    // Just set current pulse_data for decoder access if needed
    rtl433_set_current_pulse_data(fsk_pulse_data);
    
    // CALL ORIGINAL: Process with original rtl_433 function
    int result = run_fsk_demods(r_devs, fsk_pulse_data);
    
    // AFTER: Send raw signal data to RabbitMQ if configured
    rtl433_send_pulse_data_to_rabbitmq(fsk_pulse_data);
    
    // Simplified: just return result
    
    return result;
}

/**
 * Simplified device preparation - no longer needed for time-based correlation
 * 
 * @param r_devs List of devices (kept for API compatibility)
 */
void rtl433_prepare_devices_for_msg_id(list_t *r_devs)
{
    // No longer needed: using time-based correlation instead of msg_id patching
    (void)r_devs; // Suppress unused parameter warning
}
