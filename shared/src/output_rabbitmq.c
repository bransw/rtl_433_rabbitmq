/** @file
    RabbitMQ output for rtl_433 events

    Copyright (C) 2025 RTL_433 Remote Project

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "output_rabbitmq.h"
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

/* RabbitMQ client abstraction */

typedef struct {
    struct data_output output;
    rtl433_transport_connection_t conn;
    rtl433_transport_config_t config;
    char hostname[64];
    char *signals_queue;
    char *detected_queue;
    char *base_topic;
    int connected;
} data_output_rabbitmq_t;

static void R_API_CALLCONV print_rabbitmq_array(data_output_t *output, data_array_t *array, char const *format)
{
    UNUSED(format);
    // For now, just ignore arrays or handle them later
    // This prevents segfaults during initial testing
    UNUSED(output);
    UNUSED(array);
}

static void R_API_CALLCONV print_rabbitmq_data(data_output_t *output, data_t *data, char const *format)
{
    UNUSED(format);
    
    if (!output || !data) {
        return;
    }
    
    data_output_rabbitmq_t *rabbitmq = (data_output_rabbitmq_t *)output;
    
    if (!rabbitmq || !rabbitmq->connected) {
        return;
    }
    
    // Clean approach: both signals and detected devices have time field for correlation
    
    // Note: Time-based correlation is preferred over msg_id for signal-device matching
    // All messages (both signals and detected devices) will have 'time' field for correlation
    
    // Check if this has data we want to send
    data_t *data_model = NULL;
    data_t *data_mod = NULL;
    for (data_t *d = data; d; d = d->next) {
        if (!strcmp(d->key, "model"))
            data_model = d;
        if (!strcmp(d->key, "mod"))
            data_mod = d;
    }
    
    // Route data based on type
    if (data_model || data_mod) {
        char json_buffer[8192];  // Larger buffer for combined data
        const char *target_queue = NULL;
        
        if (data_model) {
            // This is decoded device data - send clean device information
            
            data_print_jsons(data, json_buffer, sizeof(json_buffer));
            
            // Clean approach: send only device data without pulse analysis
            
            target_queue = rabbitmq->detected_queue;
            print_logf(LOG_DEBUG, "RabbitMQ", "Sending device data to 'detected': %.100s...", json_buffer);
            
        } else if (data_mod) {
            // This is raw pulse data - send to 'signals' queue
            // Try to use enhanced JSON if we have pulse_data structure available
            pulse_data_t *pulse_data = NULL;
            (void)pulse_data; // Mark as used to avoid warning
            
            // Look for pulse_data in the data chain
            for (data_t *d = data; d; d = d->next) {
                if (!strcmp(d->key, "pulses") && d->type == DATA_ARRAY) {
                    // This appears to be pulse data, but we need the actual pulse_data_t structure
                    // For now, fall back to standard JSON
                    break;
                }
            }
            
            // For now, use standard JSON until we can properly extract pulse_data_t
            data_print_jsons(data, json_buffer, sizeof(json_buffer));
            target_queue = rabbitmq->signals_queue;
            print_logf(LOG_DEBUG, "RabbitMQ", "Sending raw pulse data to 'signals': %.100s...", json_buffer);
        }
        
        // Send raw JSON directly instead of converting to rtl433_message_t
        // This preserves the original data format
        if (rtl433_transport_send_raw_json_to_queue(&rabbitmq->conn, json_buffer, target_queue) != 0) {
            // Fallback: if raw JSON sending fails, use the old method
            rtl433_message_t *message = rtl433_message_create_from_json(json_buffer);
            if (message) {
                rtl433_transport_send_message_to_queue(&rabbitmq->conn, message, target_queue);
                rtl433_message_free(message);
            }
        }
    }
}

static void R_API_CALLCONV print_rabbitmq_string(data_output_t *output, char const *str, char const *format)
{
    UNUSED(format);
    
    if (!output || !str) {
        return;
    }
    
    data_output_rabbitmq_t *rabbitmq = (data_output_rabbitmq_t *)output;
    
    if (!rabbitmq || !rabbitmq->connected) {
        return;
    }
    
    // Create message from string and send to detected queue
    rtl433_message_t *message = rtl433_message_create_from_json(str);
    if (message) {
        rtl433_transport_send_message_to_queue(&rabbitmq->conn, message, rabbitmq->detected_queue);
        rtl433_message_free(message);
    }
}

static void R_API_CALLCONV print_rabbitmq_double(data_output_t *output, double data, char const *format)
{
    char str[20];
    // use scientific notation for very big/small values
    if (data > 1e7 || data < 1e-4) {
        snprintf(str, sizeof(str), "%g", data);
    }
    else {
        int ret = snprintf(str, sizeof(str), "%.5f", data);
        // remove trailing zeros, always keep one digit after the decimal point
        char *p = str + ret - 1;
        while (*p == '0' && p[-1] != '.') {
            *p-- = '\0';
        }
    }
    
    print_rabbitmq_string(output, str, format);
}

static void R_API_CALLCONV print_rabbitmq_int(data_output_t *output, int data, char const *format)
{
    char str[20];
    snprintf(str, sizeof(str), "%d", data);
    print_rabbitmq_string(output, str, format);
}

static void R_API_CALLCONV data_output_rabbitmq_free(data_output_t *output)
{
    data_output_rabbitmq_t *rabbitmq = (data_output_rabbitmq_t *)output;
    
    if (!rabbitmq)
        return;
    
    if (rabbitmq->connected) {
        rtl433_transport_disconnect(&rabbitmq->conn);
    }
    
    free(rabbitmq->signals_queue);
    free(rabbitmq->detected_queue);
    free(rabbitmq->base_topic);
    
    free(rabbitmq);
}

void add_rabbitmq_output(r_cfg_t *cfg, char *param)
{
    list_push(&cfg->output_handler, data_output_rabbitmq_create(get_mgr(cfg), param, cfg->dev_query));
}

struct data_output *data_output_rabbitmq_create(struct mg_mgr *mgr, char *param, char const *dev_hint)
{
    UNUSED(mgr); // We don't use mongoose for RabbitMQ
    UNUSED(dev_hint); // We don't use dev_hint for now
    
    print_logf(LOG_DEBUG, "RabbitMQ", "Creating RabbitMQ output with param: %s", param ? param : "(null)");
    
    data_output_rabbitmq_t *rabbitmq = calloc(1, sizeof(data_output_rabbitmq_t));
    if (!rabbitmq)
        FATAL_CALLOC("data_output_rabbitmq_create()");
    
    // Initialize all fields to safe values
    rabbitmq->connected = 0;
    rabbitmq->signals_queue = NULL;
    rabbitmq->detected_queue = NULL;
    rabbitmq->base_topic = NULL;
    
    gethostname(rabbitmq->hostname, sizeof(rabbitmq->hostname) - 1);
    rabbitmq->hostname[sizeof(rabbitmq->hostname) - 1] = '\0';
    // only use hostname, not domain part
    char *dot = strchr(rabbitmq->hostname, '.');
    if (dot)
        *dot = '\0';
    
    // Parse RabbitMQ URL from param (e.g., "rabbitmq://guest:guest@localhost:5672/rtl_433")
    char url[512];
    if (param && strncmp(param, "rabbitmq://", 11) == 0) {
        strncpy(url, param, sizeof(url) - 1);
        url[sizeof(url) - 1] = '\0';
        
        // Convert rabbitmq:// to amqp://
        memmove(url, url + 8, strlen(url + 8) + 1); // Remove "rabbitmq"
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
            print_logf(LOG_DEBUG, "RabbitMQ", "Exchange name extracted: %s", exchange);
            
            // Store the exchange name for later use
            if (rabbitmq->base_topic) {
                free(rabbitmq->base_topic);
            }
            rabbitmq->base_topic = strdup(exchange);
        }
    } else {
        // Default URL with default vhost
        strcpy(url, "amqp://guest:guest@localhost:5672/");
    }
    
    print_logf(LOG_DEBUG, "RabbitMQ", "Parsed URL: %s", url);
    
    // Parse the URL
    if (rtl433_transport_parse_url(url, &rabbitmq->config) != 0) {
        print_logf(LOG_ERROR, "RabbitMQ", "Failed to parse RabbitMQ URL: %s", url);
        free(rabbitmq);
        return NULL;
    }
    
    // Override exchange name with extracted value if we have one
    if (rabbitmq->base_topic && strlen(rabbitmq->base_topic) > 0) {
        if (rabbitmq->config.exchange) {
            free(rabbitmq->config.exchange);
        }
        rabbitmq->config.exchange = strdup(rabbitmq->base_topic);
        print_logf(LOG_DEBUG, "RabbitMQ", "Using exchange: %s", rabbitmq->config.exchange);
    }
    
    // Try to connect to RabbitMQ
    print_logf(LOG_DEBUG, "RabbitMQ", "Attempting to connect...");
    
    if (rtl433_transport_connect(&rabbitmq->conn, &rabbitmq->config) != 0) {
        print_logf(LOG_ERROR, "RabbitMQ", "Failed to connect to RabbitMQ: %s", url);
        // Don't return NULL, just mark as disconnected
        // This allows the program to continue without crashing
        rabbitmq->connected = 0;
    } else {
        rabbitmq->connected = 1;
        print_logf(LOG_NOTICE, "RabbitMQ", "Connected to RabbitMQ: %s", url);
    }
    
    // Set default queue names (always set these, even if not connected)
    rabbitmq->signals_queue = strdup("signals");
    rabbitmq->detected_queue = strdup("detected");
    rabbitmq->base_topic = strdup("rtl_433");
    
    if (!rabbitmq->signals_queue || !rabbitmq->detected_queue || !rabbitmq->base_topic) {
        print_logf(LOG_ERROR, "RabbitMQ", "Failed to allocate queue names");
        data_output_rabbitmq_free((data_output_t*)rabbitmq);
        return NULL;
    }
    
    // Setup output functions
    rabbitmq->output.print_data   = print_rabbitmq_data;
    rabbitmq->output.print_array  = print_rabbitmq_array;
    rabbitmq->output.print_string = print_rabbitmq_string;
    rabbitmq->output.print_double = print_rabbitmq_double;
    rabbitmq->output.print_int    = print_rabbitmq_int;
    rabbitmq->output.output_free  = data_output_rabbitmq_free;
    
    print_logf(LOG_DEBUG, "RabbitMQ", "RabbitMQ output created successfully");
    
    return (struct data_output *)rabbitmq;
}

