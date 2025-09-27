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
#include "rtl433_asn1.h"

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
    
    // Debug: Print what data we received
    print_logf(LOG_DEBUG, "ASN1", "Received data - model: %s, mod: %s", 
               data_model ? "YES" : "NO", data_mod ? "YES" : "NO");
    
    // Route data based on type and -Q parameter
    if (data_model || data_mod) {
        const char *target_queue = NULL;
        rtl433_asn1_buffer_t asn1_buffer = {0};
        int should_send = 0;
        
        if (data_mod && !data_model) {
            // This is raw pulse data (mod without model) - encode as SignalMessage
            target_queue = asn1_out->signals_queue;
            
            // Check -Q parameter: 0=both, 1=signals only, 2=detected only, 3=both
            should_send = (g_rtl433_raw_mode == 0 || g_rtl433_raw_mode == 1 || g_rtl433_raw_mode == 3);
            
            if (!should_send) {
                print_logf(LOG_DEBUG, "ASN1", "Skipping signal message due to -Q %d", g_rtl433_raw_mode);
                return;
            }
            
            // Try to get pulse_data if available
            pulse_data_t *pulse_data = NULL;
            
            // Look for pulse_data in the data structure
            for (data_t *d = data; d; d = d->next) {
                if (strcmp(d->key, "pulse_data") == 0 && d->type == DATA_DATA) {
                    pulse_data = (pulse_data_t*)d->value.v_ptr;
                    break;
                }
            }
            
            if (pulse_data) {
                // Use direct pulse_data encoding
                asn1_buffer = rtl433_asn1_encode_pulse_data_to_signal(
                    pulse_data,
                    ++asn1_out->package_counter
                );
                if (asn1_buffer.result == RTL433_ASN1_OK) {
                    print_logf(LOG_NOTICE, "ASN1", "Sending ASN.1 signal data to '%s' (pulse_data): %zu bytes", 
                              target_queue, asn1_buffer.buffer_size);
                }
            } else {
                // Fallback: extract basic signal parameters
                uint32_t frequency = 433920000;  // Default frequency
                int modulation = 0;  // Default to OOK
                
                // Extract frequency and modulation from data
                for (data_t *d = data; d; d = d->next) {
                    if (strcmp(d->key, "freq") == 0 && d->type == DATA_STRING) {
                        frequency = (uint32_t)atoi((const char*)d->value.v_ptr);
                    } else if (strcmp(d->key, "mod") == 0 && d->type == DATA_STRING) {
                        if (strcmp((const char*)d->value.v_ptr, "FSK") == 0) {
                            modulation = 1;  // FSK
                        }
                    }
                }
                
                // Use basic signal encoding (without actual signal data)
                asn1_buffer = rtl433_asn1_encode_signal(
                    ++asn1_out->package_counter,  // package_id
                    NULL,                         // timestamp
                    NULL, 0,                      // hex_string (none)
                    NULL, 0, 0,                   // pulses_data (none)
                    modulation,
                    frequency
                );
                if (asn1_buffer.result == RTL433_ASN1_OK) {
                    print_logf(LOG_NOTICE, "ASN1", "Sending ASN.1 signal data to '%s' (basic): %zu bytes", 
                              target_queue, asn1_buffer.buffer_size);
                }
            }
            
        } else if (data_mod) {
            // This is raw pulse data - encode as SignalMessage
            target_queue = asn1_out->signals_queue;
            
            // Check -Q parameter: 0=both, 1=signals only, 2=detected only, 3=both
            should_send = (g_rtl433_raw_mode == 0 || g_rtl433_raw_mode == 1 || g_rtl433_raw_mode == 3);
            
            if (!should_send) {
                print_logf(LOG_DEBUG, "ASN1", "Skipping signal message due to -Q %d", g_rtl433_raw_mode);
                return;
            }
            
            // Try to get pulse_data if available
            pulse_data_t *pulse_data = NULL;
            
            // Look for pulse_data in the data structure
            for (data_t *d = data; d; d = d->next) {
                if (strcmp(d->key, "pulse_data") == 0 && d->type == DATA_DATA) {
                    pulse_data = (pulse_data_t*)d->value.v_ptr;
                    break;
                }
            }
            
            if (pulse_data) {
                // Use direct pulse_data encoding
                asn1_buffer = rtl433_asn1_encode_pulse_data_to_signal(
                    pulse_data,
                    ++asn1_out->package_counter
                );
                if (asn1_buffer.result == RTL433_ASN1_OK) {
                    print_logf(LOG_NOTICE, "ASN1", "Sending ASN.1 signal data to '%s' (pulse_data): %zu bytes", 
                              target_queue, asn1_buffer.buffer_size);
                }
            } else {
                // Fallback: extract basic signal parameters
                uint32_t frequency = 433920000;  // Default frequency
                int modulation = 0;  // Default to OOK
                
                // Extract frequency and modulation from data
                for (data_t *d = data; d; d = d->next) {
                    if (strcmp(d->key, "freq") == 0 && d->type == DATA_STRING) {
                        frequency = (uint32_t)atoi((const char*)d->value.v_ptr);
                    } else if (strcmp(d->key, "mod") == 0 && d->type == DATA_STRING) {
                        if (strcmp((const char*)d->value.v_ptr, "FSK") == 0) {
                            modulation = 1;  // FSK
                        }
                    }
                }
                
                // Use basic signal encoding (without actual signal data)
                asn1_buffer = rtl433_asn1_encode_signal(
                    ++asn1_out->package_counter,  // package_id
                    NULL,                         // timestamp
                    NULL, 0,                      // hex_string (none)
                    NULL, 0, 0,                   // pulses_data (none)
                    modulation,
                    frequency
                );
            }
            
            if (asn1_buffer.result == RTL433_ASN1_OK) {
                print_logf(LOG_NOTICE, "ASN1", "Sending ASN.1 signal data to '%s': %zu bytes", 
                          target_queue, asn1_buffer.buffer_size);
            }
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
            rtl433_asn1_free_buffer(&asn1_buffer);
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
