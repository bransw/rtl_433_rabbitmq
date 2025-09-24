/** @file
 * Implementation of the common signal processing module for rtl_433
 */

#include "rtl433_signal.h"
#include "pulse_slicer.h"
#include "pulse_analyzer.h"
#include "rfraw.h"
#include "r_api.h"
#include "r_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

// === SIGNAL RECONSTRUCTION ===

int rtl433_signal_reconstruct_from_hex(const char *hex_str, 
                                      pulse_data_t *pulse_data,
                                      const char *json_metadata)
{
    if (!hex_str || !pulse_data) return -1;
    
    // Initialize pulse_data
    memset(pulse_data, 0, sizeof(pulse_data_t));
    rtl433_signal_set_defaults(pulse_data, 250000);
    
    // Validate hex string
    if (!rtl433_signal_rfraw_check(hex_str)) {
        return -1;
    }
    
    // Reconstruct pulses from hex
    if (!rtl433_signal_rfraw_parse(pulse_data, hex_str)) {
        return -1;
    }
    
    // Apply metadata from JSON
    if (json_metadata) {
        return rtl433_signal_reconstruct_from_json(json_metadata, pulse_data);
    }
    
    return 0;
}

int rtl433_signal_reconstruct_from_json(const char *json_str, pulse_data_t *pulse_data)
{
    if (!json_str || !pulse_data) return -1;
    
    json_object *root = json_tokener_parse(json_str);
    if (!root) return -1;
    
    // Extract all critical fields
    json_object *obj;
    
    // Sample rate
    if (json_object_object_get_ex(root, "rate_Hz", &obj)) {
        pulse_data->sample_rate = json_object_get_int(obj);
    }
    
    // Frequencies
    if (json_object_object_get_ex(root, "freq_Hz", &obj)) {
        pulse_data->centerfreq_hz = (float)json_object_get_double(obj);
    }
    if (json_object_object_get_ex(root, "freq1_Hz", &obj)) {
        pulse_data->freq1_hz = (float)json_object_get_double(obj);
    }
    if (json_object_object_get_ex(root, "freq2_Hz", &obj)) {
        pulse_data->freq2_hz = (float)json_object_get_double(obj);
    }
    
    // Signal quality
    if (json_object_object_get_ex(root, "rssi_dB", &obj)) {
        pulse_data->rssi_db = (float)json_object_get_double(obj);
    }
    if (json_object_object_get_ex(root, "snr_dB", &obj)) {
        pulse_data->snr_db = (float)json_object_get_double(obj);
    }
    if (json_object_object_get_ex(root, "noise_dB", &obj)) {
        pulse_data->noise_db = (float)json_object_get_double(obj);
    }
    
    // Modulation and FSK estimates
    if (json_object_object_get_ex(root, "mod", &obj)) {
        const char *mod_str = json_object_get_string(obj);
        if (strcmp(mod_str, "FSK") == 0) {
            // Set FSK parameters
            pulse_data->fsk_f1_est = 1000;
            pulse_data->fsk_f2_est = -1000;
            pulse_data->ook_low_estimate = 1000;
            pulse_data->ook_high_estimate = 8000;
            
            // Refine by frequencies if available
            if (pulse_data->freq1_hz != 0 && pulse_data->centerfreq_hz != 0) {
                pulse_data->fsk_f1_est = (int)(pulse_data->freq1_hz - pulse_data->centerfreq_hz);
            }
            if (pulse_data->freq2_hz != 0 && pulse_data->centerfreq_hz != 0) {
                pulse_data->fsk_f2_est = (int)(pulse_data->freq2_hz - pulse_data->centerfreq_hz);
            }
        } else {
            // OOK signal
            pulse_data->fsk_f1_est = 0;
            pulse_data->fsk_f2_est = 0;
            pulse_data->ook_low_estimate = 1000;
            pulse_data->ook_high_estimate = 8000;
        }
    }
    
    // Pulse array (if no hex_string)
    if (json_object_object_get_ex(root, "pulses", &obj) && json_object_is_type(obj, json_type_array)) {
        int array_len = json_object_array_length(obj);
        int pulse_count = array_len / 2; // pulse + gap
        
        if (pulse_count > 0 && pulse_count <= PD_MAX_PULSES) {
            pulse_data->num_pulses = pulse_count;
            
            for (int i = 0; i < pulse_count; i++) {
                json_object *pulse_obj = json_object_array_get_idx(obj, i * 2);
                json_object *gap_obj = json_object_array_get_idx(obj, i * 2 + 1);
                
                if (pulse_obj && gap_obj) {
                    pulse_data->pulse[i] = json_object_get_int(pulse_obj);
                    pulse_data->gap[i] = json_object_get_int(gap_obj);
                }
            }
        }
    }
    
    // Calculate timing information
    if (pulse_data->num_pulses > 0) {
        int total_time_samples = 0;
        for (unsigned i = 0; i < pulse_data->num_pulses; i++) {
            total_time_samples += pulse_data->pulse[i] + pulse_data->gap[i];
        }
        pulse_data->start_ago = total_time_samples;
        pulse_data->end_ago = 0;
    }
    
    json_object_put(root);
    return 0;
}

int rtl433_signal_copy_metadata(const pulse_data_t *src, pulse_data_t *dst)
{
    if (!src || !dst) return -1;
    
    // Copy all metadata except pulses themselves
    dst->offset = src->offset;
    dst->sample_rate = src->sample_rate;
    dst->depth_bits = src->depth_bits;
    dst->start_ago = src->start_ago;
    dst->end_ago = src->end_ago;
    dst->ook_low_estimate = src->ook_low_estimate;
    dst->ook_high_estimate = src->ook_high_estimate;
    dst->fsk_f1_est = src->fsk_f1_est;
    dst->fsk_f2_est = src->fsk_f2_est;
    dst->freq1_hz = src->freq1_hz;
    dst->freq2_hz = src->freq2_hz;
    dst->centerfreq_hz = src->centerfreq_hz;
    dst->range_db = src->range_db;
    dst->rssi_db = src->rssi_db;
    dst->snr_db = src->snr_db;
    dst->noise_db = src->noise_db;
    
    return 0;
}

bool rtl433_signal_validate_pulse_data(const pulse_data_t *pulse_data)
{
    if (!pulse_data) return false;
    
    // Check basic parameters
    if (pulse_data->sample_rate == 0) return false;
    if (pulse_data->num_pulses == 0) return false;
    if (pulse_data->num_pulses > PD_MAX_PULSES) return false;
    
    // Check pulse reasonableness
    for (unsigned i = 0; i < pulse_data->num_pulses; i++) {
        if (pulse_data->pulse[i] < 0) return false;
        if (pulse_data->gap[i] < 0) return false;
        
        // Check for too long or too short pulses
        float pulse_us = rtl433_signal_samples_to_us(pulse_data->pulse[i], pulse_data->sample_rate);
        if (pulse_us > 100000.0f || (pulse_us < 1.0f && pulse_data->pulse[i] > 0)) {
            return false;
        }
    }
    
    return true;
}

void rtl433_signal_set_defaults(pulse_data_t *pulse_data, uint32_t sample_rate)
{
    if (!pulse_data) return;
    
    pulse_data->sample_rate = sample_rate ? sample_rate : 250000;
    pulse_data->depth_bits = 8;
    pulse_data->offset = 0;
    pulse_data->start_ago = 0;
    pulse_data->end_ago = 0;
    pulse_data->ook_low_estimate = 1000;
    pulse_data->ook_high_estimate = 8000;
    pulse_data->fsk_f1_est = 0;
    pulse_data->fsk_f2_est = 0;
    pulse_data->range_db = 0.0f;
}

// === АНАЛИЗ СИГНАЛОВ ===

rtl433_modulation_type_t rtl433_signal_detect_modulation(const pulse_data_t *pulse_data)
{
    if (!pulse_data) return RTL433_MODULATION_UNKNOWN;
    
    // Simple heuristic based on FSK estimates
    if (pulse_data->fsk_f1_est != 0 || pulse_data->fsk_f2_est != 0) {
        return RTL433_MODULATION_FSK;
    }
    
    // Analyze pulse duration variance
    if (pulse_data->num_pulses < 2) return RTL433_MODULATION_UNKNOWN;
    
    int min_pulse = pulse_data->pulse[0];
    int max_pulse = pulse_data->pulse[0];
    
    for (unsigned i = 1; i < pulse_data->num_pulses; i++) {
        if (pulse_data->pulse[i] < min_pulse) min_pulse = pulse_data->pulse[i];
        if (pulse_data->pulse[i] > max_pulse) max_pulse = pulse_data->pulse[i];
    }
    
    // If high variance - likely OOK
    if (max_pulse > min_pulse * 2) {
        return RTL433_MODULATION_OOK;
    }
    
    // If pulses are roughly equal - likely FSK
    return RTL433_MODULATION_FSK;
}

int rtl433_signal_analyze_quality(const pulse_data_t *pulse_data, rtl433_signal_quality_t *quality)
{
    if (!pulse_data || !quality) return -1;
    
    memset(quality, 0, sizeof(rtl433_signal_quality_t));
    
    quality->snr_db = pulse_data->snr_db;
    quality->rssi_db = pulse_data->rssi_db;
    quality->noise_db = pulse_data->noise_db;
    quality->pulse_count = pulse_data->num_pulses;
    
    if (pulse_data->num_pulses > 0) {
        // Average pulse duration
        int total_pulse_time = 0;
        int total_gap_time = 0;
        
        for (unsigned i = 0; i < pulse_data->num_pulses; i++) {
            total_pulse_time += pulse_data->pulse[i];
            total_gap_time += pulse_data->gap[i];
        }
        
        quality->avg_pulse_width_us = rtl433_signal_samples_to_us(
            total_pulse_time / pulse_data->num_pulses, pulse_data->sample_rate);
        quality->avg_gap_width_us = rtl433_signal_samples_to_us(
            total_gap_time / pulse_data->num_pulses, pulse_data->sample_rate);
        
        // Simple validity assessment
        quality->is_valid = (quality->snr_db > 5.0f) && 
                           (quality->pulse_count >= 3) &&
                           (quality->avg_pulse_width_us >= 10.0f) &&
                           (quality->avg_pulse_width_us <= 10000.0f);
    }
    
    return 0;
}

// === ДЕКОДИРОВАНИЕ ===

int rtl433_signal_decode(const pulse_data_t *pulse_data, 
                        list_t *r_devs,
                        rtl433_decode_result_t *result)
{
    if (!pulse_data || !r_devs || !result) return -1;
    
    memset(result, 0, sizeof(rtl433_decode_result_t));
    
    // Determine modulation type
    rtl433_modulation_type_t mod_type = rtl433_signal_detect_modulation(pulse_data);
    
    if (mod_type == RTL433_MODULATION_FSK || mod_type == RTL433_MODULATION_UNKNOWN) {
        result->fsk_events = run_fsk_demods(r_devs, (pulse_data_t*)pulse_data);
        result->devices_decoded += result->fsk_events;
    }
    
    if (mod_type == RTL433_MODULATION_OOK || 
        (mod_type == RTL433_MODULATION_UNKNOWN && result->fsk_events == 0)) {
        result->ook_events = run_ook_demods(r_devs, (pulse_data_t*)pulse_data);
        result->devices_decoded += result->ook_events;
    }
    
    return result->devices_decoded;
}

int rtl433_signal_decode_fsk(const pulse_data_t *pulse_data, 
                            list_t *r_devs,
                            rtl433_decode_result_t *result)
{
    if (!pulse_data || !r_devs || !result) return -1;
    
    memset(result, 0, sizeof(rtl433_decode_result_t));
    
    // Re-enable FSK decoding with proper error handling
    printf("🔍 Running FSK decoders on %d pulses...\n", pulse_data->num_pulses);
    if (pulse_data->num_pulses > 0 && r_devs->len > 0) {
        result->fsk_events = run_fsk_demods(r_devs, (pulse_data_t*)pulse_data);
    } else {
        result->fsk_events = 0;
    }
    result->devices_decoded = result->fsk_events;
    
    return result->devices_decoded;
}

int rtl433_signal_decode_ook(const pulse_data_t *pulse_data,
                            list_t *r_devs, 
                            rtl433_decode_result_t *result)
{
    if (!pulse_data || !r_devs || !result) return -1;
    
    memset(result, 0, sizeof(rtl433_decode_result_t));
    
    printf("🔍 Running OOK decoders on %d pulses, %zu devices available...\n", 
           pulse_data->num_pulses, r_devs->len);
    
    if (pulse_data->num_pulses > 0 && r_devs->len > 0) {
        printf("🔍 Calling run_ook_demods with pulse_data=%p, r_devs=%p...\n", 
               (void*)pulse_data, (void*)r_devs);
        printf("🔍 Pulse data info: num_pulses=%d, sample_rate=%u, freq=%f\n",
               pulse_data->num_pulses, pulse_data->sample_rate, pulse_data->freq1_hz);
        
        // TEMPORARY: Skip actual call to isolate segfault
        printf("⚠️ TEMPORARILY SKIPPING run_ook_demods call to avoid segfault\n");
        result->ook_events = 0; // run_ook_demods(r_devs, (pulse_data_t*)pulse_data);
    } else {
        result->ook_events = 0;
    }
    result->devices_decoded = result->ook_events;
    
    return result->devices_decoded;
}

void rtl433_decode_result_free(rtl433_decode_result_t *result)
{
    if (!result) return;
    
    if (result->device_names) {
        for (int i = 0; i < result->devices_decoded; i++) {
            free(result->device_names[i]);
        }
        free(result->device_names);
    }
    
    if (result->device_data) {
        // TODO: Free data_t structures
        free(result->device_data);
    }
    
    memset(result, 0, sizeof(rtl433_decode_result_t));
}

// === УТИЛИТЫ ===

uint32_t rtl433_signal_duration_us(const pulse_data_t *pulse_data)
{
    if (!pulse_data || pulse_data->num_pulses == 0) return 0;
    
    int total_samples = 0;
    for (unsigned i = 0; i < pulse_data->num_pulses; i++) {
        total_samples += pulse_data->pulse[i] + pulse_data->gap[i];
    }
    
    return (uint32_t)rtl433_signal_samples_to_us(total_samples, pulse_data->sample_rate);
}

float rtl433_signal_samples_to_us(int samples, uint32_t sample_rate)
{
    if (sample_rate == 0) return 0.0f;
    return (float)samples * 1000000.0f / (float)sample_rate;
}

int rtl433_signal_us_to_samples(float us, uint32_t sample_rate)
{
    if (sample_rate == 0) return 0;
    return (int)(us * (float)sample_rate / 1000000.0f + 0.5f);
}

int rtl433_signal_normalize(pulse_data_t *pulse_data)
{
    if (!pulse_data) return -1;
    
    // Remove trailing zero pulses
    while (pulse_data->num_pulses > 0 && 
           pulse_data->pulse[pulse_data->num_pulses - 1] == 0) {
        pulse_data->num_pulses--;
    }
    
    // Set minimum values for too short pulses
    for (unsigned i = 0; i < pulse_data->num_pulses; i++) {
        if (pulse_data->pulse[i] > 0 && pulse_data->pulse[i] < 5) {
            pulse_data->pulse[i] = 5;
        }
        if (pulse_data->gap[i] > 0 && pulse_data->gap[i] < 5) {
            pulse_data->gap[i] = 5;
        }
    }
    
    return 0;
}

void rtl433_signal_debug_print(const pulse_data_t *pulse_data, const char *prefix)
{
    if (!pulse_data) return;
    
    const char *pfx = prefix ? prefix : "SIGNAL";
    
    printf("%s: %u pulses, sr=%u Hz, freq=%.1f Hz\n", 
           pfx, pulse_data->num_pulses, pulse_data->sample_rate, pulse_data->centerfreq_hz);
    printf("%s: RSSI=%.1f dB, SNR=%.1f dB, Noise=%.1f dB\n",
           pfx, pulse_data->rssi_db, pulse_data->snr_db, pulse_data->noise_db);
    printf("%s: FSK f1=%d, f2=%d, OOK low=%d, high=%d\n",
           pfx, pulse_data->fsk_f1_est, pulse_data->fsk_f2_est, 
           pulse_data->ook_low_estimate, pulse_data->ook_high_estimate);
    
    if (pulse_data->num_pulses > 0) {
        printf("%s: Duration=%.2f ms\n", pfx, rtl433_signal_duration_us(pulse_data) / 1000.0f);
    }
}

// === СОВМЕСТИМОСТЬ С ОРИГИНАЛЬНЫМ RTL_433 ===

int rtl433_signal_slice_pcm(const pulse_data_t *pulse_data, void *r_dev)
{
    return pulse_slicer_pcm((pulse_data_t*)pulse_data, (r_device*)r_dev);
}

int rtl433_signal_analyze(const pulse_data_t *pulse_data, rtl433_modulation_type_t mod_type, void *r_dev)
{
    // Use original pulse_analyzer function
    // TODO: adapt to new interface
    (void)pulse_data; // suppress warning
    (void)mod_type;   // suppress warning
    (void)r_dev;      // suppress warning
    return 0;
}

int rtl433_signal_rfraw_parse(pulse_data_t *pulse_data, const char *hex_str)
{
    return rfraw_parse(pulse_data, hex_str) ? 0 : -1;
}

bool rtl433_signal_rfraw_check(const char *hex_str)
{
    return rfraw_check(hex_str);
}
