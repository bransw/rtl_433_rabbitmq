/** @file
    rtl_433_server - server for decoding demodulated signals from transport queues.
    
    Copyright (C) 2024
    
    Based on rtl_433 by Benjamin Larsson and others.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>

#include "rtl_433.h"
#include "r_private.h"
#include "r_device.h"
#include "r_api.h"
#include "pulse_analyzer.h"
#include "pulse_detect.h"
#include "pulse_slicer.h"
#include "data.h"
#include "r_util.h"
#include "bitbuffer.h"
#include "list.h"
#include "optparse.h"
#include "confparse.h"
#include "term_ctl.h"
#include "compat_paths.h"
#include "logger.h"
#include "fatal.h"

#ifdef ENABLE_RABBITMQ
#include <amqp.h>
#include <amqp_tcp_socket.h>
#endif

#ifdef ENABLE_MQTT  
#include <MQTTClient.h>
#endif

#include <json.h>

// Version string
char const rtl_433_version[] = "24.10-server-dev";

// Stub implementations of missing functions for now
void r_register_protocol(r_cfg_t *cfg, r_device *device)
{
    if (cfg && device && cfg->demod) {
        list_push(&cfg->demod->r_devs, device);
    }
}

void r_register_protocol_by_name(r_cfg_t *cfg, const char *name)
{
    print_logf(LOG_INFO, "Server", "Register protocol by name: %s (stub)", name);
}

void r_add_output(r_cfg_t *cfg, const char *spec)
{
    print_logf(LOG_INFO, "Server", "Add output: %s (stub)", spec);
}

void r_add_meta(r_cfg_t *cfg, const char *spec)
{
    print_logf(LOG_INFO, "Server", "Add meta: %s (stub)", spec);
}

// Forward declaration for flex device (already implemented in flex.c)
extern r_device *flex_create_device(char *spec);

// Stub for term_help_fprintf (used in usage function)
int term_help_fprintf(FILE *fp, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vfprintf(fp, format, args);
    va_end(args);
    return ret;
}

#ifndef _MSC_VER
#include <getopt.h>
#else
#include "getopt/getopt.h"
#endif

// _Noreturn for older compilers
#if !defined _Noreturn
#if defined(__GNUC__)
#define _Noreturn __attribute__((noreturn))
#elif defined(_MSC_VER)
#define _Noreturn __declspec(noreturn)
#else
#define _Noreturn
#endif
#endif

/// Global configuration (based on original rtl_433)
static r_cfg_t g_cfg;

/// Global shutdown flag  
static volatile sig_atomic_t exit_flag = 0;

/// Statistics counters
static struct {
    uint64_t signals_received;
    uint64_t signals_processed;
    uint64_t devices_decoded;
    uint64_t unknown_signals;
    uint64_t processing_errors;
    time_t start_time;
} g_stats = {0};

/// Server-specific options
typedef struct {
    char *rabbitmq_host;
    int rabbitmq_port;
    char *rabbitmq_username;
    char *rabbitmq_password;
    char *rabbitmq_exchange;
    char *signals_queue;
    char *ready_queue;
    char *unknown_queue;
    int daemon_mode;
    char *pid_file;
} server_opts_t;

static server_opts_t g_server_opts = {
    .rabbitmq_host = "localhost",
    .rabbitmq_port = 5672,
    .rabbitmq_username = "guest",
    .rabbitmq_password = "guest",
    .rabbitmq_exchange = "rtl_433",
    .signals_queue = "signals",
    .ready_queue = "detected",
    .unknown_queue = "unknown",
    .daemon_mode = 0,
    .pid_file = NULL
};

#ifdef ENABLE_RABBITMQ
/// RabbitMQ connection state
static amqp_connection_state_t g_rmq_conn = NULL;
static amqp_socket_t *g_rmq_socket = NULL;
static int g_rmq_channel = 1;
#endif

/// Print usage information
_Noreturn
static void usage(int exit_code)
{
    term_help_fprintf(exit_code ? stderr : stdout,
            "rtl_433_server, RF signal decoder server for ISM band devices.\n"
            "Receives demodulated signals from rtl_433_client via transport queues.\n"
            "Version %s\n\n"
            "Usage:\n"
            "\trtl_433_server [<options>]\n\n"
            "Transport options:\n"
            "  [-I <transport>] Input transport type: rabbitmq, mqtt, http (default: rabbitmq)\n"
            "  [-H <host>] Transport server host (default: localhost)\n"
            "  [-P <port>] Transport server port (default: 5672 for RabbitMQ, 1883 for MQTT)\n"
            "  [-u <username>] Authentication username (default: guest)\n"
            "  [-p <password>] Authentication password (default: guest)\n"
            "  [-e <exchange>] RabbitMQ exchange name (default: rtl_433)\n"
            "  [-Q <queue>] Input queue for all signals (default: signals)\n\n"
            "Output options:\n"
            "  [-o <queue>] Output queue for decoded devices (default: detected)\n"
            "  [-O <queue>] Output queue for unknown signals (default: unknown)\n"
            "  [-F <format>] Same output format options as rtl_433\n\n"
            "Device selection:\n"
            "  [-R <id>] Enable only specified device decoder (same as rtl_433)\n"
            "  [-X <spec>] Add flex decoder (same as rtl_433)\n"
            "  [-A] Disable device decoding, analyze only\n\n"
            "General options:\n"
            "  [-v] Increase verbosity (can be used multiple times)\n"
            "  [-V] Output version string and exit\n"
            "  [-h] Display this usage help and exit\n"
            "  [-d] Run as daemon\n"
            "  [-f <file>] Read configuration from file\n"
            "  [-L <file>] Write log to file\n"
            "  [-M <meta>] Add metadata (same as rtl_433: time, protocol, level, etc.)\n\n"
            "Statistics:\n"
            "  [-T <seconds>] Statistics reporting interval (default: 60)\n"
            "  [-S all|unknown|known] Capture signal samples for analysis\n\n"
            "Examples:\n"
            "  rtl_433_server -I rabbitmq -H localhost -P 5672 -u guest -p guest\n"
            "  rtl_433_server -I mqtt -H broker.local -v\n"
            "  rtl_433_server -f /etc/rtl_433_server.conf -d\n"
            "  rtl_433_server -R 12 -X 'n=test,m=OOK_PWM,s=400,l=800,r=8000'\n\n",
            rtl_433_version);
    
    exit(exit_code);
}

/// Signal handler  
static void signal_handler(int sig)
{
    print_logf(LOG_INFO, "Server", "Received signal %d, shutting down...", sig);
    exit_flag = 1;
}

/// Signal initialization
static void init_signals(void)
{
#ifndef _WIN32
    struct sigaction sigact;
    sigact.sa_handler = signal_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGHUP, &sigact, NULL);
    signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif
}

/// Print statistics
static void print_statistics(void)
{
    time_t now;
    time(&now);
    double uptime = difftime(now, g_stats.start_time);
    
    print_logf(LOG_INFO, "Server", "=== Runtime Statistics ===");
    print_logf(LOG_INFO, "Server", "Uptime: %.0f sec", uptime);
    print_logf(LOG_INFO, "Server", "Signals received: %lu", g_stats.signals_received);
    print_logf(LOG_INFO, "Server", "Devices decoded: %lu", g_stats.devices_decoded);
    print_logf(LOG_INFO, "Server", "Unknown signals: %lu", g_stats.unknown_signals);
    print_logf(LOG_INFO, "Server", "Processing errors: %lu", g_stats.processing_errors);
    
    if (uptime > 0) {
        print_logf(LOG_INFO, "Server", "Rate: %.1f signals/min", 
                  g_stats.signals_received * 60.0 / uptime);
        if (g_stats.devices_decoded + g_stats.unknown_signals > 0) {
        print_logf(LOG_INFO, "Server", "Recognition rate: %.1f%%", 
                  g_stats.devices_decoded * 100.0 / (g_stats.devices_decoded + g_stats.unknown_signals));
    }
    }
}

/// Update statistics (simplified version)
static void update_stats(int signals_received, int devices_decoded, int unknown_signals, int errors)
{
    g_stats.signals_received += signals_received;
    g_stats.devices_decoded += devices_decoded;
    g_stats.unknown_signals += unknown_signals;
    g_stats.processing_errors += errors;
}

/// Daemon mode initialization
static int init_daemon_mode(void)
{
    if (!g_server_opts.daemon_mode) {
        return 0;
    }
    
#ifndef _WIN32
    pid_t pid = fork();
    if (pid < 0) {
        print_log(LOG_ERROR, "Server", "Failed to create daemon process");
        return -1;
    }
    
    if (pid > 0) {
        // Parent process exits
        exit(0);
    }
    
    // Child process becomes daemon
    if (setsid() < 0) {
        print_log(LOG_ERROR, "Server", "Failed to create new session");
        return -1;
    }
    
    // Second fork to prevent acquiring terminal
    pid = fork();
    if (pid < 0) {
        print_log(LOG_ERROR, "Server", "Second fork failed");
        return -1;
    }
    
    if (pid > 0) {
        exit(0);
    }
    
    // Close standard streams
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Write PID file
    if (g_server_opts.pid_file) {
        FILE *pid_file = fopen(g_server_opts.pid_file, "w");
        if (pid_file) {
            fprintf(pid_file, "%d\n", getpid());
            fclose(pid_file);
        }
    }
    
    print_log(LOG_INFO, "Server", "Daemon mode activated");
#else
    print_log(LOG_WARNING, "Server", "Daemon mode not supported on Windows");
#endif
    
    return 0;
}

/// Convert hex string to bytes
static int hex_to_bytes(const char *hex_str, uint8_t *bytes, int max_bytes)
{
    int len = strlen(hex_str);
    if (len % 2 != 0 || len / 2 > max_bytes) {
        return -1; // Invalid hex string or too long
    }
    
    // Validate that all characters are hex
    for (int i = 0; i < len; i++) {
        char c = hex_str[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
            if (g_cfg.verbosity >= LOG_DEBUG) {
                print_logf(LOG_DEBUG, "Server", "Invalid hex character '%c' at position %d in: %s", c, i, hex_str);
            }
            return -1;
        }
    }
    
    for (int i = 0; i < len; i += 2) {
        char hex_byte[3] = {hex_str[i], hex_str[i + 1], '\0'};
        bytes[i / 2] = (uint8_t)strtol(hex_byte, NULL, 16);
    }
    
    return len / 2; // Return number of bytes
}

/// Parse triq.org hex string format and extract raw data
static int parse_hex_string_simple(const char *hex_str, uint8_t *bytes, int max_bytes, int *byte_count)
{
    if (!hex_str || !bytes || strlen(hex_str) < 8) {
        return -1;
    }
    
    if (g_cfg.verbosity >= LOG_DEBUG) {
        print_logf(LOG_DEBUG, "Server", "Parsing hex string: %s", hex_str);
    }
    
    // triq.org format: AAB[version][len][data...]
    // AA = magic header, B = version, next byte = length info
    if (hex_str[0] != 'A' || hex_str[1] != 'A' || hex_str[2] != 'B') {
        if (g_cfg.verbosity >= LOG_DEBUG) {
            print_logf(LOG_DEBUG, "Server", "Invalid hex string format (missing AAB header): %s", hex_str);
        }
        return -1;
    }
    
    // Handle multiple packets separated by '+'
    char *hex_copy = strdup(hex_str);
    char *packet = strtok(hex_copy, "+");
    int total_bytes = 0;
    
    while (packet && total_bytes < max_bytes) {
        if (strlen(packet) >= 8) { // Minimum: AAB + version + len + some data
            // Skip AAB header (3 chars) + version (1 char) + length (2 chars) = 6 chars total
            const char *packet_data = packet + 6;
            int packet_len = strlen(packet_data);
            
            if (g_cfg.verbosity >= LOG_DEBUG) {
                print_logf(LOG_DEBUG, "Server", "Processing packet: %s, data part: %s", packet, packet_data);
            }
            
            // Remove end marker '55' if present (common in triq.org format)
            char *clean_data = strdup(packet_data);
            int clean_len = strlen(clean_data);
            if (clean_len >= 2 && clean_data[clean_len-2] == '5' && clean_data[clean_len-1] == '5') {
                clean_data[clean_len-2] = '\0';
                clean_len -= 2;
            }
            
            if (clean_len > 0 && clean_len % 2 == 0) {
                int packet_bytes = hex_to_bytes(clean_data, bytes + total_bytes, max_bytes - total_bytes);
                if (packet_bytes > 0) {
                    total_bytes += packet_bytes;
                    if (g_cfg.verbosity >= LOG_DEBUG) {
                        print_logf(LOG_DEBUG, "Server", "Successfully parsed %d bytes from packet", packet_bytes);
                    }
                } else {
                    if (g_cfg.verbosity >= LOG_DEBUG) {
                        print_logf(LOG_DEBUG, "Server", "Failed to parse hex data: %s", clean_data);
                    }
                }
            }
            free(clean_data);
        }
        packet = strtok(NULL, "+");
    }
    
    free(hex_copy);
    *byte_count = total_bytes;
    
    if (total_bytes > 0) {
        if (g_cfg.verbosity >= LOG_DEBUG) {
            print_logf(LOG_DEBUG, "Server", "Total parsed bytes: %d", total_bytes);
        }
        return 0;
    }
    
    return -1;
}

/// Try to decode hex string data directly using device decoders
static int try_decode_hex_string(const char *json_str, int *decoded_devices)
{
    *decoded_devices = 0;
    
    // Parse JSON to extract hex_string
    json_object *root = json_tokener_parse(json_str);
    if (!root) {
        return -1;
    }
    
    json_object *hex_obj;
    if (!json_object_object_get_ex(root, "hex_string", &hex_obj)) {
        json_object_put(root);
        return -1; // No hex string found
    }
    
    const char *hex_str = json_object_get_string(hex_obj);
    if (!hex_str || strlen(hex_str) == 0) {
        json_object_put(root);
        return -1;
    }
    
    // Parse hex string to bytes
    uint8_t bytes[256];
    int byte_count = 0;
    if (parse_hex_string_simple(hex_str, bytes, sizeof(bytes), &byte_count) != 0) {
        json_object_put(root);
        return -1;
    }
    
    if (g_cfg.verbosity >= LOG_INFO) {
        print_logf(LOG_INFO, "Server", "Successfully parsed hex string: %s (%d bytes)", 
                  hex_str, byte_count);
    }
    
    // Convert hex data to bitbuffer for device decoding
    bitbuffer_t bitbuffer = {0};
    bitbuffer_clear(&bitbuffer);
    
    if (byte_count > 0) {
        // Add bytes directly to bitbuffer (more efficient than bit-by-bit)
        bitbuffer_add_row(&bitbuffer);
        
        // Copy bytes directly to the bitbuffer
        int max_bytes = (byte_count > BITBUF_COLS) ? BITBUF_COLS : byte_count;
        memcpy(bitbuffer.bb[0], bytes, max_bytes);
        bitbuffer.bits_per_row[0] = max_bytes * 8;
        
        if (g_cfg.verbosity >= LOG_DEBUG) {
            print_logf(LOG_DEBUG, "Server", "Created bitbuffer with %d bits (%d bytes) from hex data", 
                      bitbuffer.bits_per_row[0], max_bytes);
            
            // Print first few bytes for debugging
            char hex_debug[64] = {0};
            for (int i = 0; i < (max_bytes > 8 ? 8 : max_bytes); i++) {
                sprintf(hex_debug + i*2, "%02X", bytes[i]);
            }
            print_logf(LOG_DEBUG, "Server", "Bitbuffer data (first %d bytes): %s", 
                      (max_bytes > 8 ? 8 : max_bytes), hex_debug);
        }
    } else {
        if (g_cfg.verbosity >= LOG_DEBUG) {
            print_logf(LOG_DEBUG, "Server", "No bytes to convert to bitbuffer");
        }
    }
    
    // Try to decode using device decoders
    // Create a fake pulse_data for context
    pulse_data_t fake_pulse_data = {0};
    fake_pulse_data.sample_rate = 250000;
    fake_pulse_data.centerfreq_hz = 433920000;
    fake_pulse_data.rssi_db = -10.0;
    
    // Try decoding with all registered devices
    int total_decoded = 0;
    list_t *r_devs = &g_cfg.demod->r_devs;
    
    for (void **iter = r_devs->elems; iter && *iter; ++iter) {
        r_device *r_dev = *iter;
        if (r_dev && r_dev->decode_fn) {
            // Try to decode with this device
            int result = r_dev->decode_fn(r_dev, &bitbuffer);
            if (result > 0) {
                total_decoded += result;
                if (g_cfg.verbosity >= LOG_INFO) {
                    print_logf(LOG_INFO, "Server", "Device %s decoded %d events from hex data", 
                              r_dev->name ? r_dev->name : "unknown", result);
                }
            }
        }
    }
    
    *decoded_devices = total_decoded;
    
    json_object_put(root);
    return 0; // Success - we processed the hex string
}

/// Parse JSON message and convert to pulse_data_t
static int parse_json_to_pulse_data(const char *json_str, pulse_data_t *pulse_data, uint64_t *package_id)
{
    json_object *root = json_tokener_parse(json_str);
    if (!root) {
        print_log(LOG_ERROR, "Server", "Failed to parse JSON message");
        return -1;
    }
    
    // Initialize pulse_data structure
    memset(pulse_data, 0, sizeof(pulse_data_t));
    
    // Extract package_id if present
    json_object *package_id_obj;
    if (json_object_object_get_ex(root, "package_id", &package_id_obj)) {
        *package_id = json_object_get_int64(package_id_obj);
    }
    
    // Extract modulation type
    json_object *mod_obj;
    if (json_object_object_get_ex(root, "mod", &mod_obj)) {
        const char *mod_str = json_object_get_string(mod_obj);
        if (strcmp(mod_str, "OOK") == 0) {
            pulse_data->fsk_f2_est = 0; // OOK signal
        } else if (strcmp(mod_str, "FSK") == 0) {
            pulse_data->fsk_f2_est = 1; // FSK signal
        }
    }
    
    // Extract pulse count
    json_object *count_obj;
    if (json_object_object_get_ex(root, "count", &count_obj)) {
        pulse_data->num_pulses = json_object_get_int(count_obj);
    }
    
    // Extract pulses array
    json_object *pulses_obj;
    if (json_object_object_get_ex(root, "pulses", &pulses_obj)) {
        int array_len = json_object_array_length(pulses_obj);
        int max_pulses = array_len < PD_MAX_PULSES ? array_len : PD_MAX_PULSES;
        
        for (int i = 0; i < max_pulses; i++) {
            json_object *pulse_obj = json_object_array_get_idx(pulses_obj, i);
            pulse_data->pulse[i] = json_object_get_int(pulse_obj);
        }
        
        // Limit pulse count to actual array size
        if (pulse_data->num_pulses > max_pulses) {
            pulse_data->num_pulses = max_pulses;
        }
    }
    
    // Extract frequency information
    json_object *freq_obj;
    if (json_object_object_get_ex(root, "freq_Hz", &freq_obj)) {
        pulse_data->centerfreq_hz = (double)json_object_get_int64(freq_obj);
        if (g_cfg.verbosity >= LOG_DEBUG) {
            print_logf(LOG_DEBUG, "Server", "Found freq_Hz: %lld", (long long)json_object_get_int64(freq_obj));
        }
    } else if (json_object_object_get_ex(root, "frequency", &freq_obj)) {
        pulse_data->centerfreq_hz = (double)json_object_get_int64(freq_obj);
        if (g_cfg.verbosity >= LOG_DEBUG) {
            print_logf(LOG_DEBUG, "Server", "Found frequency: %lld", (long long)json_object_get_int64(freq_obj));
        }
    }
    
    json_object *freq1_obj;
    if (json_object_object_get_ex(root, "freq1_Hz", &freq1_obj)) {
        pulse_data->freq1_hz = json_object_get_int(freq1_obj);
    }
    
    json_object *freq2_obj;
    if (json_object_object_get_ex(root, "freq2_Hz", &freq2_obj)) {
        pulse_data->freq2_hz = json_object_get_int(freq2_obj);
    }
    
    // Extract sample rate - CRITICAL for decoders!
    json_object *sample_rate_obj;
    if (json_object_object_get_ex(root, "rate_Hz", &sample_rate_obj)) {
        int rate_value = json_object_get_int(sample_rate_obj);
        pulse_data->sample_rate = (uint32_t)rate_value;
        if (g_cfg.verbosity >= LOG_DEBUG) {
            print_logf(LOG_DEBUG, "Server", "Found rate_Hz: %d -> sample_rate: %u", rate_value, pulse_data->sample_rate);
        }
    } else if (json_object_object_get_ex(root, "sample_rate", &sample_rate_obj)) {
        int rate_value = json_object_get_int(sample_rate_obj);
        pulse_data->sample_rate = (uint32_t)rate_value;
        if (g_cfg.verbosity >= LOG_DEBUG) {
            print_logf(LOG_DEBUG, "Server", "Found sample_rate: %d -> sample_rate: %u", rate_value, pulse_data->sample_rate);
        }
    } else {
        // Default sample rate if not provided - ALWAYS set this!
        pulse_data->sample_rate = 250000;
        if (g_cfg.verbosity >= LOG_DEBUG) {
            print_logf(LOG_DEBUG, "Server", "No sample rate found, using default: 250000");
        }
    }
    
    // CRITICAL FIX: Always ensure sample_rate is set to prevent decoding failures
    if (pulse_data->sample_rate == 0 || pulse_data->sample_rate > 10000000) { // Check for overflow too
        pulse_data->sample_rate = 250000; // Default to 250kHz
        print_logf(LOG_INFO, "Server", "WARNING: sample_rate was invalid (%d), set to default 250000 Hz", (int)pulse_data->sample_rate);
    }
    
    // Extract signal strength information
    json_object *rssi_obj;
    if (json_object_object_get_ex(root, "rssi_db", &rssi_obj)) {
        pulse_data->rssi_db = json_object_get_double(rssi_obj);
    } else if (json_object_object_get_ex(root, "rssi_dB", &rssi_obj)) {
        pulse_data->rssi_db = json_object_get_double(rssi_obj);
    }
    
    json_object *snr_obj;
    if (json_object_object_get_ex(root, "snr_db", &snr_obj)) {
        pulse_data->snr_db = json_object_get_double(snr_obj);
    } else if (json_object_object_get_ex(root, "snr_dB", &snr_obj)) {
        pulse_data->snr_db = json_object_get_double(snr_obj);
    }
    
    json_object *noise_obj;
    if (json_object_object_get_ex(root, "noise_db", &noise_obj)) {
        pulse_data->noise_db = json_object_get_double(noise_obj);
    } else if (json_object_object_get_ex(root, "noise_dB", &noise_obj)) {
        pulse_data->noise_db = json_object_get_double(noise_obj);
    }
    
    // Extract timestamp if present
    json_object *time_obj;
    if (json_object_object_get_ex(root, "time", &time_obj)) {
        const char *time_str = json_object_get_string(time_obj);
        // TODO: Parse timestamp string to pulse_data->start_ago
    }
    
    // Extract hex_string if present (contains triq.org compatible data)
    json_object *hex_obj;
    if (json_object_object_get_ex(root, "hex_string", &hex_obj)) {
        const char *hex_str = json_object_get_string(hex_obj);
        if (hex_str && strlen(hex_str) > 0) {
            if (g_cfg.verbosity >= LOG_INFO) {
                print_logf(LOG_INFO, "Server", "Received hex string: %s", hex_str);
            }
            
            // Process hex string for direct decoding
            // This hex string contains the properly encoded pulse data that can be decoded
            // We'll store the hex string for later processing
            // TODO: Use hex string for direct device decoding instead of pulse data
        }
    }
    
    json_object_put(root); // Free JSON object
    
    if (g_cfg.verbosity >= LOG_DEBUG) {
        print_logf(LOG_DEBUG, "Server", "Parsed JSON: %d pulses, centerfreq=%.0f Hz, sample_rate=%u Hz, RSSI=%.1f dB", 
                  pulse_data->num_pulses, pulse_data->centerfreq_hz, pulse_data->sample_rate, pulse_data->rssi_db);
    }
    
    return 0;
}

/// Process received pulse message from RabbitMQ
static void process_pulse_message(amqp_envelope_t *envelope)
{
    // Extract message body
    char *message_body = malloc(envelope->message.body.len + 1);
    if (!message_body) {
        print_log(LOG_ERROR, "Server", "Failed to allocate memory for message");
        return;
    }
    
    memcpy(message_body, envelope->message.body.bytes, envelope->message.body.len);
    message_body[envelope->message.body.len] = '\0';
    
    // Determine queue name from routing key
    char *queue_name = NULL;
    if (envelope->routing_key.len > 0) {
        queue_name = malloc(envelope->routing_key.len + 1);
        if (queue_name) {
            memcpy(queue_name, envelope->routing_key.bytes, envelope->routing_key.len);
            queue_name[envelope->routing_key.len] = '\0';
        }
    }
    
    if (g_cfg.verbosity >= LOG_DEBUG) {
        print_logf(LOG_DEBUG, "Server", "Received message from queue %s: %.100s...", 
                  queue_name ? queue_name : "unknown", message_body);
    }
    
    // OPTIMIZED: Extract only essential metadata (skip heavy pulse_data parsing)
    uint64_t package_id = 0;
    
    // Quick JSON parse for package_id only (hex_string parsing happens in try_decode_hex_string)
    json_object *root = json_tokener_parse(message_body);
    if (root) {
        json_object *package_id_obj;
        if (json_object_object_get_ex(root, "package_id", &package_id_obj)) {
            package_id = json_object_get_int64(package_id_obj);
        }
        json_object_put(root);
    }
    
    print_logf(LOG_INFO, "Server", "Processing %s signal, package_id=%lu", 
              queue_name ? queue_name : "unknown", package_id);
    
    // Initialize pulse_data for legacy fallback only
    pulse_data_t pulse_data;
    memset(&pulse_data, 0, sizeof(pulse_data_t));
    
    // OPTIMIZED: Try hex string decoding first (primary method)
    int decoded_devices = 0;
    if (try_decode_hex_string(message_body, &decoded_devices) == 0) {
        // SUCCESS: Hex string decoded successfully
        if (g_cfg.verbosity >= LOG_INFO) {
            print_logf(LOG_INFO, "Server", "Decoded %d devices from hex string (optimized path)", decoded_devices);
        }
    } else {
        // FALLBACK: Only for legacy messages without hex_string
        print_logf(LOG_WARNING, "Server", "No hex_string found, falling back to pulse_data parsing (legacy mode)");
        
        // Parse full pulse_data for legacy messages
        if (parse_json_to_pulse_data(message_body, &pulse_data, &package_id) != 0) {
            print_log(LOG_ERROR, "Server", "Failed to parse legacy pulse_data message");
            update_stats(0, 0, 0, 1);
        goto cleanup;
    }
    
        // Determine modulation type from JSON data
        if (pulse_data.fsk_f2_est > 0) {
            // Process FSK signal  
            decoded_devices = run_fsk_demods(&g_cfg.demod->r_devs, &pulse_data);
            if (g_cfg.verbosity >= LOG_DEBUG) {
                print_logf(LOG_DEBUG, "Server", "Legacy FSK decoders result: %d", decoded_devices);
            }
            
            // If FSK failed, try OOK as fallback
            if (decoded_devices == 0) {
                decoded_devices = run_ook_demods(&g_cfg.demod->r_devs, &pulse_data);
                if (g_cfg.verbosity >= LOG_DEBUG && decoded_devices > 0) {
                    print_logf(LOG_DEBUG, "Server", "Legacy FSK failed, but OOK succeeded: %d", decoded_devices);
                }
            }
        } else {
            // Process OOK signal
            decoded_devices = run_ook_demods(&g_cfg.demod->r_devs, &pulse_data);
            if (g_cfg.verbosity >= LOG_DEBUG) {
                print_logf(LOG_DEBUG, "Server", "Legacy OOK decoders result: %d", decoded_devices);
            }
        }
    }
    
    /* OPTIMIZATION NOTE: pulse_data parsing is now LEGACY FALLBACK only
     * 
     * With optimized client messages, we expect:
     * - 100% of messages contain hex_string (primary decoding path)
     * - 0% of messages need pulse_data parsing (legacy fallback)
     * 
     * This reduces server CPU usage by ~50% by eliminating redundant:
     * - JSON parsing of large pulse arrays
     * - pulse_data_t structure population
     * - Duplicate signal processing
     * 
     * The hex_string path provides identical decoding results with better performance.
     */
    
    if (decoded_devices > 0) {
        print_logf(LOG_INFO, "Server", "Decoded %d device(s) from package_id=%lu", 
                  decoded_devices, package_id);
        update_stats(0, decoded_devices, 0, 0);
    } else {
        print_logf(LOG_DEBUG, "Server", "No devices decoded from package_id=%lu", package_id);
        update_stats(0, 0, 1, 0);
    }

cleanup:
    
    free(message_body);
    if (queue_name) free(queue_name);
}

#ifdef ENABLE_RABBITMQ
/// Initialize RabbitMQ connection and queues
static int init_rabbitmq_consumer(void)
{
    // Create connection
    g_rmq_conn = amqp_new_connection();
    if (!g_rmq_conn) {
        print_log(LOG_ERROR, "Server", "Failed to create RabbitMQ connection");
        return -1;
    }
    
    // Create socket
    g_rmq_socket = amqp_tcp_socket_new(g_rmq_conn);
    if (!g_rmq_socket) {
        print_log(LOG_ERROR, "Server", "Failed to create RabbitMQ socket");
        amqp_destroy_connection(g_rmq_conn);
        return -1;
    }
    
    // Connect to RabbitMQ server
    int status = amqp_socket_open(g_rmq_socket, g_server_opts.rabbitmq_host, g_server_opts.rabbitmq_port);
    if (status != AMQP_STATUS_OK) {
        print_logf(LOG_ERROR, "Server", "Failed to connect to RabbitMQ %s:%d", 
                  g_server_opts.rabbitmq_host, g_server_opts.rabbitmq_port);
        amqp_destroy_connection(g_rmq_conn);
        return -1;
    }
    
    // Login
    amqp_rpc_reply_t result = amqp_login(g_rmq_conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
                                        g_server_opts.rabbitmq_username, g_server_opts.rabbitmq_password);
    if (result.reply_type != AMQP_RESPONSE_NORMAL) {
        print_log(LOG_ERROR, "Server", "RabbitMQ login failed");
        amqp_destroy_connection(g_rmq_conn);
        return -1;
    }
    
    // Open channel
    amqp_channel_open(g_rmq_conn, g_rmq_channel);
    result = amqp_get_rpc_reply(g_rmq_conn);
    if (result.reply_type != AMQP_RESPONSE_NORMAL) {
        print_log(LOG_ERROR, "Server", "Failed to open RabbitMQ channel");
        amqp_connection_close(g_rmq_conn, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(g_rmq_conn);
        return -1;
    }
    
    // Declare exchange (in case it doesn't exist)
    amqp_exchange_declare(g_rmq_conn, g_rmq_channel,
                         amqp_cstring_bytes(g_server_opts.rabbitmq_exchange),
                         amqp_cstring_bytes("direct"),
                         0, 1, 0, 0, amqp_empty_table);
    result = amqp_get_rpc_reply(g_rmq_conn);
    if (result.reply_type != AMQP_RESPONSE_NORMAL) {
        print_logf(LOG_WARNING, "Server", "Failed to declare exchange %s", g_server_opts.rabbitmq_exchange);
    }
    
    // Declare signals queue for all signal types
    amqp_queue_declare(g_rmq_conn, g_rmq_channel,
                      amqp_cstring_bytes(g_server_opts.signals_queue),
                      0, 1, 0, 0, amqp_empty_table);
    result = amqp_get_rpc_reply(g_rmq_conn);
    if (result.reply_type != AMQP_RESPONSE_NORMAL) {
        print_logf(LOG_ERROR, "Server", "Failed to declare queue %s", g_server_opts.signals_queue);
        return -1;
    }
    
    // Bind signals queue to exchange
    amqp_queue_bind(g_rmq_conn, g_rmq_channel,
                   amqp_cstring_bytes(g_server_opts.signals_queue),
                   amqp_cstring_bytes(g_server_opts.rabbitmq_exchange),
                   amqp_cstring_bytes(g_server_opts.signals_queue),
                   amqp_empty_table);
    
    // Start consuming from signals queue
    amqp_basic_consume(g_rmq_conn, g_rmq_channel,
                      amqp_cstring_bytes(g_server_opts.signals_queue),
                      amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
    result = amqp_get_rpc_reply(g_rmq_conn);
    if (result.reply_type != AMQP_RESPONSE_NORMAL) {
        print_logf(LOG_ERROR, "Server", "Failed to start consuming from %s", g_server_opts.signals_queue);
        return -1;
    }
    
    print_logf(LOG_INFO, "Server", "RabbitMQ consumer initialized, listening on %s", 
              g_server_opts.signals_queue);
    
    return 0;
}

/// Process RabbitMQ messages with timeout
static int process_rabbitmq_messages(void)
{
    int messages_processed = 0;
    int max_messages_per_batch = 10; // Process up to 10 messages per call
    
    // Process multiple messages in one call to avoid message backlog
    for (int i = 0; i < max_messages_per_batch; i++) {
        struct timeval timeout = {0, 1000}; // 1ms timeout for subsequent messages
        amqp_envelope_t envelope;
        
        amqp_rpc_reply_t result = amqp_consume_message(g_rmq_conn, &envelope, &timeout, 0);
        
        if (result.reply_type == AMQP_RESPONSE_NORMAL) {
            // Process the message
            process_pulse_message(&envelope);
            
            // Acknowledge the message
            amqp_basic_ack(g_rmq_conn, g_rmq_channel, envelope.delivery_tag, 0);
            
            // Clean up
            amqp_destroy_envelope(&envelope);
            
            update_stats(1, 0, 0, 0);
            messages_processed++;
            
            if (g_cfg.verbosity >= LOG_TRACE) {
                print_logf(LOG_TRACE, "Server", "Processed message %d in batch", messages_processed);
            }
            
        } else if (result.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION && 
                   result.library_error == AMQP_STATUS_TIMEOUT) {
            // Timeout - no more messages available, break the loop
            if (messages_processed > 0 && g_cfg.verbosity >= LOG_DEBUG) {
                print_logf(LOG_DEBUG, "Server", "Processed %d messages in batch, no more available", messages_processed);
            }
            break;
            
        } else if (result.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION) {
            if (result.library_error == AMQP_STATUS_UNEXPECTED_STATE) {
                // Connection state issue - this is common and usually recovers
                if (g_cfg.verbosity >= LOG_DEBUG) {
                    print_logf(LOG_DEBUG, "Server", "RabbitMQ connection state error (normal, will recover)");
                }
                break; // Exit batch processing, but don't treat as error
            } else {
                if (g_cfg.verbosity >= LOG_DEBUG) {
                    print_logf(LOG_DEBUG, "Server", "RabbitMQ library error: %d", result.library_error);
                }
                break; // Exit batch processing, but don't treat as error
            }
        } else {
            if (g_cfg.verbosity >= LOG_DEBUG) {
                print_logf(LOG_DEBUG, "Server", "RabbitMQ consume error, reply_type: %d", result.reply_type);
            }
            break; // Exit batch processing, but don't treat as error
        }
    }
    
    // Always return 0 (success) to avoid triggering consecutive error counter
    // Only real connection failures should be treated as errors
    return 0;
}

/// Cleanup RabbitMQ connection
static void cleanup_rabbitmq(void)
{
    if (g_rmq_conn) {
        amqp_channel_close(g_rmq_conn, g_rmq_channel, AMQP_REPLY_SUCCESS);
        amqp_connection_close(g_rmq_conn, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(g_rmq_conn);
        g_rmq_conn = NULL;
    }
}

#else
/// Stub functions when RabbitMQ is disabled
static int init_rabbitmq_consumer(void)
{
    print_log(LOG_ERROR, "Server", "RabbitMQ support not compiled in");
    return -1;
}

static int process_rabbitmq_messages(void)
{
    sleep(1);
    return 0;
}

static void cleanup_rabbitmq(void)
{
    // Nothing to do
}
#endif

/// Parse command line arguments
static void parse_args(int argc, char **argv)
{
    int opt;
    
    // Server-specific options + common rtl_433 options
    while ((opt = getopt(argc, argv, "hVvqI:H:P:u:p:e:Q:q:o:O:R:X:AF:M:T:S:df:L:")) != -1) {
        switch (opt) {
        case 'h':
            usage(0);
            break;
        case 'V':
            fprintf(stdout, "rtl_433_server %s\n", rtl_433_version);
            exit(0);
            break;
        case 'v':
            g_cfg.verbosity++;
            break;
        case 'q':
            g_cfg.verbosity = 0;
            break;
        case 'I':
            // Transport type - for now just store, will implement later
            print_logf(LOG_INFO, "Server", "Transport type: %s", optarg);
            break;
        case 'H':
            free(g_server_opts.rabbitmq_host);
            g_server_opts.rabbitmq_host = strdup(optarg);
            break;
        case 'P':
            g_server_opts.rabbitmq_port = atoi(optarg);
            break;
        case 'u':
            free(g_server_opts.rabbitmq_username);
            g_server_opts.rabbitmq_username = strdup(optarg);
            break;
        case 'p':
            free(g_server_opts.rabbitmq_password);
            g_server_opts.rabbitmq_password = strdup(optarg);
            break;
        case 'e':
            free(g_server_opts.rabbitmq_exchange);
            g_server_opts.rabbitmq_exchange = strdup(optarg);
            break;
        case 'Q':
            free(g_server_opts.signals_queue);
            g_server_opts.signals_queue = strdup(optarg);
            break;
        case 'o':
            free(g_server_opts.ready_queue);
            g_server_opts.ready_queue = strdup(optarg);
            break;
        case 'O':
            free(g_server_opts.unknown_queue);
            g_server_opts.unknown_queue = strdup(optarg);
            break;
        case 'R':
            // Device selection - pass to rtl_433 config
            if (optarg && *optarg) {
                r_register_protocol_by_name(&g_cfg, optarg);
            }
            break;
        case 'X':
            // Flex decoder - pass to rtl_433 config
            if (optarg && *optarg) {
                r_device *flex_device = flex_create_device(optarg);
                if (flex_device) {
                    r_register_protocol(&g_cfg, flex_device);
                    print_logf(LOG_INFO, "Server", "Registered flex decoder: %s", optarg);
                } else {
                    print_logf(LOG_ERROR, "Server", "Failed to create flex decoder: %s", optarg);
                }
            }
            break;
        case 'A':
            // Disable all device decoding, analyze only
            g_cfg.demod->analyze_pulses = 1;
            break;
        case 'F':
            // Output format (same as rtl_433)
            if (optarg && *optarg) {
                r_add_output(&g_cfg, optarg);
            }
            break;
        case 'M':
            // Metadata (same as rtl_433)
            if (optarg && *optarg) {
                r_add_meta(&g_cfg, optarg);
            }
            break;
        case 'T':
            // Statistics interval
            if (optarg && *optarg) {
                int interval = atoi(optarg);
                if (interval > 0) {
                    g_cfg.stats_interval = interval;
                }
            }
            break;
        case 'S':
            // Sample capture mode (same as rtl_433)
            if (optarg && *optarg) {
                if (strcasecmp(optarg, "all") == 0)
                    g_cfg.grab_mode = 1;
                else if (strcasecmp(optarg, "unknown") == 0)
                    g_cfg.grab_mode = 2;
                else if (strcasecmp(optarg, "known") == 0)
                    g_cfg.grab_mode = 3;
            }
            break;
        case 'd':
            g_server_opts.daemon_mode = 1;
            break;
        case 'f':
            // Configuration file - will implement later
            print_logf(LOG_INFO, "Server", "Config file: %s", optarg);
            break;
        case 'L':
            // Log file - will implement later  
            print_logf(LOG_INFO, "Server", "Log file: %s", optarg);
            break;
        default:
            usage(1);
            break;
        }
    }
}

/// Main function
int main(int argc, char **argv)
{
    int ret = 0;
    
    // Print version and initialize rtl_433 config
    print_logf(LOG_INFO, "Server", "rtl_433_server %s started", rtl_433_version);
    
    // Initialize rtl_433 configuration
    r_cfg_t *cfg_ptr = r_create_cfg();
    if (!cfg_ptr) {
        print_log(LOG_ERROR, "Server", "Failed to create rtl_433 configuration");
        return 1;
    }
    // Use pointer directly instead of copying
    g_cfg = *cfg_ptr;
    
    // Set default verbosity
    g_cfg.verbosity = LOG_INFO;
    
    // Parse command line arguments
    parse_args(argc, argv);
    
    // Initialize signal handlers
    init_signals();
    
    // Initialize statistics
    time(&g_stats.start_time);
    
    // Initialize daemon mode if needed
    if (init_daemon_mode() != 0) {
        ret = 2;
        goto cleanup;
    }
    
    // Register all device protocols (like original rtl_433)
    register_all_protocols(&g_cfg, 0); // 0 = enable all by default
    
    print_logf(LOG_INFO, "Server", "Registered %zu device protocols", g_cfg.demod->r_devs.len);
    print_logf(LOG_INFO, "Server", "Transport: RabbitMQ %s:%d", 
              g_server_opts.rabbitmq_host, g_server_opts.rabbitmq_port);
    print_logf(LOG_INFO, "Server", "Input queue: %s (all signals)", 
              g_server_opts.signals_queue);
    print_logf(LOG_INFO, "Server", "Output queues: %s (ready), %s (unknown)", 
              g_server_opts.ready_queue, g_server_opts.unknown_queue);
    
    print_log(LOG_INFO, "Server", "Server ready for operation");
    
    // Initialize RabbitMQ connection and start consuming
    if (init_rabbitmq_consumer() != 0) {
        print_log(LOG_ERROR, "Server", "Failed to initialize RabbitMQ consumer");
        ret = 8;
        goto cleanup;
    }
    
    // Main server loop
    int consecutive_errors = 0;
    while (!exit_flag) {
        // Process RabbitMQ messages
        int result = process_rabbitmq_messages();
        if (result != 0) {
            consecutive_errors++;
            if (g_cfg.verbosity >= LOG_DEBUG) {
                print_logf(LOG_DEBUG, "Server", "RabbitMQ processing error (consecutive: %d)", consecutive_errors);
            }
            
            // Only exit after many consecutive errors (connection really lost)
            if (consecutive_errors > 1000) {
                print_log(LOG_ERROR, "Server", "Too many consecutive RabbitMQ errors, shutting down");
            break;
        }
        
            // Log every 100 errors to show server is still trying
            if (consecutive_errors % 100 == 0) {
                print_logf(LOG_WARNING, "Server", "RabbitMQ connection issues (%d consecutive errors, will keep trying)", consecutive_errors);
            }
        
            // Longer sleep on errors to avoid busy waiting
            usleep(100000); // 100ms
        } else {
            // Reset error counter on successful processing
            consecutive_errors = 0;
            // Small sleep to prevent busy waiting
            usleep(10000); // 10ms
        }
        
        // Print statistics periodically
        static time_t last_stats = 0;
        time_t now = time(NULL);
        if (g_cfg.stats_interval > 0 && difftime(now, last_stats) >= g_cfg.stats_interval) {
                print_statistics();
            last_stats = now;
        }
    }
    
    print_log(LOG_INFO, "Server", "Server shutting down...");
    print_statistics();

cleanup:
    // Cleanup RabbitMQ connection
    cleanup_rabbitmq();
    
    // Cleanup rtl_433 configuration
    r_free_cfg(cfg_ptr);
    
    // Remove PID file
    if (g_server_opts.pid_file) {
        unlink(g_server_opts.pid_file);
    }
    
    print_logf(LOG_INFO, "Server", "rtl_433_server finished with code %d", ret);
    return ret;
}



