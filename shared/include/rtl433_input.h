/** @file
 * RabbitMQ input module for rtl_433
 * 
 * This module provides functionality to read pulse data from RabbitMQ queues
 * using the existing rtl433_transport infrastructure.
 */

#ifndef RTL433_INPUT_H
#define RTL433_INPUT_H

#include "rtl433_transport.h"
#include "pulse_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/// RabbitMQ input configuration
typedef struct {
    rtl433_transport_connection_t *conn;
    char *queue_name;
    rtl433_message_handler_t message_handler;
    void *user_data;
    volatile bool running;
    int timeout_ms;
} rtl433_input_config_t;

/// Callback function type for processing received pulse data
typedef void (*rtl433_input_pulse_handler_t)(pulse_data_t *pulse_data, void *user_data);

// === MAIN INPUT FUNCTIONS ===

/// Initialize RabbitMQ input from URL
/// URL format: rabbitmq://user:pass@host:port/queue_name
int rtl433_input_init_from_url(rtl433_input_config_t *input_config, 
                               const char *url,
                               rtl433_input_pulse_handler_t pulse_handler,
                               void *user_data);

/// Start reading messages from RabbitMQ queue
int rtl433_input_start_reading(rtl433_input_config_t *input_config);

/// Stop reading messages
void rtl433_input_stop_reading(rtl433_input_config_t *input_config);

/// Read one message (blocking)
int rtl433_input_read_message(rtl433_input_config_t *input_config, int timeout_ms);

/// Cleanup input configuration
void rtl433_input_cleanup(rtl433_input_config_t *input_config);

// === UTILITY FUNCTIONS ===

/// Check if URL is a RabbitMQ input URL
bool rtl433_input_is_rabbitmq_url(const char *url);

/// Parse pulse data from JSON message
pulse_data_t* rtl433_input_parse_pulse_data_from_json(const char *json_str);

/// Convert received message to pulse_data
pulse_data_t* rtl433_input_message_to_pulse_data(rtl433_message_t *message);

#ifdef __cplusplus
}
#endif

#endif // RTL433_INPUT_H


