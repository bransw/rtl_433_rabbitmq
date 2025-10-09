/*
 * RTL433 ASN.1 Pulse Data Extended Support
 * Header file for extended pulse data structures and ASN.1 encoding/decoding
 */

#ifndef RTL433_ASN1_PULSE_H
#define RTL433_ASN1_PULSE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <json-c/json.h>
#include "pulse_data.h"
#include "rtl433_asn1_common.h"

// ASN.1 generated headers (forward declarations to avoid conflicts)
// These will be properly defined when ASN.1 headers are included in .c file
typedef struct RTL433Message RTL433Message_t;

#ifdef __cplusplus
extern "C" {
#endif

// Extended pulse data structure with hex string support
typedef struct pulse_data_ex {
    pulse_data_t pulse_data;        // Embedded original structure
    char *hex_string;               // Hex string representation (single or multiple signals separated by '+')
    char *modulation_type;          // Modulation type string ("FSK", "OOK", "ASK", etc.)
    char *timestamp_str;            // Timestamp string (e.g., "@0.262144s")
    uint32_t package_id;            // Package/message identifier (0 = not set)
} pulse_data_ex_t;

// === pulse_data_ex_t Management Functions ===

/**
 * Initialize pulse_data_ex_t structure
 * @param data_ex Pointer to pulse_data_ex_t structure
 */
void pulse_data_ex_init(pulse_data_ex_t *data_ex);

/**
 * Free memory allocated for pulse_data_ex_t structure
 * @param data_ex Pointer to pulse_data_ex_t structure
 */
void pulse_data_ex_free(pulse_data_ex_t *data_ex);

/**
 * Set hex string in pulse_data_ex_t structure
 * @param data_ex Pointer to pulse_data_ex_t structure
 * @param hex_string Hex string to set
 * @return 0 on success, -1 on error
 */
int pulse_data_ex_set_hex_string(pulse_data_ex_t *data_ex, const char *hex_string);

/**
 * Get hex string from pulse_data_ex_t structure
 * @param data_ex Pointer to pulse_data_ex_t structure
 * @return Hex string or NULL
 */
const char* pulse_data_ex_get_hex_string(const pulse_data_ex_t *data_ex);

/**
 * Set modulation type in pulse_data_ex_t structure
 * @param data_ex Pointer to pulse_data_ex_t structure
 * @param modulation_type Modulation type string to set
 * @return 0 on success, -1 on error
 */
int pulse_data_ex_set_modulation_type(pulse_data_ex_t *data_ex, const char *modulation_type);

/**
 * Get modulation type from pulse_data_ex_t structure
 * @param data_ex Pointer to pulse_data_ex_t structure
 * @return Modulation type string or NULL
 */
const char* pulse_data_ex_get_modulation_type(const pulse_data_ex_t *data_ex);

/**
 * Set timestamp string in pulse_data_ex_t structure
 * @param data_ex Pointer to pulse_data_ex_t structure
 * @param timestamp_str Timestamp string to set
 * @return 0 on success, -1 on error
 */
int pulse_data_ex_set_timestamp_str(pulse_data_ex_t *data_ex, const char *timestamp_str);

/**
 * Get timestamp string from pulse_data_ex_t structure
 * @param data_ex Pointer to pulse_data_ex_t structure
 * @return Timestamp string or NULL
 */
const char* pulse_data_ex_get_timestamp_str(const pulse_data_ex_t *data_ex);

/**
 * Set package ID in pulse_data_ex_t structure
 * @param data_ex Pointer to pulse_data_ex_t structure
 * @param package_id Package ID to set (0 = not set)
 * @return 0 on success, -1 on error
 */
int pulse_data_ex_set_package_id(pulse_data_ex_t *data_ex, uint32_t package_id);

/**
 * Get package ID from pulse_data_ex_t structure
 * @param data_ex Pointer to pulse_data_ex_t structure
 * @return Package ID (0 = not set)
 */
uint32_t pulse_data_ex_get_package_id(const pulse_data_ex_t *data_ex);

// === Hex String Utility Functions ===

/**
 * Split hex string by '+' delimiter
 * @param hex_string Input hex string with '+' separators
 * @param count Output parameter for number of strings
 * @return Array of hex strings or NULL on error
 */
char** split_hex_string(const char *hex_string, int *count);

/**
 * Free array of hex strings created by split_hex_string
 * @param hex_strings Array of hex strings
 * @param count Number of strings in array
 */
void free_hex_strings_array(char **hex_strings, int count);

/**
 * Convert hex string to binary data
 * @param hex_string Input hex string
 * @param binary_size Output parameter for binary data size
 * @return Binary data or NULL on error
 */
unsigned char* hex_to_binary(const char *hex_string, size_t *binary_size);

// === ASN.1 Data Preparation Functions ===

/**
 * Set package ID in RTL433Message (optional message sequence number)
 * @param rtl433_msg Pointer to RTL433Message structure
 * @param package_id Package ID to set
 * @return 0 on success, -1 on error
 */
int rtl433_message_set_package_id(RTL433Message_t *rtl433_msg, unsigned long package_id);

/**
 * Prepare RTL433Message from pulse_data_ex_t
 * @param pulse_ex Pointer to pulse_data_ex_t structure
 * @return RTL433Message_t structure or NULL on error
 */
RTL433Message_t *prepare_pulse_data(pulse_data_ex_t *pulse_ex);

/**
 * Extract pulse_data_ex from RTL433Message
 * @param rtl433_msg Pointer to RTL433Message_t structure
 * @return pulse_data_ex_t structure or NULL on error
 */
pulse_data_ex_t *extract_pulse_data(RTL433Message_t *rtl433_msg);

// === ASN.1 Encoding/Decoding Functions ===

/**
 * Encode pulse_data_ex_t to ASN.1 RTL433Message
 * @param pulse_ex Pointer to pulse_data_ex_t structure
 * @return rtl433_asn1_buffer_t with encoded data
 */
rtl433_asn1_buffer_t encode(pulse_data_ex_t *pulse_ex);

/**
 * Decode ASN.1 RTL433Message to pulse_data_t
 * @param buffer Pointer to rtl433_asn1_buffer_t with encoded data
 * @return pulse_data_t structure or NULL on error
 */
pulse_data_t* decode(const rtl433_asn1_buffer_t *buffer);

/**
 * Encode pulse_data_t as RTL433Message (wrapper function)
 * @param pulse Pointer to pulse_data_t structure
 * @return rtl433_asn1_buffer_t with encoded data
 */
rtl433_asn1_buffer_t encode_rtl433_message(pulse_data_t *pulse);

// === JSON Parsing Functions ===

/**
 * Parse JSON string to pulse_data_ex_t structure
 * @param json_string JSON string to parse
 * @return pulse_data_ex_t structure or NULL on error
 */
pulse_data_ex_t* parse_json_to_pulse_data_ex(const char *json_string);

// === Print Functions ===

/**
 * Print pulse_data_t structure
 * @param pulse_data Pointer to pulse_data_t structure
 */
void print_pulse_data(const pulse_data_t *pulse_data);

/**
 * Print pulse_data_ex_t structure
 * @param pulse_data_ex Pointer to pulse_data_ex_t structure
 */
void print_pulse_data_ex(const pulse_data_ex_t *pulse_data_ex);

// === Utility Functions ===

/**
 * Decode hex ASN.1 data
 * @param hex_string Hex string to decode
 * @param use_rtl433_wrapper Whether to use RTL433Message wrapper
 */
void decode_hex_asn1(const char *hex_string, bool use_rtl433_wrapper);

/**
 * Parse rfraw format and extract ONLY pulses, preserving all other fields
 * @param data Pointer to pulse_data_t structure
 * @param p Hex string to parse
 * @return true if successful, false otherwise
 */
bool rfraw_parse_pulses_only(pulse_data_t *data, char const *p);

#ifdef __cplusplus
}
#endif

#endif // RTL433_ASN1_PULSE_H
