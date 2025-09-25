#ifndef RTL433_API_WRAPPERS_H
#define RTL433_API_WRAPPERS_H

#include "r_api.h"
#include "pulse_data.h"
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file rtl433_api_wrappers.h
 * @brief Wrapper functions for rtl_433 API with our custom extensions
 * 
 * These wrapper functions allow us to extend the functionality of original
 * rtl_433 API functions without modifying the original source code.
 */

/**
 * Extended wrapper for run_ook_demods with RabbitMQ integration
 * 
 * @param r_devs List of registered devices for OOK demodulation
 * @param pulse_data Pulse data to be processed
 * @return Number of events detected (same as original run_ook_demods)
 */
int run_ook_demods_ex(list_t *r_devs, pulse_data_t *pulse_data);

/**
 * Extended wrapper for run_fsk_demods with RabbitMQ integration
 * 
 * @param r_devs List of registered devices for FSK demodulation  
 * @param fsk_pulse_data FSK pulse data to be processed
 * @return Number of events detected (same as original run_fsk_demods)
 */
int run_fsk_demods_ex(list_t *r_devs, pulse_data_t *fsk_pulse_data);


#ifdef __cplusplus
}
#endif

#endif // RTL433_API_WRAPPERS_H
