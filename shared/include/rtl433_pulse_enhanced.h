#ifndef SHARED_INCLUDE_RTL433_PULSE_ENHANCED_H_
#define SHARED_INCLUDE_RTL433_PULSE_ENHANCED_H_

#include "pulse_data.h"
#include "data.h"

/**
 * @brief Enhanced version of pulse_data_print_data with hex_string generation.
 * 
 * This function creates a data_t structure from pulse_data_t with additional
 * hex_string and triq_url fields for complete signal reconstruction.
 * 
 * @param data The pulse_data_t structure to convert
 * @return A data_t structure with enhanced fields, or NULL on error
 *         The caller is responsible for freeing the returned data_t with data_free()
 */
data_t *rtl433_pulse_data_print_data_enhanced(pulse_data_t const *data);

/**
 * @brief Converts pulse_data_t to an enhanced JSON string with hex_string.
 * 
 * @param data The pulse_data_t structure to convert
 * @return A dynamically allocated JSON string, or NULL on error
 *         The caller is responsible for freeing the returned string
 */
char* rtl433_pulse_data_to_enhanced_json_string(pulse_data_t const *data);

#endif /* SHARED_INCLUDE_RTL433_PULSE_ENHANCED_H_ */

