#ifndef RTL433_RECONSTRUCT_H
#define RTL433_RECONSTRUCT_H

#include "pulse_data.h"
#include "data.h"
#include <json-c/json.h>

/**
 * Signal reconstruction from JSON data
 */

/**
 * Reconstruct pulse_data_t from JSON object
 * @param json_obj JSON object containing signal data
 * @return reconstructed pulse_data_t or NULL on error
 */
pulse_data_t* rtl433_reconstruct_from_json_obj(json_object *json_obj);

/**
 * Reconstruct pulse_data_t from JSON string
 * @param json_str JSON string containing signal data
 * @return reconstructed pulse_data_t or NULL on error
 */
pulse_data_t* rtl433_reconstruct_from_json_string(const char *json_str);

/**
 * Reconstruct pulse_data_t from hex_string
 * @param hex_str hex_string from enhanced JSON
 * @param freq_hz center frequency
 * @param rate_hz sample rate
 * @return reconstructed pulse_data_t or NULL on error
 */
pulse_data_t* rtl433_reconstruct_from_hex_string(const char *hex_str, unsigned freq_hz, unsigned rate_hz);

/**
 * Free reconstructed pulse_data_t
 * @param data pulse_data_t to free
 */
void rtl433_reconstruct_free(pulse_data_t *data);

/**
 * Validate reconstructed signal data
 * @param data pulse_data_t to validate
 * @return 1 if valid, 0 if invalid
 */
int rtl433_reconstruct_validate(pulse_data_t const *data);

#endif // RTL433_RECONSTRUCT_H

