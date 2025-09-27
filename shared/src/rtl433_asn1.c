/**
 * @file rtl433_asn1.c
 * @brief ASN.1 encoding/decoding implementation for RTL433 shared library
 */

#include "rtl433_asn1.h"

#ifdef ENABLE_ASN1

#include <stdlib.h>
#include <string.h>
#include <time.h>

// Include RTL433 headers for pulse_data_t
#include "pulse_data.h"

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
) {
    rtl433_asn1_buffer_t result = {0};
    
    // Create RTL433Message wrapper
    RTL433Message_t *rtl_message = calloc(1, sizeof(RTL433Message_t));
    if (!rtl_message) {
        result.result = RTL433_ASN1_ERROR_MEMORY;
        return result;
    }
    
    // Create SignalMessage
    SignalMessage_t *signal_msg = calloc(1, sizeof(SignalMessage_t));
    if (!signal_msg) {
        free(rtl_message);
        result.result = RTL433_ASN1_ERROR_MEMORY;
        return result;
    }
    
    // Set package ID
    signal_msg->packageId = package_id;
    
    // Set timestamp if provided
    if (timestamp) {
        signal_msg->timestamp = create_generalized_time(timestamp);
    }
    
    // Set signal data
    if (hex_string && hex_string_len > 0) {
        // Use hex string format
        signal_msg->signalData.present = SignalData_PR_hexString;
        OCTET_STRING_fromBuf(&signal_msg->signalData.choice.hexString, 
                            (const char*)hex_string, hex_string_len);
    } else if (pulses_data && pulses_count > 0) {
        // Use pulses array format
        signal_msg->signalData.present = SignalData_PR_pulsesArray;
        PulsesData_t *pulses = &signal_msg->signalData.choice.pulsesArray;
        
        pulses->count = pulses_count;
        pulses->sampleRate = sample_rate;
        
        // Allocate and copy pulse data
        for (int i = 0; i < pulses_count; i++) {
            long *pulse_val = calloc(1, sizeof(long));
            if (pulse_val) {
                *pulse_val = pulses_data[i];
                asn_sequence_add(&pulses->pulses, pulse_val);
            }
        }
    } else {
        // No signal data provided
        free(signal_msg);
        free(rtl_message);
        result.result = RTL433_ASN1_ERROR_INVALID_DATA;
        return result;
    }
    
    // Set modulation
    signal_msg->modulation = modulation;
    
    // Set RF parameters
    signal_msg->frequency.centerFreq = frequency;
    
    // Set message type
    rtl_message->present = RTL433Message_PR_signalMessage;
    rtl_message->choice.signalMessage = *signal_msg;
    
    // Encode to binary using unaligned PER
    ssize_t encoded_size = uper_encode_to_new_buffer(&asn_DEF_RTL433Message, 
                                                     NULL, rtl_message, 
                                                     (void**)&result.buffer);
    
    if (encoded_size < 0) {
        result.result = RTL433_ASN1_ERROR_ENCODE;
        result.buffer_size = 0;
    } else {
        result.buffer_size = encoded_size;
        result.result = RTL433_ASN1_OK;
    }
    
    // Cleanup
    ASN_STRUCT_FREE(asn_DEF_RTL433Message, rtl_message);
    
    return result;
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
        result.result = RTL433_ASN1_ERROR_ENCODE;
        result.buffer_size = 0;
    } else {
        result.buffer_size = encoded_size;
        result.result = RTL433_ASN1_OK;
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

rtl433_asn1_result_t rtl433_asn1_decode_signal_to_pulse_data(
    const uint8_t *buffer,
    size_t buffer_size,
    void *pulse_data_ptr
) {
    if (!buffer || buffer_size == 0 || !pulse_data_ptr) {
        return RTL433_ASN1_ERROR_INVALID_DATA;
    }
    
    pulse_data_t *pulse_data = (pulse_data_t*)pulse_data_ptr;
    RTL433Message_t *message = NULL;
    asn_dec_rval_t rval;
    
    // Decode the message using unaligned PER
    rval = uper_decode_complete(NULL, &asn_DEF_RTL433Message, (void**)&message,
                               buffer, buffer_size);
    
    if (rval.code != RC_OK) {
        return RTL433_ASN1_ERROR_DECODE;
    }
    
    // Check if this is a signal message
    if (message->present != RTL433Message_PR_signalMessage) {
        ASN_STRUCT_FREE(asn_DEF_RTL433Message, message);
        return RTL433_ASN1_ERROR_INVALID_DATA;
    }
    
    SignalMessage_t *signal_msg = &message->choice.signalMessage;
    
    // Initialize pulse_data structure
    memset(pulse_data, 0, sizeof(pulse_data_t));
    
    // Extract basic parameters
    pulse_data->freq1_hz = signal_msg->frequency.centerFreq;
    if (signal_msg->frequency.freq1) {
        pulse_data->freq2_hz = *(signal_msg->frequency.freq1);
    }
    
    // Extract signal data
    if (signal_msg->signalData.present == SignalData_PR_hexString) {
        // hex_string format - reconstruct from hex bytes
        OCTET_STRING_t *hex_data = &signal_msg->signalData.choice.hexString;
        
        // Convert hex bytes to pulse array (implementation depends on rtl433 internals)
        // This would need to call rtl433's hex-to-pulse conversion functions
        // For now, store the hex data and mark it for later conversion
        if (hex_data->size <= sizeof(pulse_data->pulse) * 2) {
            // Simple hex decode - this is a placeholder
            pulse_data->num_pulses = hex_data->size * 4; // Approximate
            // Real implementation would call rtl433_signal_reconstruct_from_hex_bytes
        }
        
    } else if (signal_msg->signalData.present == SignalData_PR_pulsesArray) {
        // pulses array format - direct copy
        PulsesData_t *pulses = &signal_msg->signalData.choice.pulsesArray;
        
        pulse_data->sample_rate = pulses->sampleRate;
        pulse_data->num_pulses = (pulses->count < PD_MAX_PULSES) ? pulses->count : PD_MAX_PULSES;
        
        // Copy pulse values
        for (unsigned int i = 0; i < pulse_data->num_pulses; i++) {
            if (i < (unsigned int)pulses->pulses.list.count) {
                pulse_data->pulse[i] = *(pulses->pulses.list.array[i]);
            }
        }
    }
    
    // Extract timing info if available
    if (signal_msg->timingInfo) {
        if (signal_msg->timingInfo->offset) {
            pulse_data->offset = *(signal_msg->timingInfo->offset);
        }
    }
    
    // Extract signal quality if available
    if (signal_msg->signalQuality) {
        if (signal_msg->signalQuality->rssiDb) {
            pulse_data->rssi_db = *(signal_msg->signalQuality->rssiDb);
        }
        if (signal_msg->signalQuality->snrDb) {
            pulse_data->snr_db = *(signal_msg->signalQuality->snrDb);
        }
    }
    
    // Cleanup
    ASN_STRUCT_FREE(asn_DEF_RTL433Message, message);
    return RTL433_ASN1_OK;
}

rtl433_asn1_buffer_t rtl433_asn1_encode_pulse_data_to_signal(
    const void *pulse_data_ptr,
    uint32_t package_id
) {
    rtl433_asn1_buffer_t result = {0};
    
    if (!pulse_data_ptr) {
        result.result = RTL433_ASN1_ERROR_INVALID_DATA;
        return result;
    }
    
    const pulse_data_t *pulse_data = (const pulse_data_t*)pulse_data_ptr;
    
    // Create RTL433Message wrapper
    RTL433Message_t *rtl_message = calloc(1, sizeof(RTL433Message_t));
    if (!rtl_message) {
        result.result = RTL433_ASN1_ERROR_MEMORY;
        return result;
    }
    
    // Create SignalMessage
    SignalMessage_t *signal_msg = calloc(1, sizeof(SignalMessage_t));
    if (!signal_msg) {
        free(rtl_message);
        result.result = RTL433_ASN1_ERROR_MEMORY;
        return result;
    }
    
    // Set package ID
    signal_msg->packageId = package_id;
    
    // Set signal data as pulses array (most complete format)
    signal_msg->signalData.present = SignalData_PR_pulsesArray;
    PulsesData_t *pulses = &signal_msg->signalData.choice.pulsesArray;
    
    pulses->count = pulse_data->num_pulses;
    pulses->sampleRate = pulse_data->sample_rate;
    
    // Copy pulse data
    for (unsigned int i = 0; i < pulse_data->num_pulses && i < PD_MAX_PULSES; i++) {
        long *pulse_val = calloc(1, sizeof(long));
        if (pulse_val) {
            *pulse_val = pulse_data->pulse[i];
            asn_sequence_add(&pulses->pulses, pulse_val);
        }
    }
    
    // Set modulation (try to detect from pulse characteristics)
    signal_msg->modulation = ModulationType_ook; // Default to OOK
    
    // Set RF parameters
    signal_msg->frequency.centerFreq = pulse_data->freq1_hz;
    if (pulse_data->freq2_hz != 0.0) {
        signal_msg->frequency.freq1 = calloc(1, sizeof(long));
        if (signal_msg->frequency.freq1) {
            *(signal_msg->frequency.freq1) = pulse_data->freq2_hz;
        }
    }
    
    // Set signal quality if available
    if (pulse_data->rssi_db != 0.0 || pulse_data->snr_db != 0.0) {
        signal_msg->signalQuality = calloc(1, sizeof(SignalQuality_t));
        if (signal_msg->signalQuality) {
            if (pulse_data->rssi_db != 0.0) {
                signal_msg->signalQuality->rssiDb = calloc(1, sizeof(double));
                if (signal_msg->signalQuality->rssiDb) {
                    *(signal_msg->signalQuality->rssiDb) = pulse_data->rssi_db;
                }
            }
            if (pulse_data->snr_db != 0.0) {
                signal_msg->signalQuality->snrDb = calloc(1, sizeof(double));
                if (signal_msg->signalQuality->snrDb) {
                    *(signal_msg->signalQuality->snrDb) = pulse_data->snr_db;
                }
            }
        }
    }
    
    // Set timing info if available
    if (pulse_data->offset != 0) {
        signal_msg->timingInfo = calloc(1, sizeof(TimingInfo_t));
        if (signal_msg->timingInfo) {
            signal_msg->timingInfo->offset = calloc(1, sizeof(long));
            if (signal_msg->timingInfo->offset) {
                *(signal_msg->timingInfo->offset) = pulse_data->offset;
            }
        }
    }
    
    // Set message type
    rtl_message->present = RTL433Message_PR_signalMessage;
    rtl_message->choice.signalMessage = *signal_msg;
    
    // Encode to binary using unaligned PER
    ssize_t encoded_size = uper_encode_to_new_buffer(&asn_DEF_RTL433Message, 
                                                     NULL, rtl_message, 
                                                     (void**)&result.buffer);
    
    if (encoded_size < 0) {
        result.result = RTL433_ASN1_ERROR_ENCODE;
        result.buffer_size = 0;
    } else {
        result.buffer_size = encoded_size;
        result.result = RTL433_ASN1_OK;
    }
    
    // Cleanup
    ASN_STRUCT_FREE(asn_DEF_RTL433Message, rtl_message);
    
    return result;
}

const char* rtl433_asn1_get_version(void) {
    return "RTL433 ASN.1 v1.0 (PER encoding)";
}

rtl433_asn1_result_t rtl433_asn1_validate(const uint8_t *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return RTL433_ASN1_ERROR_INVALID_DATA;
    }
    
    RTL433Message_t *message = NULL;
    asn_dec_rval_t rval;
    
    // Try to decode the message using unaligned PER
    rval = uper_decode_complete(NULL, &asn_DEF_RTL433Message, (void**)&message,
                               buffer, buffer_size);
    
    if (rval.code != RC_OK) {
        return RTL433_ASN1_ERROR_DECODE;
    }
    
    // Cleanup and return success
    ASN_STRUCT_FREE(asn_DEF_RTL433Message, message);
    return RTL433_ASN1_OK;
}

#endif /* ENABLE_ASN1 */
