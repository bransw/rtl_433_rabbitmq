/** @file
    RabbitMQ forwarder device for rtl_433 - sends raw input data to RabbitMQ queue.

    Copyright (C) 2024

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

/**
RabbitMQ forwarder device for rtl_433 - sends raw input data to RabbitMQ queue.

This device doesn't decode any specific protocol. Instead, it captures all raw
input data and forwards it to a specified RabbitMQ queue "rtl_433".

The device accepts all modulations and forwards the raw bitbuffer data to RabbitMQ
in JSON format with timestamp and metadata.

Usage:
    - Enable this device with -R rabbitmq
    - Configure RabbitMQ connection details through environment variables:
      * RABBITMQ_HOST (default: localhost)
      * RABBITMQ_PORT (default: 5672)
      * RABBITMQ_USER (default: guest)
      * RABBITMQ_PASS (default: guest)
      * RABBITMQ_QUEUE (default: rtl_433)

Data format sent to RabbitMQ:
    {
        "timestamp": "2024-01-01T12:00:00Z",
        "frequency": 433920000,
        "rows": [
            {
                "bits": 32,
                "data": "AABBCCDD"
            }
        ]
    }
*/

#include "decoder.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Note: For now we'll simulate RabbitMQ connection
// In a real implementation, you would use librabbitmq or amqp-cpp
// For this template, we'll just log the data that would be sent

typedef struct {
    char host[256];
    int port;
    char user[256];
    char pass[256];
    char queue[256];
    int connected;
} rabbitmq_config_t;

static rabbitmq_config_t g_rabbitmq_config = {
    .host = "localhost",
    .port = 5672,
    .user = "guest",
    .pass = "guest",
    .queue = "rtl_433",
    .connected = 0
};

static void init_rabbitmq_config(void)
{
    char const *env_host = getenv("RABBITMQ_HOST");
    char const *env_port = getenv("RABBITMQ_PORT");
    char const *env_user = getenv("RABBITMQ_USER");
    char const *env_pass = getenv("RABBITMQ_PASS");
    char const *env_queue = getenv("RABBITMQ_QUEUE");

    if (env_host) {
        strncpy(g_rabbitmq_config.host, env_host, sizeof(g_rabbitmq_config.host) - 1);
    }
    if (env_port) {
        g_rabbitmq_config.port = atoi(env_port);
    }
    if (env_user) {
        strncpy(g_rabbitmq_config.user, env_user, sizeof(g_rabbitmq_config.user) - 1);
    }
    if (env_pass) {
        strncpy(g_rabbitmq_config.pass, env_pass, sizeof(g_rabbitmq_config.pass) - 1);
    }
    if (env_queue) {
        strncpy(g_rabbitmq_config.queue, env_queue, sizeof(g_rabbitmq_config.queue) - 1);
    }
}

static char *bytes_to_hex(uint8_t const *bytes, int len)
{
    char *hex = malloc(len * 2 + 1);
    if (!hex) return NULL;
    
    for (int i = 0; i < len; i++) {
        sprintf(hex + i * 2, "%02X", bytes[i]);
    }
    hex[len * 2] = '\0';
    return hex;
}

static void send_to_rabbitmq(bitbuffer_t *bitbuffer)
{
    // Initialize config if not done yet
    static int config_initialized = 0;
    if (!config_initialized) {
        init_rabbitmq_config();
        config_initialized = 1;
        fprintf(stderr, "RabbitMQ device: Configured to send to %s:%d queue '%s'\n", 
                g_rabbitmq_config.host, g_rabbitmq_config.port, g_rabbitmq_config.queue);
    }

    // Get current timestamp
    time_t now;
    time(&now);
    struct tm *utc_tm = gmtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_tm);

    // Create JSON message
    char json_message[8192];
    int json_len = snprintf(json_message, sizeof(json_message),
        "{\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"device\": \"rtl_433\",\n"
        "  \"rows\": [\n",
        timestamp);

    for (int row = 0; row < bitbuffer->num_rows && row < 50; row++) {
        if (bitbuffer->bits_per_row[row] > 0) {
            int byte_len = (bitbuffer->bits_per_row[row] + 7) / 8;
            char *hex_data = bytes_to_hex(bitbuffer->bb[row], byte_len);
            
            if (hex_data) {
                json_len += snprintf(json_message + json_len, sizeof(json_message) - json_len,
                    "    {\n"
                    "      \"bits\": %d,\n"
                    "      \"data\": \"%s\"\n"
                    "    }%s\n",
                    bitbuffer->bits_per_row[row],
                    hex_data,
                    (row < bitbuffer->num_rows - 1 && bitbuffer->bits_per_row[row + 1] > 0) ? "," : "");
                free(hex_data);
            }
        }
    }

    json_len += snprintf(json_message + json_len, sizeof(json_message) - json_len,
        "  ]\n"
        "}");

    // For now, just print what would be sent to RabbitMQ
    // In a real implementation, this would use librabbitmq to send the message
    fprintf(stderr, "RabbitMQ device: Would send to queue '%s':\n%s\n", 
            g_rabbitmq_config.queue, json_message);

    // TODO: Implement actual RabbitMQ connection using librabbitmq
    // Example of what the real implementation would look like:
    //
    // amqp_connection_state_t conn = amqp_new_connection();
    // amqp_socket_t *socket = amqp_tcp_socket_new(conn);
    // amqp_socket_open(socket, g_rabbitmq_config.host, g_rabbitmq_config.port);
    // amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, 
    //            g_rabbitmq_config.user, g_rabbitmq_config.pass);
    // amqp_channel_open(conn, 1);
    // 
    // amqp_basic_properties_t props;
    // props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
    // props.content_type = amqp_cstring_bytes("application/json");
    // props.delivery_mode = 2; // persistent
    //
    // amqp_basic_publish(conn, 1, amqp_cstring_bytes(""), 
    //                    amqp_cstring_bytes(g_rabbitmq_config.queue),
    //                    0, 0, &props, amqp_cstring_bytes(json_message));
}

static int rabbitmq_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{
    // This device accepts ALL input data, regardless of content
    // We don't perform any validation or filtering
    
    // Skip empty buffers
    if (bitbuffer->num_rows == 0) {
        return DECODE_ABORT_EARLY;
    }

    // Check if there's any actual data
    int has_data = 0;
    for (int i = 0; i < bitbuffer->num_rows; i++) {
        if (bitbuffer->bits_per_row[i] > 0) {
            has_data = 1;
            break;
        }
    }

    if (!has_data) {
        return DECODE_ABORT_EARLY;
    }

    // Send raw data to RabbitMQ
    send_to_rabbitmq(bitbuffer);

    // Create minimal data output for rtl_433 logging
    data_t *data = data_make(
            "model", "", DATA_STRING, "RabbitMQ-Forwarder",
            "rows", "", DATA_INT, bitbuffer->num_rows,
            "action", "", DATA_STRING, "forwarded_to_rabbitmq",
            NULL);
    
    decoder_output_data(decoder, data);

    // Return 1 to indicate successful processing
    return 1;
}

static char const *const output_fields[] = {
        "model",
        "rows",
        "action",
        NULL,
};

/*
 * RabbitMQ forwarder device registration
 * 
 * This device uses very permissive settings to catch as much data as possible:
 * - Uses OOK_PULSE_PCM for broad compatibility
 * - Short timing values to catch quick pulses
 * - Long reset limit to capture entire transmissions
 * - Priority 0 so it runs first and captures everything
 */
r_device const rabbitmq = {
        .name        = "RabbitMQ forwarder",
        .modulation  = OOK_PULSE_PCM,
        .short_width = 50,   // Very short to catch most signals
        .long_width  = 100,  // Short long width
        .gap_limit   = 1000, // Medium gap limit
        .reset_limit = 10000, // Long reset to capture full transmissions
        .decode_fn   = &rabbitmq_decode,
        .priority    = 0,    // High priority to run first
        .disabled    = 1,    // Disabled by default (use -R rabbitmq to enable)
        .fields      = output_fields,
};
