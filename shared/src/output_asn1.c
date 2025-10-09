/** @file
    ASN.1 output for rtl_433 events using binary encoding

    Copyright (C) 2025 RTL_433 Remote Project

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "output_asn1.h"
#include "optparse.h"
#include "bit_util.h"
#include "logger.h"
#include "fatal.h"
#include "r_util.h"
#include "rtl_433.h"
#include "r_api.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>  // for gethostname

// Include our transport library
#include "rtl433_transport.h"
#include "rtl433_config.h"

// Include ASN.1 support
#include "rtl433_asn1_pulse.h"

// Global variable to access raw_mode from client
extern int g_rtl433_raw_mode;

/* ASN.1 RabbitMQ client abstraction */

typedef struct {
    struct data_output output;
    rtl433_transport_connection_t conn;
    rtl433_transport_config_t config;
    char hostname[64];
    char *signals_queue;
    char *detected_queue;
    char *base_topic;
    int connected;
    uint32_t package_counter;  // For package ID generation
} data_output_asn1_t;

static void R_API_CALLCONV print_asn1_array(data_output_t *output, data_array_t *array, char const *format)
{
    UNUSED(format);
    // For now, just ignore arrays or handle them later
    // This prevents segfaults during initial testing
    UNUSED(output);
    UNUSED(array);
}

static void R_API_CALLCONV print_asn1_data(data_output_t *output, data_t *data, char const *format)
{
    UNUSED(format);
    
    if (!output || !data) {
        return;
    }
    
    data_output_asn1_t *asn1_out = (data_output_asn1_t *)output;
    
    if (!asn1_out || !asn1_out->connected) {
        return;
    }

#ifndef ENABLE_ASN1
    print_logf(LOG_ERROR, "ASN1", "ASN.1 support not compiled in");
    return;
#endif
    
    // Check if this has data we want to send
    data_t *data_model = NULL;
    data_t *data_mod = NULL;
    for (data_t *d = data; d; d = d->next) {
        if (!strcmp(d->key, "model"))
            data_model = d;
        if (!strcmp(d->key, "mod"))
            data_mod = d;
    }
    
    
    
    // Route data based on type and -Q parameter
    if (data_model || data_mod) {
        const char *target_queue = NULL;
        rtl433_asn1_buffer_t asn1_buffer = {0};
        int should_send = 0;
        
        if (data_model) {
            // Skip detected messages (commented out for now)
        } else if (data_mod) {
            // This is raw pulse data - encode as SignalMessage
            target_queue = asn1_out->signals_queue;
            
            // Check -Q parameter: 0=both, 1=signals only, 2=detected only, 3=both
            should_send = (g_rtl433_raw_mode == 0 || g_rtl433_raw_mode == 1 || g_rtl433_raw_mode == 3);
            
            if (!should_send) {
                print_logf(LOG_DEBUG, "ASN1", "Skipping signal message due to -Q %d", g_rtl433_raw_mode);
                return;
            }
            
            // Create pulse_data_ex_t from available data fields
            pulse_data_ex_t pulse_ex;
            pulse_data_ex_init(&pulse_ex);
            
            // Set defaults
            pulse_ex.pulse_data.centerfreq_hz = 433920000;  // Default frequency
            pulse_ex.pulse_data.sample_rate = 250000;       // Default sample rate
            
            // Set package_id
            pulse_data_ex_set_package_id(&pulse_ex, ++asn1_out->package_counter);
            
            // Extract all fields from data_t in one pass
                for (data_t *d = data; d; d = d->next) {
                    if (strcmp(d->key, "freq_Hz") == 0 && d->type == DATA_INT) {
                    pulse_ex.pulse_data.centerfreq_hz = (double)d->value.v_int;
                    } else if (strcmp(d->key, "mod") == 0 && d->type == DATA_STRING) {
                    pulse_data_ex_set_modulation_type(&pulse_ex, (const char*)d->value.v_ptr);
                    } else if (strcmp(d->key, "time") == 0 && d->type == DATA_STRING) {
                    pulse_data_ex_set_timestamp_str(&pulse_ex, (const char*)d->value.v_ptr);
                } else if (strcmp(d->key, "hex_string") == 0 && d->type == DATA_STRING) {
                    pulse_data_ex_set_hex_string(&pulse_ex, (const char*)d->value.v_ptr);
                } else if (strcmp(d->key, "rate_Hz") == 0 && d->type == DATA_INT) {
                    pulse_ex.pulse_data.sample_rate = (uint32_t)d->value.v_int;
                } else if (strcmp(d->key, "count") == 0 && d->type == DATA_INT) {
                    pulse_ex.pulse_data.num_pulses = (uint32_t)d->value.v_int;
                    } else if (strcmp(d->key, "pulses") == 0 && d->type == DATA_ARRAY) {
                    // Handle pulses array - format: [pulse_μs, gap_μs, pulse_μs, gap_μs, ...]
                    // Values are in microseconds, need to convert to samples
                    data_array_t *pulses_array = (data_array_t*)d->value.v_ptr;
                    if (pulses_array && pulse_ex.pulse_data.num_pulses > 0 && pulse_ex.pulse_data.sample_rate > 0) {
                        int pulses_to_copy = (pulse_ex.pulse_data.num_pulses > PD_MAX_PULSES) ? 
                                            PD_MAX_PULSES : pulse_ex.pulse_data.num_pulses;
                        int *array_values = (int*)pulses_array->values;
                        double from_us = pulse_ex.pulse_data.sample_rate / 1e6; // Convert μs back to samples
                        
                        // Extract pulse and gap pairs from alternating array (values in microseconds)
                        for (int i = 0; i < pulses_to_copy; i++) {
                            int array_idx = i * 2;  // Each pulse-gap pair takes 2 elements
                            if (array_idx < pulses_array->num_values) {
                                double pulse_us = array_values[array_idx];
                                pulse_ex.pulse_data.pulse[i] = (int)(pulse_us * from_us);
                            }
                            if (array_idx + 1 < pulses_array->num_values) {
                                double gap_us = array_values[array_idx + 1];
                                pulse_ex.pulse_data.gap[i] = (int)(gap_us * from_us);
                            }
                        }
                    }
                }
                // Add signal quality fields
                else if (strcmp(d->key, "rssi_dB") == 0 && d->type == DATA_DOUBLE) {
                    pulse_ex.pulse_data.rssi_db = (float)d->value.v_dbl;
                } else if (strcmp(d->key, "snr_dB") == 0 && d->type == DATA_DOUBLE) {
                    pulse_ex.pulse_data.snr_db = (float)d->value.v_dbl;
                } else if (strcmp(d->key, "noise_dB") == 0 && d->type == DATA_DOUBLE) {
                    pulse_ex.pulse_data.noise_db = (float)d->value.v_dbl;
                } else if (strcmp(d->key, "range_dB") == 0 && d->type == DATA_DOUBLE) {
                    pulse_ex.pulse_data.range_db = (float)d->value.v_dbl;
                } else if (strcmp(d->key, "depth_bits") == 0 && d->type == DATA_INT) {
                    pulse_ex.pulse_data.depth_bits = (unsigned)d->value.v_int;
                }
                // Add timing info fields
                else if (strcmp(d->key, "ook_low_estimate") == 0 && d->type == DATA_INT) {
                    pulse_ex.pulse_data.ook_low_estimate = d->value.v_int;
                } else if (strcmp(d->key, "ook_high_estimate") == 0 && d->type == DATA_INT) {
                    pulse_ex.pulse_data.ook_high_estimate = d->value.v_int;
                } else if (strcmp(d->key, "fsk_f1_est") == 0 && d->type == DATA_INT) {
                    pulse_ex.pulse_data.fsk_f1_est = d->value.v_int;
                } else if (strcmp(d->key, "fsk_f2_est") == 0 && d->type == DATA_INT) {
                    pulse_ex.pulse_data.fsk_f2_est = d->value.v_int;
                }
                // Add additional fields
                else if (strcmp(d->key, "offset") == 0 && d->type == DATA_INT) {
                    pulse_ex.pulse_data.offset = (uint64_t)d->value.v_int;
                } else if (strcmp(d->key, "start_ago") == 0 && d->type == DATA_INT) {
                    pulse_ex.pulse_data.start_ago = (unsigned)d->value.v_int;
                } else if (strcmp(d->key, "end_ago") == 0 && d->type == DATA_INT) {
                    pulse_ex.pulse_data.end_ago = (unsigned)d->value.v_int;
                } else if (strcmp(d->key, "freq1_Hz") == 0 && d->type == DATA_INT) {
                    pulse_ex.pulse_data.freq1_hz = (float)d->value.v_int;
                } else if (strcmp(d->key, "freq2_Hz") == 0 && d->type == DATA_INT) {
                    pulse_ex.pulse_data.freq2_hz = (float)d->value.v_int;
                }
            }
            
            // Encode using rtl433_asn1_pulse (package_id automatically used)
            asn1_buffer = encode(&pulse_ex);
            
            // Cleanup
            pulse_data_ex_free(&pulse_ex);
            
            // ASN.1 encoding successful - will be sent below
        }
        
        // Send the ASN.1 encoded message
        if (asn1_buffer.result == RTL433_ASN1_OK && asn1_buffer.buffer) {
            // Send binary ASN.1 data with proper content type
            int result = rtl433_transport_send_binary_to_queue(&asn1_out->conn, 
                                                             asn1_buffer.buffer, 
                                                             asn1_buffer.buffer_size,
                                                             target_queue);
            
            if (result != 0) {
                print_logf(LOG_ERROR, "ASN1", "Failed to send ASN.1 message to %s", target_queue);
            }
            
            // Free the ASN.1 buffer
            if (asn1_buffer.buffer) {
                free(asn1_buffer.buffer);
            }
        } else {
            print_logf(LOG_ERROR, "ASN1", "Failed to encode ASN.1 message: %d", asn1_buffer.result);
        }
    }
}

static void R_API_CALLCONV print_asn1_double(data_output_t *output, double data, char const *format)
{
    UNUSED(output);
    UNUSED(data);
    UNUSED(format);
    // ASN.1 output doesn't handle individual values, only complete data structures
}

static void R_API_CALLCONV print_asn1_int(data_output_t *output, int data, char const *format)
{
    UNUSED(output);
    UNUSED(data);
    UNUSED(format);
    // ASN.1 output doesn't handle individual values, only complete data structures
}

static void R_API_CALLCONV print_asn1_string(data_output_t *output, char const *data, char const *format)
{
    UNUSED(output);
    UNUSED(data);
    UNUSED(format);
    // ASN.1 output doesn't handle individual values, only complete data structures
}

static void R_API_CALLCONV data_output_asn1_free(data_output_t *output)
{
    data_output_asn1_t *asn1_out = (data_output_asn1_t *)output;
    
    if (!asn1_out)
        return;
    
    if (asn1_out->connected) {
        rtl433_transport_disconnect(&asn1_out->conn);
    }
    
    free(asn1_out->signals_queue);
    free(asn1_out->detected_queue);
    free(asn1_out->base_topic);
    
    free(asn1_out);
}

void add_asn1_output(struct r_cfg *cfg, char *param)
{
    list_push(&cfg->output_handler, data_output_asn1_create(get_mgr(cfg), param, cfg->dev_query));
}

struct data_output *data_output_asn1_create(struct mg_mgr *mgr, char *param, char const *dev_hint)
{
    UNUSED(mgr); // We don't use mongoose for ASN.1 RabbitMQ
    UNUSED(dev_hint); // We don't use dev_hint for now
    
#ifndef ENABLE_ASN1
    print_logf(LOG_ERROR, "ASN1", "ASN.1 support not compiled in");
    return NULL;
#endif
    
    print_logf(LOG_DEBUG, "ASN1", "Creating ASN.1 output with param: %s", param ? param : "(null)");
    
    data_output_asn1_t *asn1_out = calloc(1, sizeof(data_output_asn1_t));
    if (!asn1_out)
        FATAL_CALLOC("data_output_asn1_create()");
    
    // Initialize all fields to safe values
    asn1_out->connected = 0;
    asn1_out->signals_queue = NULL;
    asn1_out->detected_queue = NULL;
    asn1_out->base_topic = NULL;
    asn1_out->package_counter = 0;
    
    gethostname(asn1_out->hostname, sizeof(asn1_out->hostname) - 1);
    asn1_out->hostname[sizeof(asn1_out->hostname) - 1] = '\0';
    // only use hostname, not domain part
    char *dot = strchr(asn1_out->hostname, '.');
    if (dot)
        *dot = '\0';
    
    // Parse ASN.1 URL from param (e.g., "asn1://guest:guest@localhost:5672/rtl_433")
    char url[512];
    if (param && strncmp(param, "asn1://", 7) == 0) {
        strncpy(url, param, sizeof(url) - 1);
        url[sizeof(url) - 1] = '\0';
        
        // Convert asn1:// to amqp://
        memmove(url, url + 4, strlen(url + 4) + 1); // Remove "asn1"
        memmove(url + 4, url, strlen(url) + 1);     // Make space for "amqp"
        memcpy(url, "amqp", 4);                     // Insert "amqp"
        
        // For RabbitMQ, the path after host:port should be exchange name, not vhost
        // But our transport expects vhost format, so we need to adjust
        // Convert amqp://user:pass@host:port/exchange to proper format
        char *slash = strrchr(url, '/');
        if (slash && strlen(slash) > 1) {
            // Extract exchange name and use default vhost
            char exchange[256];
            strncpy(exchange, slash + 1, sizeof(exchange) - 1);
            exchange[sizeof(exchange) - 1] = '\0';
            strcpy(slash, "/");  // Use default vhost "/"
            print_logf(LOG_DEBUG, "ASN1", "Exchange name extracted: %s", exchange);
            
            // Store the exchange name for later use
            if (asn1_out->base_topic) {
                free(asn1_out->base_topic);
            }
            asn1_out->base_topic = strdup(exchange);
        }
    } else {
        // Default URL with default vhost
        strcpy(url, "amqp://guest:guest@localhost:5672/");
    }
    
    print_logf(LOG_DEBUG, "ASN1", "Parsed URL: %s", url);
    
    // Parse the URL
    if (rtl433_transport_parse_url(url, &asn1_out->config) != 0) {
        print_logf(LOG_ERROR, "ASN1", "Failed to parse ASN.1 URL: %s", url);
        free(asn1_out);
        return NULL;
    }
    
    // Override exchange name with extracted value if we have one
    if (asn1_out->base_topic && strlen(asn1_out->base_topic) > 0) {
        if (asn1_out->config.exchange) {
            free(asn1_out->config.exchange);
        }
        asn1_out->config.exchange = strdup(asn1_out->base_topic);
        print_logf(LOG_DEBUG, "ASN1", "Using exchange: %s", asn1_out->config.exchange);
    }
    
    // Try to connect to RabbitMQ
    print_logf(LOG_DEBUG, "ASN1", "Attempting to connect...");
    
    if (rtl433_transport_connect(&asn1_out->conn, &asn1_out->config) != 0) {
        print_logf(LOG_ERROR, "ASN1", "Failed to connect to RabbitMQ: %s", url);
        // Don't return NULL, just mark as disconnected
        // This allows the program to continue without crashing
        asn1_out->connected = 0;
    } else {
        asn1_out->connected = 1;
        print_logf(LOG_NOTICE, "ASN1", "Connected to RabbitMQ for ASN.1 output: %s", url);
    }
    
    // Set ASN.1 specific queue names (always set these, even if not connected)
    asn1_out->signals_queue = strdup("asn1_signals");
    asn1_out->detected_queue = strdup("asn1_detected");
    asn1_out->base_topic = strdup("rtl_433");
    
    if (!asn1_out->signals_queue || !asn1_out->detected_queue || !asn1_out->base_topic) {
        print_logf(LOG_ERROR, "ASN1", "Failed to allocate queue names");
        data_output_asn1_free((data_output_t*)asn1_out);
        return NULL;
    }
    
    // Setup output functions
    asn1_out->output.print_data   = print_asn1_data;
    asn1_out->output.print_array  = print_asn1_array;
    asn1_out->output.print_string = print_asn1_string;
    asn1_out->output.print_double = print_asn1_double;
    asn1_out->output.print_int    = print_asn1_int;
    asn1_out->output.output_print = NULL; // Use print_data interface
    asn1_out->output.output_free  = data_output_asn1_free;
    
    print_logf(LOG_DEBUG, "ASN1", "ASN.1 output created successfully");
    
    return (struct data_output *)asn1_out;
}

