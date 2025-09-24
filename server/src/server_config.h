/** @file
    Configuration for rtl_433_server.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <stdint.h>
#include "list.h"

/// Input transport types
typedef enum {
    INPUT_TRANSPORT_HTTP,
    INPUT_TRANSPORT_MQTT,
    INPUT_TRANSPORT_RABBITMQ,
    INPUT_TRANSPORT_TCP,
    INPUT_TRANSPORT_UDP
} input_transport_type_t;

/// Output queue types
typedef enum {
    OUTPUT_QUEUE_MQTT,
    OUTPUT_QUEUE_RABBITMQ,
    OUTPUT_QUEUE_HTTP,
    OUTPUT_QUEUE_FILE,
    OUTPUT_QUEUE_DATABASE,
    OUTPUT_QUEUE_WEBSOCKET
} output_queue_type_t;

/// Input transport configuration
typedef struct input_transport_config {
    input_transport_type_t type;
    char *host;
    int port;
    char *username;
    char *password;
    char *topic_queue;
    char *exchange;
    int ssl_enabled;
    char *ca_cert_path;
    char *client_cert_path;
    char *client_key_path;
    int max_connections;
    int timeout_ms;
} input_transport_config_t;

/// Output queue configuration
typedef struct output_queue_config {
    output_queue_type_t type;
    char *name;
    char *host;
    int port;
    char *username;
    char *password;
    char *topic_queue;
    char *exchange;
    char *routing_key;
    char *file_path;
    char *db_path;
    int ssl_enabled;
    char *ca_cert_path;
    char *client_cert_path;
    char *client_key_path;
    int batch_size;
    int batch_timeout_ms;
    int max_queue_size;
} output_queue_config_t;

/// Device decoder configuration
typedef struct decoder_config {
    int device_id;
    char *device_name;
    int enabled;
    int priority;
    char *parameters;  // Additional parameters for decoder
} decoder_config_t;

/// Server configuration
typedef struct server_config {
    // Input transports
    list_t input_transports;
    
    // Output queues
    output_queue_config_t ready_queue;      // Queue for recognized devices
    output_queue_config_t unknown_queue;    // Queue for unknown signals
    
    // Device decoders
    list_t decoders;
    
    // Processing settings
    int max_concurrent_signals;    // Maximum number of concurrently processed signals
    int signal_timeout_ms;         // Timeout for processing one signal
    int batch_processing;          // Enable batch processing
    int batch_size;                // Batch size
    int batch_timeout_ms;          // Batch timeout
    
    // Database settings for unknown signals
    char *unknown_signals_db_path;
    int db_retention_days;         // Retention time for unknown signals
    int db_max_size_mb;            // Maximum database size
    
    // Web interface
    int web_enabled;
    char *web_host;
    int web_port;
    char *web_static_path;
    
    // Logging
    int verbosity;
    char *log_file;
    int log_rotation;
    int log_max_size_mb;
    int log_max_files;
    
    // Monitoring and statistics
    int stats_enabled;
    int stats_interval_sec;
    char *stats_output_file;
    
    // Operating modes
    int daemon_mode;
    char *pid_file;
    char *user;
    char *group;
    
    // Performance
    int worker_threads;            // Number of worker threads
    int io_threads;                // Number of I/O threads
    int queue_buffer_size;         // Queue buffer size
} server_config_t;

/// Initialize server configuration with default values
void server_config_init(server_config_t *cfg);

/// Free configuration memory
void server_config_free(server_config_t *cfg);

/// Load configuration from file
int server_config_load_file(server_config_t *cfg, const char *filename);

/// Parse command line arguments
int server_config_parse_args(server_config_t *cfg, int argc, char **argv);

/// Print help
void server_config_print_help(const char *program_name);

/// Configuration validation
int server_config_validate(const server_config_t *cfg);

/// Add input transport
int server_config_add_input_transport(server_config_t *cfg, const input_transport_config_t *transport);

/// Add device decoder
int server_config_add_decoder(server_config_t *cfg, const decoder_config_t *decoder);

/// Find decoder configuration by ID
decoder_config_t *server_config_find_decoder(const server_config_t *cfg, int device_id);

/// Load decoders from configuration file
int server_config_load_decoders(server_config_t *cfg, const char *filename);

/// Save configuration to file
int server_config_save_file(const server_config_t *cfg, const char *filename);

#endif // SERVER_CONFIG_H

