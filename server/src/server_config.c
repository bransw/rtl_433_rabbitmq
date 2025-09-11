/** @file
    Server configuration implementation.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "server_config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Stub functions for missing dependencies
void set_log_level(int level) { (void)level; }
void samp_grab_write(void *data, size_t size) { (void)data; (void)size; }
int mg_connect_opt(void *mgr, const char *url, void *opts, void *cb) { (void)mgr; (void)url; (void)opts; (void)cb; return -1; }
void mg_send(void *conn, const void *data, int len) { (void)conn; (void)data; (void)len; }
void mbuf_remove(void *buf, size_t len) { (void)buf; (void)len; }
int rfraw_check(const char *data) { (void)data; return 0; }
int rfraw_parse(void *data, const char *input) { (void)data; (void)input; return 0; }
void mg_mgr_init(void *mgr, void *data) { (void)mgr; (void)data; }
void mg_mgr_free(void *mgr) { (void)mgr; }
void sdr_deactivate(void *sdr) { (void)sdr; }
void sdr_close(void *sdr) { (void)sdr; }
void raw_output_free(void *output) { (void)output; }
void* data_output_json_create(void *cfg) { (void)cfg; return NULL; }
void* data_output_csv_create(void *cfg) { (void)cfg; return NULL; }
void* data_output_log_create(void *cfg) { (void)cfg; return NULL; }
void* data_output_kv_create(void *cfg) { (void)cfg; return NULL; }
void* data_output_mqtt_create(void *cfg) { (void)cfg; return NULL; }
void* data_output_influx_create(void *cfg) { (void)cfg; return NULL; }
void* data_output_syslog_create(void *cfg) { (void)cfg; return NULL; }
void* data_output_http_create(void *cfg) { (void)cfg; return NULL; }
void* data_output_trigger_create(void *cfg) { (void)cfg; return NULL; }
void* raw_output_rtltcp_create(void *cfg) { (void)cfg; return NULL; }
void write_sigrok(void *data) { (void)data; }
void open_pulseview(void *data) { (void)data; }
void sdr_set_center_freq(void *sdr, uint32_t freq) { (void)sdr; (void)freq; }
void sdr_set_freq_correction(void *sdr, int ppm) { (void)sdr; (void)ppm; }
void sdr_set_sample_rate(void *sdr, uint32_t rate) { (void)sdr; (void)rate; }
void sdr_set_tuner_gain(void *sdr, int gain) { (void)sdr; (void)gain; }

/// Initialize server configuration with default values
void server_config_init(server_config_t *cfg)
{
    if (!cfg) return;
    
    memset(cfg, 0, sizeof(server_config_t));
    
    // Initialize input transports list
    list_ensure_size(&cfg->input_transports, 1);
    
    // Add default HTTP transport for receiving client data
    input_transport_config_t *http_transport = calloc(1, sizeof(input_transport_config_t));
    if (http_transport) {
        http_transport->type = INPUT_TRANSPORT_HTTP;
        http_transport->host = strdup("0.0.0.0");
        http_transport->port = 8080;  // Default port for client connections
        http_transport->timeout_ms = 30000;
        http_transport->max_connections = 100;
        list_push(&cfg->input_transports, http_transport);
    }
    
    // Ready queue defaults
    cfg->ready_queue.type = OUTPUT_QUEUE_FILE;
    cfg->ready_queue.name = strdup("ready_devices");
    cfg->ready_queue.file_path = strdup("/var/log/rtl433/ready.jsonl");
    cfg->ready_queue.batch_size = 100;
    cfg->ready_queue.batch_timeout_ms = 5000;
    cfg->ready_queue.max_queue_size = 10000;
    
    // Unknown queue defaults
    cfg->unknown_queue.type = OUTPUT_QUEUE_DATABASE;
    cfg->unknown_queue.name = strdup("unknown_signals");
    cfg->unknown_queue.db_path = strdup("/var/lib/rtl433/unknown.db");
    cfg->unknown_queue.batch_size = 50;
    cfg->unknown_queue.batch_timeout_ms = 10000;
    cfg->unknown_queue.max_queue_size = 5000;
    
    // Processing defaults
    cfg->max_concurrent_signals = 100;
    cfg->signal_timeout_ms = 2000;
    cfg->batch_processing = 1;
    cfg->batch_size = 20;
    cfg->batch_timeout_ms = 1000;
    cfg->worker_threads = 4;
    cfg->io_threads = 2;
    cfg->queue_buffer_size = 1000;
    
    // Database defaults
    cfg->unknown_signals_db_path = strdup("/var/lib/rtl433/unknown.db");
    cfg->db_retention_days = 30;
    cfg->db_max_size_mb = 1024;
    
    // Web interface defaults
    cfg->web_enabled = 1;
    cfg->web_host = strdup("0.0.0.0");
    cfg->web_port = 8081;
    cfg->web_static_path = strdup("/usr/share/rtl433_server/web");
    
    // Logging defaults
    cfg->verbosity = 2;
    cfg->log_file = strdup("/var/log/rtl433/server.log");
    cfg->log_rotation = 1;
    cfg->log_max_size_mb = 100;
    cfg->log_max_files = 5;
    
    // Monitoring defaults
    cfg->stats_enabled = 1;
    cfg->stats_interval_sec = 60;
    
    // Daemon defaults
    cfg->daemon_mode = 0;
    cfg->pid_file = strdup("/var/run/rtl433_server.pid");
    cfg->user = strdup("rtl433");
    cfg->group = strdup("rtl433");
}

/// Free configuration memory
void server_config_free(server_config_t *cfg)
{
    if (!cfg) return;
    
    // Free ready queue
    free(cfg->ready_queue.name);
    free(cfg->ready_queue.host);
    free(cfg->ready_queue.username);
    free(cfg->ready_queue.password);
    free(cfg->ready_queue.topic_queue);
    free(cfg->ready_queue.file_path);
    free(cfg->ready_queue.db_path);
    
    // Free unknown queue
    free(cfg->unknown_queue.name);
    free(cfg->unknown_queue.host);
    free(cfg->unknown_queue.username);
    free(cfg->unknown_queue.password);
    free(cfg->unknown_queue.topic_queue);
    free(cfg->unknown_queue.file_path);
    free(cfg->unknown_queue.db_path);
    
    // Free other fields
    free(cfg->unknown_signals_db_path);
    free(cfg->web_host);
    free(cfg->web_static_path);
    free(cfg->log_file);
    free(cfg->stats_output_file);
    free(cfg->pid_file);
    free(cfg->user);
    free(cfg->group);
    
    memset(cfg, 0, sizeof(server_config_t));
}

/// Load configuration from file
int server_config_load_file(server_config_t *cfg, const char *filename)
{
    // TODO: Implement configuration file parsing
    (void)cfg;
    (void)filename;
    return 0;
}

/// Parse command line arguments
int server_config_parse_args(server_config_t *cfg, int argc, char **argv)
{
    // TODO: Implement command line argument parsing
    (void)cfg;
    (void)argc;
    (void)argv;
    return 0;
}

/// Print help
void server_config_print_help(const char *program_name)
{
    printf("Usage: %s [options]\n", program_name);
    printf("\nRTL_433 Server - Device decoding and queue management\n");
    printf("\nOptions:\n");
    printf("  -h, --help              Show this help\n");
    printf("  -H <bind>               HTTP server (host:port)\n");
    printf("  -M <url>                MQTT broker (mqtt://host:port)\n");
    printf("  -v                      Increase verbosity\n");
    printf("  -c <file>               Configuration file\n");
    printf("  -d                      Daemon mode\n");
    printf("  -p <file>               PID file\n");
    printf("\nFor more information, see README_SPLIT.md\n");
}

/// Validate configuration
int server_config_validate(const server_config_t *cfg)
{
    if (!cfg) return -1;
    
    if (cfg->worker_threads <= 0 || cfg->worker_threads > 64) {
        return -1;
    }
    
    if (cfg->io_threads <= 0 || cfg->io_threads > 16) {
        return -1;
    }
    
    if (cfg->web_port <= 0 || cfg->web_port > 65535) {
        return -1;
    }
    
    return 0;
}

/// Add input transport
int server_config_add_input_transport(server_config_t *cfg, const input_transport_config_t *transport)
{
    // TODO: Implement input transport management
    (void)cfg;
    (void)transport;
    return 0;
}

/// Add decoder
int server_config_add_decoder(server_config_t *cfg, const decoder_config_t *decoder)
{
    // TODO: Implement decoder management
    (void)cfg;
    (void)decoder;
    return 0;
}

/// Find decoder configuration by ID
decoder_config_t *server_config_find_decoder(const server_config_t *cfg, int device_id)
{
    // TODO: Implement decoder lookup
    (void)cfg;
    (void)device_id;
    return NULL;
}

/// Load decoders from configuration file
int server_config_load_decoders(server_config_t *cfg, const char *filename)
{
    // TODO: Implement decoder configuration loading
    (void)cfg;
    (void)filename;
    return 0;
}

/// Save configuration to file
int server_config_save_file(const server_config_t *cfg, const char *filename)
{
    // TODO: Implement configuration saving
    (void)cfg;
    (void)filename;
    return 0;
}
