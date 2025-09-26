/** @file
 * RabbitMQ input implementation for rtl_433
 */

#include "rtl433_input.h"
#include "rtl433_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>

/// Internal message handler wrapper
static void internal_message_handler(rtl433_message_t *message, void *user_data);

/// Parse pulse data from JSON string
static pulse_data_t* parse_pulse_data_from_json_internal(const char *json_str);

/// Convert JSON object to pulse_data_t
static int json_to_pulse_data(json_object *json_obj, pulse_data_t *pulse_data);

int rtl433_input_init_from_url(rtl433_input_config_t *input_config, 
                               const char *url,
                               rtl433_input_pulse_handler_t pulse_handler,
                               void *user_data)
{
    if (!input_config || !url || !pulse_handler) {
        printf("❌ Invalid parameters for RabbitMQ input initialization\n");
        return -1;
    }

    printf("🐰 Initializing RabbitMQ input from URL: %s\n", url);

    // Initialize input config
    memset(input_config, 0, sizeof(rtl433_input_config_t));
    
    // Allocate connection
    input_config->conn = malloc(sizeof(rtl433_transport_connection_t));
    if (!input_config->conn) {
        printf("❌ Failed to allocate transport connection\n");
        return -1;
    }
    memset(input_config->conn, 0, sizeof(rtl433_transport_connection_t));

    // Allocate transport config
    rtl433_transport_config_t *config = malloc(sizeof(rtl433_transport_config_t));
    if (!config) {
        printf("❌ Failed to allocate transport config\n");
        free(input_config->conn);
        return -1;
    }

    // Initialize and parse URL
    if (rtl433_transport_config_init(config) != 0) {
        printf("❌ Failed to initialize transport config\n");
        free(config);
        free(input_config->conn);
        return -1;
    }

    if (rtl433_transport_parse_url(url, config) != 0) {
        printf("❌ Failed to parse RabbitMQ URL: %s\n", url);
        rtl433_transport_config_free(config);
        free(config);
        free(input_config->conn);
        return -1;
    }

    // Store queue name
    if (config->queue) {
        input_config->queue_name = strdup(config->queue);
        printf("📥 Target queue: %s\n", input_config->queue_name);
    } else {
        printf("❌ No queue specified in URL\n");
        rtl433_transport_config_free(config);
        free(config);
        free(input_config->conn);
        return -1;
    }

    // Connect to RabbitMQ
    if (rtl433_transport_connect(input_config->conn, config) != 0) {
        printf("❌ Failed to connect to RabbitMQ\n");
        free(input_config->queue_name);
        rtl433_transport_config_free(config);
        free(config);
        free(input_config->conn);
        return -1;
    }

    printf("✅ Connected to RabbitMQ: %s:%d\n", config->host, config->port);

    // Store configuration
    input_config->user_data = user_data;
    input_config->running = false;
    input_config->timeout_ms = 5000; // 5 second timeout
    
    // Create wrapper for pulse handler
    typedef struct {
        rtl433_input_pulse_handler_t pulse_handler;
        void *user_data;
    } handler_wrapper_t;
    
    handler_wrapper_t *wrapper = malloc(sizeof(handler_wrapper_t));
    if (!wrapper) {
        printf("❌ Failed to allocate handler wrapper\n");
        rtl433_input_cleanup(input_config);
        return -1;
    }
    wrapper->pulse_handler = pulse_handler;
    wrapper->user_data = user_data;
    
    input_config->message_handler = internal_message_handler;
    input_config->user_data = wrapper;

    // DON'T free config! It's used by the connection
    // rtl433_transport_config_free(config);
    // free(config);

    printf("🎯 RabbitMQ input initialized successfully\n");
    return 0;
}

int rtl433_input_start_reading(rtl433_input_config_t *input_config)
{
    if (!input_config || !input_config->conn) {
        printf("❌ Invalid input configuration\n");
        return -1;
    }

    printf("📡 Starting to read from RabbitMQ queue: %s\n", input_config->queue_name);
    
    input_config->running = true;
    
    while (input_config->running) {
        int result = rtl433_transport_receive_messages(
            input_config->conn,
            input_config->message_handler,
            input_config->user_data,
            input_config->timeout_ms
        );
        
        if (result < 0) {
            printf("⚠️ Error receiving messages, attempting reconnect...\n");
            
            // Try to reconnect
            if (rtl433_transport_reconnect(input_config->conn) != 0) {
                printf("❌ Failed to reconnect to RabbitMQ\n");
                return -1;
            }
            
            printf("✅ Reconnected to RabbitMQ\n");
            sleep(1); // Brief pause before retrying
        }
        
        // Check connection status
        if (!rtl433_transport_is_connected(input_config->conn)) {
            printf("⚠️ Connection lost, attempting reconnect...\n");
            if (rtl433_transport_reconnect(input_config->conn) != 0) {
                printf("❌ Failed to reconnect\n");
                return -1;
            }
        }
    }
    
    printf("📴 Stopped reading from RabbitMQ queue\n");
    return 0;
}

void rtl433_input_stop_reading(rtl433_input_config_t *input_config)
{
    if (input_config) {
        printf("🛑 Stopping RabbitMQ input reading...\n");
        input_config->running = false;
    }
}

int rtl433_input_read_message(rtl433_input_config_t *input_config, int timeout_ms)
{
    if (!input_config || !input_config->conn) {
        return -1;
    }

    return rtl433_transport_receive_messages(
        input_config->conn,
        input_config->message_handler,
        input_config->user_data,
        timeout_ms
    );
}

void rtl433_input_cleanup(rtl433_input_config_t *input_config)
{
    if (!input_config) return;

    printf("🧹 Cleaning up RabbitMQ input...\n");

    // Stop reading if still running
    if (input_config->running) {
        rtl433_input_stop_reading(input_config);
    }

    // Disconnect transport
    if (input_config->conn) {
        rtl433_transport_disconnect(input_config->conn);
        free(input_config->conn);
        input_config->conn = NULL;
    }

    // Free queue name
    if (input_config->queue_name) {
        free(input_config->queue_name);
        input_config->queue_name = NULL;
    }

    // Free user data wrapper
    if (input_config->user_data) {
        free(input_config->user_data);
        input_config->user_data = NULL;
    }

    printf("✅ RabbitMQ input cleanup completed\n");
}

bool rtl433_input_is_rabbitmq_url(const char *url)
{
    if (!url) return false;
    
    return (strncmp(url, "rabbitmq://", 11) == 0 || 
            strncmp(url, "amqp://", 7) == 0);
}

pulse_data_t* rtl433_input_parse_pulse_data_from_json(const char *json_str)
{
    return parse_pulse_data_from_json_internal(json_str);
}

pulse_data_t* rtl433_input_message_to_pulse_data(rtl433_message_t *message)
{
    if (!message) return NULL;

    // If message already has pulse_data, return it
    if (message->pulse_data) {
        return message->pulse_data;
    }

    // If message has hex_string, try to decode it
    if (message->hex_string) {
        // TODO: Implement hex string to pulse_data conversion
        printf("📦 Message with hex_string: %s\n", message->hex_string);
    }

    return NULL;
}

// === INTERNAL FUNCTIONS ===

static void internal_message_handler(rtl433_message_t *message, void *user_data)
{
    if (!message || !user_data) return;

    typedef struct {
        rtl433_input_pulse_handler_t pulse_handler;
        void *user_data;
    } handler_wrapper_t;

    handler_wrapper_t *wrapper = (handler_wrapper_t*)user_data;
    
    printf("📨 Received message from RabbitMQ\n");

    // Convert message to pulse_data
    pulse_data_t *pulse_data = rtl433_input_message_to_pulse_data(message);
    
    if (pulse_data) {
        printf("📡 Processing pulse data: %d pulses\n", pulse_data->num_pulses);
        
        // Call user's pulse handler
        wrapper->pulse_handler(pulse_data, wrapper->user_data);
    } else {
        printf("⚠️ Failed to extract pulse data from message\n");
    }
}

static pulse_data_t* parse_pulse_data_from_json_internal(const char *json_str)
{
    if (!json_str) return NULL;

    json_object *root = json_tokener_parse(json_str);
    if (!root) {
        printf("❌ Failed to parse JSON: %s\n", json_str);
        return NULL;
    }

    pulse_data_t *pulse_data = malloc(sizeof(pulse_data_t));
    if (!pulse_data) {
        json_object_put(root);
        return NULL;
    }

    memset(pulse_data, 0, sizeof(pulse_data_t));

    if (json_to_pulse_data(root, pulse_data) != 0) {
        printf("❌ Failed to convert JSON to pulse_data\n");
        free(pulse_data);
        pulse_data = NULL;
    }

    json_object_put(root);
    return pulse_data;
}

static int json_to_pulse_data(json_object *json_obj, pulse_data_t *pulse_data)
{
    if (!json_obj || !pulse_data) return -1;

    // Parse modulation
    json_object *mod_obj;
    if (json_object_object_get_ex(json_obj, "mod", &mod_obj)) {
        const char *mod_str = json_object_get_string(mod_obj);
        if (mod_str) {
            if (strcmp(mod_str, "FSK") == 0) {
                pulse_data->fsk_f2_est = 1; // Mark as FSK
            }
            printf("📊 Modulation: %s\n", mod_str);
        }
    }

    // Parse pulse count
    json_object *count_obj;
    if (json_object_object_get_ex(json_obj, "count", &count_obj)) {
        pulse_data->num_pulses = json_object_get_int(count_obj);
        printf("📈 Pulse count: %d\n", pulse_data->num_pulses);
    }

    // Parse pulses array
    json_object *pulses_obj;
    if (json_object_object_get_ex(json_obj, "pulses", &pulses_obj)) {
        if (json_object_is_type(pulses_obj, json_type_array)) {
            int array_len = json_object_array_length(pulses_obj);
            
            if (array_len > 0 && array_len <= PD_MAX_PULSES * 2) {
                for (int i = 0; i < array_len && i < PD_MAX_PULSES * 2; i++) {
                    json_object *pulse_val = json_object_array_get_idx(pulses_obj, i);
                    if (pulse_val) {
                        pulse_data->pulse[i] = json_object_get_int(pulse_val);
                    }
                }
                printf("📊 Loaded %d pulse values\n", array_len);
            }
        }
    }

    // Parse frequency
    json_object *freq_obj;
    if (json_object_object_get_ex(json_obj, "freq_Hz", &freq_obj)) {
        pulse_data->freq1_hz = json_object_get_int(freq_obj);
        printf("📻 Frequency: %.0f Hz\n", pulse_data->freq1_hz);
    }

    // Parse sample rate
    json_object *rate_obj;
    if (json_object_object_get_ex(json_obj, "rate_Hz", &rate_obj)) {
        pulse_data->sample_rate = json_object_get_int(rate_obj);
        printf("⏱️ Sample rate: %d Hz\n", pulse_data->sample_rate);
    }

    // Parse RSSI
    json_object *rssi_obj;
    if (json_object_object_get_ex(json_obj, "rssi_dB", &rssi_obj)) {
        pulse_data->rssi_db = json_object_get_double(rssi_obj);
        printf("📶 RSSI: %.2f dB\n", pulse_data->rssi_db);
    }

    // Parse SNR
    json_object *snr_obj;
    if (json_object_object_get_ex(json_obj, "snr_dB", &snr_obj)) {
        pulse_data->snr_db = json_object_get_double(snr_obj);
        printf("📡 SNR: %.2f dB\n", pulse_data->snr_db);
    }

    return 0;
}
