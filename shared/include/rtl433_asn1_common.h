/**
 * @file rtl433_asn1_common.h
 * @brief Common ASN.1 types and constants shared between pulse and detect modules
 */

#ifndef RTL433_ASN1_COMMON_H
#define RTL433_ASN1_COMMON_H

#include <stdint.h>
#include <stddef.h>

/// ASN.1 operation result codes
typedef enum {
    RTL433_ASN1_OK = 0,
    RTL433_ASN1_ERROR_ENCODE = -1,
    RTL433_ASN1_ERROR_DECODE = -2,
    RTL433_ASN1_ERROR_MEMORY = -3,
    RTL433_ASN1_ERROR_INVALID_DATA = -4,
    RTL433_ASN1_ERROR_INVALID_MESSAGE_TYPE = -5
} rtl433_asn1_result_t;

/// ASN.1 buffer structure for encoded data
typedef struct {
    uint8_t *buffer;        ///< Encoded binary data
    size_t buffer_size;     ///< Size of encoded data in bytes
    rtl433_asn1_result_t result; ///< Operation result code
} rtl433_asn1_buffer_t;

#endif /* RTL433_ASN1_COMMON_H */
