/**
 * @file rtl433_asn1.h
 * @brief ASN.1 encoding/decoding interface for RTL433 shared library
 * 
 * This file provides C interface for ASN.1 binary encoding of RTL433 messages.
 * ASN.1 provides efficient binary encoding compared to JSON for RabbitMQ transport.
 */

#ifndef RTL433_ASN1_DETECT_H
#define RTL433_ASN1_DETECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "rtl433_asn1_common.h"

// ASN.1 support is always enabled

// ASN.1 size constraints from rtl433-rabbitmq.asn1 specification
#define RTL433_ASN1_HEX_STRING_MAX_SIZE     512  // hexStringMaxSize from ASN.1 spec
#define RTL433_ASN1_HEX_STRINGS_MAX_COUNT   32   // hexStringsMaxCount from ASN.1 spec  
#define RTL433_ASN1_HEX_STRING_MIN_SIZE     1    // hexStringMinSize from ASN.1 spec
#define RTL433_ASN1_PULSES_MAX_COUNT        65535 // pulsesMaxCount from ASN.1 spec

// Forward declarations for ASN.1 structures
// Note: These will be redefined by generated headers, but we need them for interface
struct RTL433Message;
struct SignalMessage;
struct DetectedMessage;
struct StatusMessage;
struct ConfigMessage;

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


// End of ASN.1 interface

#ifdef __cplusplus
}
#endif

#endif /* RTL433_ASN1_DETECT_H */
