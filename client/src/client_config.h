/** @file
    Configuration for rtl_433_client.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

#include <stdint.h>
#include "list.h"

/// Transport types for data transmission
typedef enum {
    TRANSPORT_HTTP,
    TRANSPORT_MQTT,
    TRANSPORT_RABBITMQ,
    TRANSPORT_TCP,
    TRANSPORT_UDP
} transport_type_t;

/// Transport configuration
typedef struct transport_config {
    transport_type_t type;
    char *host;
    int port;
    char *username;
    char *password;
    char *topic_queue;  // MQTT topic or RabbitMQ queue
    char *exchange;     // for RabbitMQ
    int ssl_enabled;
    char *ca_cert_path;
    char *client_cert_path;
    char *client_key_path;
} transport_config_t;

/// Client configuration
typedef struct client_config {
    // SDR device configuration
    char *device_query;
    char *gain_str;
    char *settings_str;
    int ppm_error;
    uint32_t frequency;
    uint32_t sample_rate;
    
    // Demodulation settings
    int ook_detect_mode;
    int fsk_detect_mode;
    float level_limit;
    float min_level;
    float min_snr;
    int analyze_pulses;
    
    // Transport
    transport_config_t transport;
    
    // Input files
    list_t input_files;
    
    // Logging
    int verbosity;
    char *log_file;
    
    // Operation modes
    int duration;           // Run time in seconds (0 = infinite)
    int raw_mode;           // Send raw pulse_data instead of processed
    int batch_mode;         // Accumulate signals in batches
    int batch_size;         // Batch size
    int batch_timeout_ms;   // Batch timeout in milliseconds
} client_config_t;

/// Initialize client configuration with default values
void client_config_init(client_config_t *cfg);

/// Free configuration memory
void client_config_free(client_config_t *cfg);

/// Load configuration from file
int client_config_load_file(client_config_t *cfg, const char *filename);

/// Parse command line arguments
int client_config_parse_args(client_config_t *cfg, int argc, char **argv);

/// Print help
void client_config_print_help(const char *program_name);

/// Validate configuration
int client_config_validate(const client_config_t *cfg);

#endif // CLIENT_CONFIG_H
