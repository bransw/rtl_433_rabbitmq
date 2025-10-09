/**
 * @file rtl433_asn1.c
 * @brief ASN.1 encoding/decoding implementation for RTL433 shared library
 */

#include "rtl433_asn1_detect.h"

#ifdef ENABLE_ASN1

#include <stdlib.h>
#include <string.h>
#include <time.h>

// Include RTL433 headers for pulse_data_t
#include "pulse_data.h"
#include "rtl433_rfraw.h"

// Include generated ASN.1 headers
#include "RTL433Message.h"
#include "SignalMessage.h"
#include "DetectedMessage.h"
#include "StatusMessage.h"
#include "ConfigMessage.h"
#include "SignalData.h"
#include "PulsesData.h"
#include "ModulationType.h"
#include "RFParameters.h"
#include "SignalQuality.h"
#include "TimingInfo.h"
#include "DeviceInfo.h"
#include "DeviceData.h"
#include "DeviceDataField.h"
#include "DeviceFieldValue.h"

// ASN.1 encoding/decoding functions
#include "per_encoder.h"
#include "per_decoder.h"

/**
 * Convert timestamp string to ASN.1 GeneralizedTime
 */
static GeneralizedTime_t* create_generalized_time(const char *timestamp) {
    if (!timestamp) return NULL;
    
    GeneralizedTime_t *gt = calloc(1, sizeof(GeneralizedTime_t));
    if (!gt) return NULL;
    
    // If timestamp is NULL, use current time
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y%m%d%H%M%SZ", tm_info);
    
    OCTET_STRING_fromString(gt, time_str);
    return gt;
}

/**
 * Create device data field from key-value pair
 */
static DeviceDataField_t* create_device_field(const char *key, const char *value) {
    if (!key || !value) return NULL;
    
    DeviceDataField_t *field = calloc(1, sizeof(DeviceDataField_t));
    if (!field) return NULL;
    
    // Set field name
    OCTET_STRING_fromString(&field->fieldName, key);
    
    // Set field value as string
    field->fieldValue.present = DeviceFieldValue_PR_stringValue;
    OCTET_STRING_fromString(&field->fieldValue.choice.stringValue, value);
    
    return field;
}

rtl433_asn1_buffer_t rtl433_asn1_encode_detected(
    uint32_t package_id,
    const char *timestamp,
    const char *model,
    const char *device_type,
    const char *device_id,
    const char *protocol,
    const char **data_fields,
    size_t fields_count
) {
    rtl433_asn1_buffer_t result = {0};
    
    if (!model) {
        result.result = RTL433_ASN1_ERROR_INVALID_DATA;
        return result;
    }
    
    // Create RTL433Message wrapper
    RTL433Message_t *rtl_message = calloc(1, sizeof(RTL433Message_t));
    if (!rtl_message) {
        result.result = RTL433_ASN1_ERROR_MEMORY;
        return result;
    }
    
    // Create DetectedMessage
    DetectedMessage_t *detected_msg = calloc(1, sizeof(DetectedMessage_t));
    if (!detected_msg) {
        free(rtl_message);
        result.result = RTL433_ASN1_ERROR_MEMORY;
        return result;
    }
    
    // Set package ID (optional)
    if (package_id > 0) {
        detected_msg->packageId = calloc(1, sizeof(long));
        if (detected_msg->packageId) {
            *(detected_msg->packageId) = package_id;
        }
    }
    
    // Set timestamp if provided
    if (timestamp) {
        detected_msg->timestamp = create_generalized_time(timestamp);
    }
    
    // Set device info
    OCTET_STRING_fromString(&detected_msg->deviceInfo.model, model);
    
    if (device_type) {
        detected_msg->deviceInfo.deviceType = calloc(1, sizeof(UTF8String_t));
        if (detected_msg->deviceInfo.deviceType) {
            OCTET_STRING_fromString(detected_msg->deviceInfo.deviceType, device_type);
        }
    }
    
    if (device_id) {
        detected_msg->deviceInfo.deviceId = calloc(1, sizeof(UTF8String_t));
        if (detected_msg->deviceInfo.deviceId) {
            OCTET_STRING_fromString(detected_msg->deviceInfo.deviceId, device_id);
        }
    }
    
    if (protocol) {
        detected_msg->deviceInfo.protocol = calloc(1, sizeof(UTF8String_t));
        if (detected_msg->deviceInfo.protocol) {
            OCTET_STRING_fromString(detected_msg->deviceInfo.protocol, protocol);
        }
    }
    
    // Add data fields
    if (data_fields && fields_count > 0) {
        for (size_t i = 0; i < fields_count; i += 2) {
            if (i + 1 < fields_count) {
                DeviceDataField_t *field = create_device_field(data_fields[i], data_fields[i + 1]);
                if (field) {
                    asn_sequence_add(&detected_msg->deviceData, field);
                }
            }
        }
    }
    
    // Set message type
    rtl_message->present = RTL433Message_PR_detectedMessage;
    rtl_message->choice.detectedMessage = *detected_msg;
    
    // Encode to binary using unaligned PER
    ssize_t encoded_size = uper_encode_to_new_buffer(&asn_DEF_RTL433Message, 
                                                     NULL, rtl_message, 
                                                     (void**)&result.buffer);
    
    if (encoded_size < 0) {
        fprintf(stderr, "❌ ASN.1 encoding failed\n");
        result.result = RTL433_ASN1_ERROR_ENCODE;
        result.buffer_size = 0;
    } else {
        result.buffer_size = encoded_size;
        result.result = RTL433_ASN1_OK;
        
        fprintf(stderr, "✅ ASN.1 encoding successful: %zu bytes\n", result.buffer_size);
        
        // 🔍 HEX DEBUG OUTPUT
        if (result.buffer && result.buffer_size > 0) {
            fprintf(stderr, "🔍 ASN.1 HEX (%zu bytes): ", result.buffer_size);
            const unsigned char *bytes = (const unsigned char *)result.buffer;
            for (size_t i = 0; i < result.buffer_size; i++) {
                fprintf(stderr, "%02X", bytes[i]);
            }
            fprintf(stderr, "\n");
        }
    }
    
    // Cleanup
    ASN_STRUCT_FREE(asn_DEF_RTL433Message, rtl_message);
    
    return result;
}

void* rtl433_asn1_decode_message(
    const uint8_t *buffer,
    size_t buffer_size,
    int *message_type
) {
    if (!buffer || buffer_size == 0 || !message_type) {
        return NULL;
    }
    
    RTL433Message_t *message = NULL;
    asn_dec_rval_t rval;
    
    // Decode the message using unaligned PER
    rval = uper_decode_complete(NULL, &asn_DEF_RTL433Message, (void**)&message,
                               buffer, buffer_size);
    
    if (rval.code != RC_OK) {
        return NULL;
    }
    
    // Determine message type
    switch (message->present) {
        case RTL433Message_PR_signalMessage:
            *message_type = 0;
            break;
        case RTL433Message_PR_detectedMessage:
            *message_type = 1;
            break;
        case RTL433Message_PR_statusMessage:
            *message_type = 2;
            break;
        case RTL433Message_PR_configMessage:
            *message_type = 3;
            break;
        default:
            ASN_STRUCT_FREE(asn_DEF_RTL433Message, message);
            return NULL;
    }
    
    return message;
}

void rtl433_asn1_free_buffer(rtl433_asn1_buffer_t *buffer) {
    if (buffer && buffer->buffer) {
        free(buffer->buffer);
        buffer->buffer = NULL;
        buffer->buffer_size = 0;
    }
}

void rtl433_asn1_free_message(void *message, int message_type) {
    (void)message_type; // Unused parameter
    if (message) {
        ASN_STRUCT_FREE(asn_DEF_RTL433Message, message);
    }
}

const char* rtl433_asn1_get_version(void) {
    return "RTL433 ASN.1 v1.0 (PER encoding)";
}

#endif /* ENABLE_ASN1 */
