/** @file
 * RFRAW conversion and signal reconstruction utilities for rtl_433
 * 
 * This module handles:
 * - Converting pulse_data_t to triq.org hex strings
 * - Enhanced JSON serialization with reconstruction data
 * - Signal reconstruction from hex strings
 */

#ifndef RTL433_RFRAW_H
#define RTL433_RFRAW_H

#include "pulse_data.h"
#include "data.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Generate triq.org compatible hex string from pulse data
/// @param data Pulse data structure
/// @return Allocated hex string (caller must free) or NULL on error
char* rtl433_rfraw_generate_hex_string(pulse_data_t const *data);

/// Generate triq.org URL from pulse data
/// @param data Pulse data structure  
/// @return Allocated URL string (caller must free) or NULL on error
char* rtl433_rfraw_generate_triq_url(pulse_data_t const *data);

/// Enhanced pulse data to JSON conversion (includes reconstruction data)
/// @param data Pulse data structure
/// @return Allocated JSON string (caller must free) or NULL on error
char* rtl433_rfraw_pulse_data_to_enhanced_json(pulse_data_t const *data);

/// Parse hex string back to pulse data (for reconstruction)
/// @param hex_string Hex string from triq.org format
/// @param pulse_data Output pulse data structure
/// @return 0 on success, negative on error
int rtl433_rfraw_parse_hex_string(const char *hex_string, pulse_data_t *pulse_data);

/// Reconstruct pulse data from JSON with hex string
/// @param json_str JSON string containing hex_string field
/// @param pulse_data Output pulse data structure
/// @return 0 on success, negative on error
int rtl433_rfraw_reconstruct_from_json(const char *json_str, pulse_data_t *pulse_data);

#ifdef __cplusplus
}
#endif

#endif // RTL433_RFRAW_H

