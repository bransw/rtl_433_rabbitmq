/**
 * @file rtl433_asn1.h
 * @brief ASN.1 encoding/decoding interface for RTL433 shared library
 * 
 * This file provides C interface for ASN.1 binary encoding of RTL433 messages.
 * ASN.1 provides efficient binary encoding compared to JSON for RabbitMQ transport.
 */

#ifndef RTL433_ASN1_H
#define RTL433_ASN1_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#ifdef ENABLE_ASN1

// Forward declarations for ASN.1 structures
// Note: These will be redefined by generated headers, but we need them for interface
struct RTL433Message;
struct SignalMessage;
struct DetectedMessage;
struct StatusMessage;
struct ConfigMessage;

// Error codes for ASN.1 operations
typedef enum {
    RTL433_ASN1_OK = 0,
    RTL433_ASN1_ERROR_ENCODE = -1,
    RTL433_ASN1_ERROR_DECODE = -2,
    RTL433_ASN1_ERROR_MEMORY = -3,
    RTL433_ASN1_ERROR_INVALID_DATA = -4
} rtl433_asn1_result_t;

// Encoding/decoding result structure
typedef struct {
    uint8_t *buffer;        // Encoded/decoded data buffer
    size_t buffer_size;     // Size of the buffer
    rtl433_asn1_result_t result;  // Operation result
} rtl433_asn1_buffer_t;

/**
 * @brief Encode RTL433 signal message to ASN.1 binary format
 * 
 * @param package_id Message package identifier
 * @param timestamp Message timestamp (can be NULL)
 * @param hex_string Signal data as hex string (can be NULL if pulses_data provided)
 * @param hex_string_len Length of hex string
 * @param pulses_data Pulse array data (can be NULL if hex_string provided)
 * @param pulses_count Number of pulses
 * @param sample_rate Sample rate for pulses
 * @param modulation Modulation type (0=OOK, 1=FSK, etc.)
 * @param frequency Center frequency in Hz
 * @return Encoded ASN.1 buffer (caller must free buffer)
 */
rtl433_asn1_buffer_t rtl433_asn1_encode_signal(
    uint32_t package_id,
    const char *timestamp,
    const uint8_t *hex_string,
    size_t hex_string_len,
    const uint16_t *pulses_data,
    uint16_t pulses_count,
    uint32_t sample_rate,
    int modulation,
    uint32_t frequency
);

/**
 * @brief Encode RTL433 detected device message to ASN.1 binary format
 * 
 * @param package_id Message package identifier (can be 0)
 * @param timestamp Message timestamp (can be NULL)
 * @param model Device model name
 * @param device_type Device type (can be NULL)
 * @param device_id Device identifier (can be NULL)
 * @param protocol Protocol name (can be NULL)
 * @param data_fields Array of key-value data fields
 * @param fields_count Number of data fields
 * @return Encoded ASN.1 buffer (caller must free buffer)
 */
rtl433_asn1_buffer_t rtl433_asn1_encode_detected(
    uint32_t package_id,
    const char *timestamp,
    const char *model,
    const char *device_type,
    const char *device_id,
    const char *protocol,
    const char **data_fields,  // Array of "key\0value\0" pairs
    size_t fields_count
);

/**
 * @brief Decode ASN.1 binary data to RTL433 message
 * 
 * @param buffer ASN.1 encoded binary data
 * @param buffer_size Size of the encoded data
 * @param message_type Output: type of decoded message (0=signal, 1=detected, 2=status, 3=config)
 * @return Decoded message structure (caller must free)
 */
void* rtl433_asn1_decode_message(
    const uint8_t *buffer,
    size_t buffer_size,
    int *message_type
);

/**
 * @brief Free ASN.1 buffer allocated by encoding functions
 * 
 * @param buffer Buffer to free
 */
void rtl433_asn1_free_buffer(rtl433_asn1_buffer_t *buffer);

/**
 * @brief Free decoded ASN.1 message structure
 * 
 * @param message Message structure to free
 * @param message_type Type of message (0=signal, 1=detected, 2=status, 3=config)
 */
void rtl433_asn1_free_message(void *message, int message_type);

/**
 * @brief Decode ASN.1 signal message and reconstruct pulse_data
 * 
 * @param buffer ASN.1 encoded signal message
 * @param buffer_size Size of the encoded data
 * @param pulse_data Output: reconstructed pulse_data_t structure
 * @return RTL433_ASN1_OK if successful, error code otherwise
 */
rtl433_asn1_result_t rtl433_asn1_decode_signal_to_pulse_data(
    const uint8_t *buffer,
    size_t buffer_size,
    void *pulse_data  // pulse_data_t* - avoiding header dependency
);

/**
 * @brief Encode pulse_data_t to ASN.1 signal message
 * 
 * @param pulse_data Input pulse_data_t structure
 * @param package_id Message package identifier
 * @return Encoded ASN.1 buffer (caller must free buffer)
 */
rtl433_asn1_buffer_t rtl433_asn1_encode_pulse_data_to_signal(
    const void *pulse_data,  // pulse_data_t* - avoiding header dependency
    uint32_t package_id
);

/**
 * @brief Free ASN.1 buffer allocated by encoding functions
 * 
 * @param buffer Buffer to free
 */
void rtl433_asn1_free_buffer(rtl433_asn1_buffer_t *buffer);

/**
 * @brief Get ASN.1 library version information
 * 
 * @return Version string
 */
const char* rtl433_asn1_get_version(void);

/**
 * @brief Validate ASN.1 encoded data
 * 
 * @param buffer ASN.1 encoded binary data
 * @param buffer_size Size of the encoded data
 * @return RTL433_ASN1_OK if valid, error code otherwise
 */
rtl433_asn1_result_t rtl433_asn1_validate(const uint8_t *buffer, size_t buffer_size);

#else /* !ENABLE_ASN1 */

// Stub functions when ASN.1 is not available
static inline const char* rtl433_asn1_get_version(void) {
    return "ASN.1 support not compiled";
}

#endif /* ENABLE_ASN1 */

#ifdef __cplusplus
}
#endif

#endif /* RTL433_ASN1_H */
