/** @file
    RTL_433 Client - Signal demodulation using rtl_433 APIs
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include "rtl_433.h"
#include "r_api.h"
#include "r_private.h"
#include "r_device.h"
#include "pulse_data.h"
#include "pulse_detect.h"
#include "pulse_analyzer.h"
#include "data.h"
#include "list.h"
#include "logger.h"
#include "sdr.h"
#include "baseband.h"
#include "compat_time.h"
#include "client_transport.h"

// Global variables
static r_cfg_t *g_cfg = NULL;
static transport_connection_t g_transport;
static volatile sig_atomic_t exit_flag = 0;

// Statistics
typedef struct {
    unsigned long signals_sent;
    unsigned long send_errors;
    time_t start_time;
} client_stats_t;

static client_stats_t g_stats = {0};

/// Forward declaration  
static void client_pulse_handler(pulse_data_t *pulse_data);

/// Process CU8 file with proper IQ -> AM -> pulse pipeline
static int process_cu8_file(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file) {
        print_logf(LOG_ERROR, "Client", "Failed to open file: %s", filename);
        return -1;
    }
    
    print_logf(LOG_INFO, "Client", "Processing CU8 file: %s", filename);
    
    // Buffer for IQ samples (use rtl_433's default)
    uint8_t iq_buf[DEFAULT_BUF_LENGTH];
    uint16_t am_buf_temp[DEFAULT_BUF_LENGTH/2];  // Temporary AM buffer (envelope_detect output)
    int16_t am_buf[DEFAULT_BUF_LENGTH/2];        // AM demodulated samples (pulse_detect input)
    int16_t fm_buf[DEFAULT_BUF_LENGTH/2];        // FM demodulated samples (signed 16-bit)
    
    struct dm_state *demod = g_cfg->demod;
    unsigned packages_found = 0;
    
    while (!exit_flag) {
        size_t n_read = fread(iq_buf, 1, sizeof(iq_buf), file);
        if (n_read == 0) {
            if (feof(file)) {
                break; // End of file
            }
            continue; // Read error, try again
        }
        
        unsigned long n_samples = n_read / 2; // CU8 = 2 bytes per sample (I+Q)
        if (n_samples == 0) continue;
        
        // Step 1: AM demodulation (IQ -> amplitude)
        float avg_db = envelope_detect(iq_buf, am_buf_temp, n_samples);
        
        // Convert uint16_t to int16_t for pulse detection
        for (unsigned long i = 0; i < n_samples; i++) {
            am_buf[i] = (int16_t)am_buf_temp[i];
        }
        
        print_logf(LOG_DEBUG, "Client", "Read %lu samples, avg_db=%.1f", n_samples, avg_db);
        
        // Step 2: Pulse detection (amplitude -> pulses)
        pulse_data_t pulse_data = {0};
        pulse_data_t fsk_pulse_data = {0};
        
        int package_type = pulse_detect_package(demod->pulse_detect, am_buf, fm_buf, n_samples, 
                                               g_cfg->samp_rate, 0, &pulse_data, &fsk_pulse_data, 
                                               FSK_PULSE_DETECT_OLD);
                                               
        if (package_type == PULSE_DATA_OOK && pulse_data.num_pulses > 0) {
            packages_found++;
            
            // Step 3: Calculate signal metrics
            calc_rssi_snr(g_cfg, &pulse_data);
            
            print_logf(LOG_INFO, "Client", "Detected OOK package #%u with %u pulses", 
                      packages_found, pulse_data.num_pulses);
                      
            // Step 4: Use pulse_analyzer for detailed analysis (like rtl_433 -A)
            r_device device = {.log_fn = log_device_handler, .output_ctx = g_cfg};
            pulse_analyzer(&pulse_data, package_type, &device);
            
            // Step 5: Send analyzed pulse data via transport
            client_pulse_handler(&pulse_data);
        }
        
        if (package_type == PULSE_DATA_FSK && fsk_pulse_data.num_pulses > 0) {
            packages_found++;
            calc_rssi_snr(g_cfg, &fsk_pulse_data);
            
            print_logf(LOG_INFO, "Client", "Detected FSK package #%u with %u pulses", 
                      packages_found, fsk_pulse_data.num_pulses);
                      
            r_device device = {.log_fn = log_device_handler, .output_ctx = g_cfg};
            pulse_analyzer(&fsk_pulse_data, package_type, &device);
            client_pulse_handler(&fsk_pulse_data);
        }
    }
    
    fclose(file);
    print_logf(LOG_INFO, "Client", "File processing completed. Found %u packages", packages_found);
    return 0;
}

/// Custom data acquired handler for rtl_433 integration  
void client_data_acquired_handler(r_device *r_dev, data_t *data)
{
    if (!data) return;
    
    // Convert data_t to JSON and send via transport
    char json_buffer[8192];
    size_t json_len = data_print_jsons(data, json_buffer, sizeof(json_buffer));
    if (json_len > 0) {
        char *json_str = strdup(json_buffer);
        if (json_str) {
            // Create demod_data_t structure for transport
            demod_data_t demod_data = {0};
            demod_data.device_id = strdup(r_dev ? r_dev->name : "unknown");
            demod_data.timestamp_us = time(NULL) * 1000000ULL;
            demod_data.frequency = g_cfg->center_frequency;
            demod_data.sample_rate = g_cfg->samp_rate;
            demod_data.raw_data_hex = json_str;
            
            // Send via transport
            int result = transport_send_demod_data(&g_transport, &demod_data);
            
            if (result == 0) {
                g_stats.signals_sent++;
                print_logf(LOG_INFO, "Client", "Sent device data: %s", r_dev ? r_dev->name : "unknown");
            } else {
                g_stats.send_errors++;
                print_log(LOG_WARNING, "Client", "Failed to send device data");
            }
            
            // Cleanup
            free(demod_data.device_id);
            free(json_str);
        }
    }
}

/// Signal handler
static void signal_handler(int sig)
{
    print_logf(LOG_INFO, "Client", "Received signal %d, shutting down...", sig);
    exit_flag = 1;
    if (g_cfg) {
        g_cfg->exit_async = 1;
    }
}

/// Print statistics
static void print_statistics(void)
{
    time_t now;
    time(&now);
    double uptime = difftime(now, g_stats.start_time);
    
    print_logf(LOG_INFO, "Client", "=== Client Statistics ===");
    print_logf(LOG_INFO, "Client", "Uptime: %.0f sec", uptime);
    print_logf(LOG_INFO, "Client", "Signals sent: %lu", g_stats.signals_sent);
    print_logf(LOG_INFO, "Client", "Send errors: %lu", g_stats.send_errors);
    
    if (uptime > 0) {
        print_logf(LOG_INFO, "Client", "Rate: %.1f signals/min", 
                  g_stats.signals_sent * 60.0 / uptime);
    }
}

// Removed unused client_event_handler

/// Custom pulse handler - sends raw pulse data to server
static void client_pulse_handler(pulse_data_t *pulse_data)
{
    if (!pulse_data || !pulse_data->num_pulses) {
        return;
    }
    
    // Send pulse data to server via transport
    if (transport_send_pulse_data(&g_transport, pulse_data) == 0) {
        g_stats.signals_sent++;
        if (g_cfg->verbosity >= LOG_DEBUG) {
            print_logf(LOG_DEBUG, "Client", "Sent signal: %u pulses", pulse_data->num_pulses);
        }
    } else {
        g_stats.send_errors++;
        print_log(LOG_WARNING, "Client", "Failed to send signal");
    }
}

/// SDR event callback function - processes received samples  
static void client_sdr_callback(sdr_event_t *ev, void *ctx)
{
    static unsigned long callback_count = 0;
    callback_count++;
    
    r_cfg_t *cfg = ctx;
    struct dm_state *demod = cfg->demod;
    
    // Debug: Always log callback invocation with high verbosity
    if (g_cfg->verbosity >= LOG_DEBUG || callback_count % 100 == 1) {
        print_logf(LOG_INFO, "SDR", "Callback #%lu: ev=0x%x, demod=%p, exit=%d", 
                  callback_count, ev->ev, (void*)demod, exit_flag);
    }
    
    if (!demod || exit_flag) {
        return; // ignore the data if demod is not available or exiting
    }
    
    // Handle different event types
    if (ev->ev & SDR_EV_DATA) {
        unsigned char *iq_buf = (unsigned char *)ev->buf;
        uint32_t len = ev->len;
        
        // Process the IQ samples to extract pulse data
        unsigned long n_samples = len / demod->sample_size;
        if (n_samples * demod->sample_size != len) {
            print_log(LOG_WARNING, "SDR", "Sample buffer length not aligned to sample size!");
            return;
        }
        if (!n_samples) {
            print_log(LOG_WARNING, "SDR", "Sample buffer too short!");
            return;
        }
        
        // For debugging - count samples
        static unsigned long total_samples = 0;
        total_samples += n_samples;
        
        // Always log first few callbacks and then periodically
        static unsigned long data_callbacks = 0;
        data_callbacks++;
        
        if (data_callbacks <= 5 || g_cfg->verbosity >= LOG_DEBUG) {
            print_logf(LOG_INFO, "SDR", "Data callback #%lu: %lu samples (%lu total), buf=%p", 
                      data_callbacks, n_samples, total_samples, (void*)iq_buf);
        }
        
        // Simplified signal processing pipeline
        // This implements the core functionality from rtl_433's sdr_callback
        
        // Skip time handling for now to avoid function import issues
        // get_time_now(&demod->now);
        
        // Age the frame position if there is one
        if (demod->frame_start_ago)
            demod->frame_start_ago += n_samples;
        if (demod->frame_end_ago)
            demod->frame_end_ago += n_samples;
        
        // Apply same proper IQ→AM→pulse pipeline as file processing
        
        // Step 1: AM demodulation (using correct buffer types)
        uint16_t am_buf_temp[n_samples];
        int16_t am_buf[n_samples];
        int16_t fm_buf[n_samples];  // For FSK if needed
        
        float avg_db = -999.0f;
        if (demod->sample_size == 2) { // CU8
            avg_db = envelope_detect(iq_buf, am_buf_temp, n_samples);
            
            // Convert uint16_t to int16_t for pulse detection
            for (unsigned long i = 0; i < n_samples; i++) {
                am_buf[i] = (int16_t)am_buf_temp[i];
            }
        }
        
        if (data_callbacks <= 3) {
            print_logf(LOG_INFO, "SDR", "AM demodulation: avg_db=%.1f, first values: %d %d %d %d %d", 
                      avg_db, am_buf[0], am_buf[1], am_buf[2], am_buf[3], am_buf[4]);
        }
        
        // Step 2: Real pulse detection (same as file processing)
        pulse_data_t pulse_data = {0};
        pulse_data_t fsk_pulse_data = {0};
        
        int package_type = pulse_detect_package(demod->pulse_detect, am_buf, fm_buf, n_samples, 
                                               cfg->samp_rate, 0, &pulse_data, &fsk_pulse_data, 
                                               FSK_PULSE_DETECT_OLD);
                                               
        // Step 3: Process detected packages with pulse_analyzer (like rtl_433 -A)
        if (package_type == PULSE_DATA_OOK && pulse_data.num_pulses > 0) {
            calc_rssi_snr(cfg, &pulse_data);
            
            print_logf(LOG_INFO, "SDR", "Detected OOK package with %u pulses (avg_db=%.1f)", 
                      pulse_data.num_pulses, avg_db);
                      
            // Use pulse_analyzer for detailed analysis
            r_device device = {.log_fn = log_device_handler, .output_ctx = cfg};
            pulse_analyzer(&pulse_data, package_type, &device);
            
            // Send real analyzed data
            client_pulse_handler(&pulse_data);
        }
        
        if (package_type == PULSE_DATA_FSK && fsk_pulse_data.num_pulses > 0) {
            calc_rssi_snr(cfg, &fsk_pulse_data);
            
            print_logf(LOG_INFO, "SDR", "Detected FSK package with %u pulses (avg_db=%.1f)", 
                      fsk_pulse_data.num_pulses, avg_db);
                      
            r_device device = {.log_fn = log_device_handler, .output_ctx = cfg};
            pulse_analyzer(&fsk_pulse_data, package_type, &device);
            client_pulse_handler(&fsk_pulse_data);
        }
    }
    
    // Handle other event types
    if (ev->ev & SDR_EV_RATE) {
        print_logf(LOG_INFO, "SDR", "Sample rate changed to %u Hz", ev->sample_rate);
    }
    if (ev->ev & SDR_EV_FREQ) {
        print_logf(LOG_INFO, "SDR", "Frequency changed to %u Hz", ev->center_frequency);
    }
    if (ev->ev & SDR_EV_GAIN) {
        print_logf(LOG_INFO, "SDR", "Gain changed to %s", ev->gain_str ? ev->gain_str : "auto");
    }
}

/// Start SDR device
static int client_start_sdr(r_cfg_t *cfg)
{
    print_log(LOG_INFO, "SDR", "Initializing SDR device...");
    
    // Open SDR device
    int r = sdr_open(&cfg->dev, cfg->dev_query, cfg->verbosity);
    if (r < 0) {
        print_log(LOG_ERROR, "SDR", "Failed to open SDR device");
        return -1;
    }
    
    cfg->dev_info = sdr_get_dev_info(cfg->dev);
    cfg->demod->sample_size = sdr_get_sample_size(cfg->dev);
    
    // Set sample rate
    sdr_set_sample_rate(cfg->dev, cfg->samp_rate, 1);
    
    // Set gain
    sdr_set_tuner_gain(cfg->dev, cfg->gain_str, 1);
    
    // Set frequency correction if specified
    if (cfg->ppm_error) {
        sdr_set_freq_correction(cfg->dev, cfg->ppm_error, 1);
    }
    
    // Reset buffers
    r = sdr_reset(cfg->dev, cfg->verbosity);
    if (r < 0) {
        print_log(LOG_ERROR, "SDR", "Failed to reset buffers");
        return -2;
    }
    
    sdr_activate(cfg->dev);
    
    print_log(LOG_NOTICE, "SDR", "Reading samples in async mode...");
    
    // Set center frequency
    sdr_set_center_freq(cfg->dev, cfg->center_frequency, 1);
    
    // Start async reading
    r = sdr_start(cfg->dev, client_sdr_callback, (void *)cfg,
                  DEFAULT_ASYNC_BUF_NUMBER, cfg->out_block_size);
    if (r < 0) {
        print_logf(LOG_ERROR, "SDR", "Failed to start async mode (%d)", r);
        return -3;
    }
    
    print_log(LOG_INFO, "SDR", "SDR device started successfully");
    return 0;
}

/// Stop SDR device
static void client_stop_sdr(r_cfg_t *cfg)
{
    if (cfg->dev) {
        print_log(LOG_INFO, "SDR", "Stopping SDR device...");
        sdr_stop(cfg->dev);
        sdr_close(cfg->dev);
        cfg->dev = NULL;
        print_log(LOG_INFO, "SDR", "SDR device stopped");
    }
}

/// Print help
static void print_help(const char *program_name)
{
    printf("Usage: %s [rtl_433 options] -T <server_url>\n", program_name);
    printf("\nRTL_433 Client - Signal demodulation and server transmission\n");
    printf("\nClient-specific options:\n");
    printf("  -T <url>                Server URL (http://host:port/path)\n");
    printf("  --transport-help        Show transport options\n");
    printf("\nAll standard rtl_433 options are supported:\n");
    printf("  -r <file>               Read from file instead of RTL-SDR\n");
    printf("  -d <device>             RTL-SDR device (default: 0)\n");
    printf("  -f <freq>               Frequency in Hz (default: 433920000)\n");
    printf("  -s <rate>               Sample rate (default: 250000)\n");
    printf("  -g <gain>               Gain (default: auto)\n");
    printf("  -v                      Increase verbosity\n");
    printf("  -h                      Show rtl_433 help\n");
    printf("\nExamples:\n");
    printf("  %s -r signal.cu8 -T http://localhost:8080/api/signals\n", program_name);
    printf("  %s -d 0 -f 433.92M -T http://server:8080\n", program_name);
    printf("\nFor more information, see README_SPLIT.md\n");
}

/// Parse client-specific arguments (extract -T before passing to rtl_433)
static int parse_client_args(int argc, char **argv, transport_config_t *transport_cfg)
{
    // Initialize transport config
    memset(transport_cfg, 0, sizeof(transport_config_t));
    transport_cfg->type = TRANSPORT_HTTP;
    transport_cfg->host = strdup("localhost");
    transport_cfg->port = 8080;
    transport_cfg->topic_queue = strdup("/api/signals");
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
            // Parse server URL
            char *url = argv[i + 1];
            free(transport_cfg->host);
            free(transport_cfg->topic_queue);
            
            if (strncmp(url, "http://", 7) == 0) {
                transport_cfg->type = TRANSPORT_HTTP;
                url += 7; // Skip "http://"
                
                char *colon = strchr(url, ':');
                char *slash = strchr(url, '/');
                
                if (colon && (!slash || colon < slash)) {
                    *colon = '\0';
                    transport_cfg->host = strdup(url);
                    
                    char *port_str = colon + 1;
                    if (slash) {
                        *slash = '\0';
                        transport_cfg->port = atoi(port_str);
                        transport_cfg->topic_queue = strdup(slash + 1);
                        *slash = '/'; // Restore
                    } else {
                        transport_cfg->port = atoi(port_str);
                        transport_cfg->topic_queue = strdup("/api/signals");
                    }
                    *colon = ':'; // Restore
                } else {
                    if (slash) {
                        *slash = '\0';
                        transport_cfg->host = strdup(url);
                        transport_cfg->topic_queue = strdup(slash + 1);
                        *slash = '/'; // Restore
                    } else {
                        transport_cfg->host = strdup(url);
                        transport_cfg->topic_queue = strdup("/api/signals");
                    }
                    transport_cfg->port = 8080;
                }
            } else if (strncmp(url, "mqtt://", 7) == 0) {
                transport_cfg->type = TRANSPORT_MQTT;
                url += 7; // Skip "mqtt://"
                
                // Parse user:password@host:port/topic format
                char *at_sign = strchr(url, '@');
                char *host_start = url;
                
                if (at_sign) {
                    // Extract credentials
                    *at_sign = '\0';
                    char *colon = strchr(url, ':');
                    if (colon) {
                        *colon = '\0';
                        transport_cfg->username = strdup(url);
                        transport_cfg->password = strdup(colon + 1);
                        *colon = ':'; // Restore
                    } else {
                        transport_cfg->username = strdup(url);
                    }
                    host_start = at_sign + 1;
                    *at_sign = '@'; // Restore
                }
                
                char *colon = strchr(host_start, ':');
                char *slash = strchr(host_start, '/');
                
                if (colon && (!slash || colon < slash)) {
                    *colon = '\0';
                    transport_cfg->host = strdup(host_start);
                    
                    char *port_str = colon + 1;
                    if (slash) {
                        *slash = '\0';
                        transport_cfg->port = atoi(port_str);
                        transport_cfg->topic_queue = strdup(slash + 1);
                        *slash = '/'; // Restore
                    } else {
                        transport_cfg->port = atoi(port_str);
                        transport_cfg->topic_queue = strdup("rtl_433/signals");
                    }
                    *colon = ':'; // Restore
                } else {
                    if (slash) {
                        *slash = '\0';
                        transport_cfg->host = strdup(host_start);
                        transport_cfg->topic_queue = strdup(slash + 1);
                        *slash = '/'; // Restore
                    } else {
                        transport_cfg->host = strdup(host_start);
                        transport_cfg->topic_queue = strdup("rtl_433/signals");
                    }
                    transport_cfg->port = 1883; // Default MQTT port
                }
            } else if (strncmp(url, "amqp://", 7) == 0) {
                transport_cfg->type = TRANSPORT_RABBITMQ;
                url += 7; // Skip "amqp://"
                
                // Parse user:password@host:port/exchange/queue format
                char *at_sign = strchr(url, '@');
                char *host_start = url;
                
                if (at_sign) {
                    // Extract credentials
                    *at_sign = '\0';
                    char *colon = strchr(url, ':');
                    if (colon) {
                        *colon = '\0';
                        transport_cfg->username = strdup(url);
                        transport_cfg->password = strdup(colon + 1);
                        *colon = ':'; // Restore
                    } else {
                        transport_cfg->username = strdup(url);
                    }
                    host_start = at_sign + 1;
                    *at_sign = '@'; // Restore
                }
                
                char *colon = strchr(host_start, ':');
                char *slash = strchr(host_start, '/');
                
                if (colon && (!slash || colon < slash)) {
                    *colon = '\0';
                    transport_cfg->host = strdup(host_start);
                    
                    char *port_str = colon + 1;
                    if (slash) {
                        *slash = '\0';
                        transport_cfg->port = atoi(port_str);
                        
                        // Parse exchange/queue
                        char *exchange_queue = slash + 1;
                        char *queue_slash = strchr(exchange_queue, '/');
                        if (queue_slash) {
                            *queue_slash = '\0';
                            transport_cfg->topic_queue = strdup(exchange_queue);
                            // Queue part will be handled in transport init
                            *queue_slash = '/'; // Restore
                        } else {
                            transport_cfg->topic_queue = strdup(exchange_queue);
                        }
                        *slash = '/'; // Restore
                    } else {
                        transport_cfg->port = atoi(port_str);
                        transport_cfg->topic_queue = strdup("rtl_433");
                    }
                    *colon = ':'; // Restore
                } else {
                    if (slash) {
                        *slash = '\0';
                        transport_cfg->host = strdup(host_start);
                        
                        // Parse exchange/queue
                        char *exchange_queue = slash + 1;
                        char *queue_slash = strchr(exchange_queue, '/');
                        if (queue_slash) {
                            *queue_slash = '\0';
                            transport_cfg->topic_queue = strdup(exchange_queue);
                            *queue_slash = '/'; // Restore
                        } else {
                            transport_cfg->topic_queue = strdup(exchange_queue);
                        }
                        *slash = '/'; // Restore
                    } else {
                        transport_cfg->host = strdup(host_start);
                        transport_cfg->topic_queue = strdup("rtl_433");
                    }
                    transport_cfg->port = 5672; // Default AMQP port
                }
            }
            
            // Remove -T and URL from argv for rtl_433 parsing
            for (int j = i; j < argc - 2; j++) {
                argv[j] = argv[j + 2];
            }
            argc -= 2;
            i--; // Adjust index after removal
        }
        else if (strcmp(argv[i], "--transport-help") == 0) {
            printf("Transport options:\n");
            printf("  http://host:port/path   HTTP transport (default)\n");
            printf("  mqtt://[user:pass@]host[:port][/topic]  MQTT transport (if enabled)\n");
            printf("  amqp://[user:pass@]host[:port][/exchange[/queue]]  RabbitMQ transport (if enabled)\n");
            exit(0);
        }
    }
    
    return argc; // Return new argc after removing client-specific args
}

/// Main function
int main(int argc, char **argv)
{
    int ret = 0;
    transport_config_t transport_cfg;
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize statistics
    time(&g_stats.start_time);
    
    print_log(LOG_INFO, "Client", "rtl_433_client started");
    
    // Parse client-specific arguments first
    argc = parse_client_args(argc, argv, &transport_cfg);
    
    // Check if server URL was provided
    if (!transport_cfg.host) {
        fprintf(stderr, "Error: Server URL required. Use -T http://host:port/path\n");
        print_help(argv[0]);
        return 1;
    }
    
    // Create and initialize rtl_433 configuration
    g_cfg = r_create_cfg();
    if (!g_cfg) {
        print_log(LOG_ERROR, "Client", "Failed to create configuration");
        return 1;
    }
    
    r_init_cfg(g_cfg);
    
    // Add null output to suppress normal rtl_433 output
    add_null_output(g_cfg, NULL);
    
    // Set client-specific configuration
    g_cfg->verbosity = LOG_INFO;  // Default verbosity
    g_cfg->center_frequency = 433920000;  // Default frequency (433.92 MHz)
    g_cfg->dev_query = strdup("0");  // Default device
    g_cfg->gain_str = strdup("auto");  // Default gain
    
    // Parse remaining arguments using rtl_433's parser
    // Note: We need to implement rtl_433 argument parsing here
    // For now, handle basic arguments manually
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            exit(0);
        }
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            add_infile(g_cfg, argv[++i]);
        }
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            // Parse frequency
            char *endptr;
            double freq = strtod(argv[++i], &endptr);
            if (endptr != argv[i]) {
                if (strcasecmp(endptr, "k") == 0 || strcasecmp(endptr, "khz") == 0) {
                    freq *= 1000;
                } else if (strcasecmp(endptr, "m") == 0 || strcasecmp(endptr, "mhz") == 0) {
                    freq *= 1000000;
                } else if (strcasecmp(endptr, "g") == 0 || strcasecmp(endptr, "ghz") == 0) {
                    freq *= 1000000000;
                }
                set_center_freq(g_cfg, (uint32_t)freq);
                // Manually set center_frequency since SDR is stubbed
                g_cfg->center_frequency = (uint32_t)freq;
            }
        }
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            set_sample_rate(g_cfg, (uint32_t)atol(argv[++i]));
        }
        else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            free(g_cfg->dev_query);
            g_cfg->dev_query = strdup(argv[++i]);
        }
        else if (strcmp(argv[i], "-g") == 0 && i + 1 < argc) {
            free(g_cfg->gain_str);
            g_cfg->gain_str = strdup(argv[++i]);
        }
        else if (strcmp(argv[i], "-v") == 0) {
            g_cfg->verbosity++;
        }
    }
    
    // Initialize transport
    if (transport_init(&g_transport, &transport_cfg) != 0) {
        print_log(LOG_ERROR, "Client", "Transport initialization error");
        ret = 2;
        goto cleanup;
    }
    
    // Connect to server
    if (transport_connect(&g_transport) != 0) {
        print_log(LOG_ERROR, "Client", "Server connection error");
        ret = 3;
        goto cleanup;
    }
    
    print_logf(LOG_INFO, "Client", "Connected to server: %s:%d", 
              transport_cfg.host, transport_cfg.port);
    
    // Override rtl_433's event handler with our custom one
    // TODO: We need to modify r_cfg to support custom pulse handlers
    
    print_log(LOG_INFO, "Client", "Starting signal demodulation...");
    
    // Main processing loop
    if (g_cfg->in_files.len > 0) {
        // File input mode - use proper IQ -> AM -> pulse pipeline
        print_log(LOG_INFO, "Client", "File input mode with proper demodulation pipeline");
        
        // Process each input file
        for (void **iter = g_cfg->in_files.elems; iter && *iter; ++iter) {
            char *filename = *iter;
            int result = process_cu8_file(filename);
            if (result != 0) {
                print_logf(LOG_ERROR, "Client", "Failed to process file: %s", filename);
            }
        }
    } else {
        // SDR input mode
        print_log(LOG_INFO, "Client", "Starting SDR input mode...");
        print_logf(LOG_INFO, "Client", "Frequency: %.1f MHz", g_cfg->center_frequency / 1e6);
        print_logf(LOG_INFO, "Client", "Sample rate: %u Hz", g_cfg->samp_rate);
        
        // Enable all decoders for SDR processing
        register_all_protocols(g_cfg, 0);
        
        // Start outputs to trigger our data_acquired_handler
        start_outputs(g_cfg, NULL);
        
        // Initialize SDR device using rtl_433 APIs
        ret = client_start_sdr(g_cfg);
        if (ret != 0) {
            print_logf(LOG_ERROR, "Client", "Failed to start SDR: %d", ret);
            goto cleanup;
        }
        
        // Main SDR processing loop
        print_log(LOG_INFO, "Client", "SDR running... Press Ctrl+C to stop");
        while (!exit_flag) {
            // Process SDR data - handled by callbacks
            sleep(1);
            
            // Check connection periodically
            if (!transport_is_connected(&g_transport)) {
                print_log(LOG_WARNING, "Client", "Lost connection to server, reconnecting...");
                if (transport_connect(&g_transport) != 0) {
                    print_log(LOG_ERROR, "Client", "Failed to reconnect");
                    break;
                }
            }
        }
        
        // Stop SDR
        client_stop_sdr(g_cfg);
    }
    
cleanup:
    print_log(LOG_INFO, "Client", "Shutting down...");
    print_statistics();
    
    // Cleanup transport
    transport_disconnect(&g_transport);
    transport_cleanup(&g_transport);
    
    // Cleanup transport config
    free(transport_cfg.host);
    free(transport_cfg.username);
    free(transport_cfg.password);
    free(transport_cfg.topic_queue);
    free(transport_cfg.exchange);
    free(transport_cfg.ca_cert_path);
    free(transport_cfg.client_cert_path);
    free(transport_cfg.client_key_path);
    
    // Cleanup rtl_433 config
    if (g_cfg) {
        r_free_cfg(g_cfg);
    }
    
    print_logf(LOG_INFO, "Client", "rtl_433_client finished with code %d", ret);
    return ret;
}
