#ifndef SHARED_INCLUDE_RTL433_OVERRIDE_H_
#define SHARED_INCLUDE_RTL433_OVERRIDE_H_

#include "pulse_data.h"
#include "data.h"

/**
 * @brief Function pointer type for pulse_data_print_data override
 */
typedef data_t* (*pulse_data_print_func_t)(pulse_data_t const *data);

/**
 * @brief Global function pointer for pulse_data_print_data override
 * 
 * When set to non-NULL, this function will be used instead of the standard
 * pulse_data_print_data function for RabbitMQ output.
 */
extern pulse_data_print_func_t g_pulse_data_print_override;

/**
 * @brief Set the pulse_data_print_data override function
 * 
 * @param func The function to use for pulse data printing, or NULL to use standard function
 */
void rtl433_set_pulse_data_print_override(pulse_data_print_func_t func);

/**
 * @brief Get the current pulse_data_print_data override function
 * 
 * @return The current override function, or NULL if using standard function
 */
pulse_data_print_func_t rtl433_get_pulse_data_print_override(void);

#endif /* SHARED_INCLUDE_RTL433_OVERRIDE_H_ */

