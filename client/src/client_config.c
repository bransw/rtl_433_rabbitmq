/** @file
    Client configuration implementation.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "client_config.h"
#include <stdlib.h>
#include <string.h>

/// Initialize client configuration with default values
void client_config_init(client_config_t *cfg)
{
    if (!cfg) return;
    
    memset(cfg, 0, sizeof(client_config_t));
    
    // SDR defaults
    cfg->device_query = strdup("0");
    cfg->gain_str = strdup("auto");
    cfg->frequency = 433920000;
    cfg->sample_rate = 250000;
    cfg->ppm_error = 0;
    
    // Demodulation defaults
    cfg->ook_detect_mode = 0; // auto
    cfg->fsk_detect_mode = 0; // auto
    cfg->level_limit = -12.0;
    cfg->min_level = -20.0;
    cfg->min_snr = 6.0;
    cfg->analyze_pulses = 0;
    
    // Transport defaults
    cfg->transport.type = TRANSPORT_HTTP;
    cfg->transport.host = strdup("localhost");
    cfg->transport.port = 8080;
    cfg->transport.topic_queue = strdup("/api/signals");
    cfg->transport.ssl_enabled = 0;
    
    // Operation defaults
    cfg->duration = 0; // infinite
    cfg->raw_mode = 0;
    cfg->batch_mode = 0;
    cfg->batch_size = 10;
    cfg->batch_timeout_ms = 1000;
    
    // Logging defaults
    cfg->verbosity = 1;
}

/// Free configuration memory
void client_config_free(client_config_t *cfg)
{
    if (!cfg) return;
    
    free(cfg->device_query);
    free(cfg->gain_str);
    free(cfg->settings_str);
    free(cfg->log_file);
    
    free(cfg->transport.host);
    free(cfg->transport.username);
    free(cfg->transport.password);
    free(cfg->transport.topic_queue);
    free(cfg->transport.exchange);
    free(cfg->transport.ca_cert_path);
    free(cfg->transport.client_cert_path);
    free(cfg->transport.client_key_path);
    
    memset(cfg, 0, sizeof(client_config_t));
}

/// Load configuration from file
int client_config_load_file(client_config_t *cfg, const char *filename)
{
    // TODO: Implement configuration file parsing
    (void)cfg;
    (void)filename;
    return 0;
}

/// Parse command line arguments
int client_config_parse_args(client_config_t *cfg, int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            client_config_print_help(argv[0]);
            exit(0);
        }
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            // Input file
            cfg->input_files.data = realloc(cfg->input_files.data, 
                                           (cfg->input_files.len + 1) * sizeof(char*));
            if (cfg->input_files.data) {
                ((char**)cfg->input_files.data)[cfg->input_files.len] = strdup(argv[++i]);
                cfg->input_files.len++;
            }
        }
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            // Frequency
            cfg->frequency = (uint32_t)atol(argv[++i]);
        }
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            // Sample rate
            cfg->sample_rate = (uint32_t)atol(argv[++i]);
        }
        else if (strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
            // Server URL
            free(cfg->transport.host);
            char *url = argv[++i];
            if (strncmp(url, "http://", 7) == 0) {
                cfg->transport.type = TRANSPORT_HTTP;
                url += 7; // Skip "http://"
                char *colon = strchr(url, ':');
                if (colon) {
                    *colon = '\0';
                    cfg->transport.host = strdup(url);
                    cfg->transport.port = atoi(colon + 1);
                    *colon = ':'; // Restore
                } else {
                    cfg->transport.host = strdup(url);
                    cfg->transport.port = 8080;
                }
            }
        }
        else if (strcmp(argv[i], "-v") == 0) {
            cfg->verbosity++;
        }
    }
    return 0;
}

/// Print help
void client_config_print_help(const char *program_name)
{
    printf("Usage: %s [options]\n", program_name);
    printf("\nRTL_433 Client - Signal demodulation and server transmission\n");
    printf("\nOptions:\n");
    printf("  -h, --help              Show this help\n");
    printf("  -d <device>             RTL-SDR device (default: 0)\n");
    printf("  -f <freq>               Frequency in Hz (default: 433920000)\n");
    printf("  -s <rate>               Sample rate (default: 250000)\n");
    printf("  -g <gain>               Gain (default: auto)\n");
    printf("  -p <ppm>                Frequency correction in ppm\n");
    printf("  -T <url>                Server URL (http://host:port/path)\n");
    printf("  -v                      Increase verbosity\n");
    printf("  -c <file>               Configuration file\n");
    printf("\nFor more information, see README_SPLIT.md\n");
}

/// Validate configuration
int client_config_validate(const client_config_t *cfg)
{
    if (!cfg) return -1;
    
    if (!cfg->transport.host) {
        return -1;
    }
    
    if (cfg->transport.port <= 0 || cfg->transport.port > 65535) {
        return -1;
    }
    
    if (cfg->frequency == 0) {
        return -1;
    }
    
    if (cfg->sample_rate == 0) {
        return -1;
    }
    
    return 0;
}
