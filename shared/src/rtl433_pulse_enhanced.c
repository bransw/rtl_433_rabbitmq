/** @file
 * Enhanced pulse data functions for RabbitMQ transport with hex_string generation
 * 
 * This file contains copies and enhancements of original rtl_433 functions
 * to provide enhanced JSON output with hex_string for RabbitMQ transport.
 */

#include "rtl433_pulse_enhanced.h"
#include "rtl433_rfraw.h"
#include "pulse_data.h"
#include "data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Enhanced version of pulse_data_print_data with hex_string generation
 * This is a copy of the original function with added hex_string/triq_url fields
 */
data_t *rtl433_pulse_data_print_data_enhanced(pulse_data_t const *data)
{
    if (!data) return NULL;
    
    static int msg_id = 0;
    msg_id++;
    
    int pulses[2 * PD_MAX_PULSES];
    double to_us = 1e6 / data->sample_rate;
    for (unsigned i = 0; i < data->num_pulses; ++i) {
        pulses[i * 2 + 0] = data->pulse[i] * to_us;
        pulses[i * 2 + 1] = data->gap[i] * to_us;
    }
    
    // Generate hex_string for enhanced output (NULL-safe)
    char *hex_string = rtl433_rfraw_generate_hex_string(data);
    
    // Early exit for complex signals that can't generate hex_string
    if (!hex_string) {
        return NULL; // This will trigger the original pulse_data_print_data
    }

    /* clang-format off */
    data_t *enhanced_data = data_make(
            "msg_id",           "", DATA_INT,    msg_id,
            "mod",              "", DATA_STRING, (data->fsk_f2_est) ? "FSK" : "OOK",
            "count",            "", DATA_INT,    data->num_pulses,
            "pulses",           "", DATA_ARRAY,  data_array(2 * data->num_pulses, DATA_INT, pulses),
            "freq1_Hz",         "", DATA_FORMAT, "%u Hz", DATA_INT, (unsigned)data->freq1_hz,
            "freq2_Hz",         "", DATA_COND,   data->fsk_f2_est, DATA_FORMAT, "%u Hz", DATA_INT, (unsigned)data->freq2_hz,
            "freq_Hz",          "", DATA_INT,    (unsigned)data->centerfreq_hz,
            "rate_Hz",          "", DATA_INT,    data->sample_rate,
            "depth_bits",       "", DATA_INT,    data->depth_bits,
            "range_dB",         "", DATA_FORMAT, "%.1f dB", DATA_DOUBLE, data->range_db,
            "rssi_dB",          "", DATA_FORMAT, "%.1f dB", DATA_DOUBLE, data->rssi_db,
            "snr_dB",           "", DATA_FORMAT, "%.1f dB", DATA_DOUBLE, data->snr_db,
            "noise_dB",         "", DATA_FORMAT, "%.1f dB", DATA_DOUBLE, data->noise_db,
            // Additional critical fields for complete signal reconstruction
            "offset",           "", DATA_FORMAT, "%llu", DATA_INT, (unsigned long long)data->offset,
            "start_ago",        "", DATA_INT,    data->start_ago,
            "end_ago",          "", DATA_INT,    data->end_ago,
            "ook_low_estimate", "", DATA_INT,    data->ook_low_estimate,
            "ook_high_estimate","", DATA_INT,    data->ook_high_estimate,
            "fsk_f1_est",       "", DATA_INT,    data->fsk_f1_est,
            "fsk_f2_est_value", "", DATA_INT,    data->fsk_f2_est,
            // Enhanced field with hex_string (URL removed as requested)
            "hex_string",       "", DATA_STRING, hex_string,
            NULL);
    /* clang-format on */
    
    // Cleanup allocated strings (data_make copies the strings)
    if (hex_string) free(hex_string);
    
    return enhanced_data;
}

/**
 * Enhanced JSON generation from pulse_data_t with hex_string
 */
char* rtl433_pulse_data_to_enhanced_json_string(pulse_data_t const *data)
{
    if (!data) return NULL;
    
    data_t *enhanced_data = rtl433_pulse_data_print_data_enhanced(data);
    if (!enhanced_data) {
        return NULL;
    }
    
    // Get required buffer size first
    size_t required_size = data_print_jsons(enhanced_data, NULL, 0);
    if (required_size == 0) {
        data_free(enhanced_data);
        return NULL;
    }
    
    // Allocate buffer and generate JSON
    char *json_str = malloc(required_size + 1);
    if (!json_str) {
        data_free(enhanced_data);
        return NULL;
    }
    
    data_print_jsons(enhanced_data, json_str, required_size + 1);
    data_free(enhanced_data);
    
    return json_str;
}
