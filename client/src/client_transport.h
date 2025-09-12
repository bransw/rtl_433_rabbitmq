/** @file
    Transport layer for rtl_433_client.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef CLIENT_TRANSPORT_H
#define CLIENT_TRANSPORT_H

#include "client_config.h"
#include "pulse_data.h"
#include "bitbuffer.h"
#include <time.h>

/// Structure for transmitting demodulated data
typedef struct demod_data {
    char *device_id;           // Source device identifier
    uint64_t timestamp_us;     // Timestamp in microseconds
    uint32_t frequency;        // Reception frequency
    uint32_t sample_rate;      // Sample rate
    float rssi_db;             // Signal level in dB
    float snr_db;              // Signal-to-noise ratio in dB
    float noise_db;            // Noise level in dB
    
    // Pulse data
    pulse_data_t pulse_data;
    
    // Alternatively - bit buffer (for FSK)
    bitbuffer_t *bitbuffer;
    
    // Metadata
    int ook_detected;          // 1 if OOK signal detected
    int fsk_detected;          // 1 if FSK signal detected
    char *raw_data_hex;        // Raw data in hex (optional)
    unsigned long package_id;  // Package ID for linking with raw data
} demod_data_t;

/// Transport connection structure
typedef struct transport_connection {
    transport_config_t *config;
    void *connection_data;     // Pointer to transport-specific data
    int connected;
    time_t last_reconnect_attempt;
} transport_connection_t;

/// Initialize transport connection
int transport_init(transport_connection_t *conn, transport_config_t *config);

/// Connect to server
int transport_connect(transport_connection_t *conn);

/// Disconnect from server
void transport_disconnect(transport_connection_t *conn);

/// Send demodulated data
int transport_send_demod_data(transport_connection_t *conn, const demod_data_t *data);
int transport_send_demod_data_to_queue(transport_connection_t *conn, const demod_data_t *data, const char *queue_name);

/// Send pulse data
int transport_send_pulse_data(transport_connection_t *conn, const pulse_data_t *pulse_data);
int transport_send_pulse_data_to_queue(transport_connection_t *conn, const pulse_data_t *pulse_data, const char *queue_name);
int transport_send_pulse_data_with_id(transport_connection_t *conn, const pulse_data_t *pulse_data, const char *queue_name, unsigned long package_id);

/// Send batch of demodulated data
int transport_send_demod_batch(transport_connection_t *conn, const demod_data_t *data_array, int count);

/// Check connection status
int transport_is_connected(const transport_connection_t *conn);

/// Free transport resources
void transport_cleanup(transport_connection_t *conn);

/// Create demod_data structure
demod_data_t *demod_data_create(void);

/// Free demod_data structure
void demod_data_free(demod_data_t *data);

/// Copy pulse_data to demod_data
void demod_data_set_pulse_data(demod_data_t *data, const pulse_data_t *pulse_data);

/// Copy bitbuffer to demod_data
void demod_data_set_bitbuffer(demod_data_t *data, const bitbuffer_t *bitbuffer);

/// Serialize demod_data to JSON
char *demod_data_to_json(const demod_data_t *data);

/// Serialize demod_data array to JSON
char *demod_data_batch_to_json(const demod_data_t *data_array, int count);

// Transport-specific functions

#ifdef ENABLE_MQTT
/// MQTT specific functions
int transport_mqtt_init(transport_connection_t *conn);
int transport_mqtt_connect(transport_connection_t *conn);
int transport_mqtt_send(transport_connection_t *conn, const char *json_data);
void transport_mqtt_cleanup(transport_connection_t *conn);
#endif

/// HTTP specific functions
int transport_http_init(transport_connection_t *conn);
int transport_http_send(transport_connection_t *conn, const char *json_data);
void transport_http_cleanup(transport_connection_t *conn);

#ifdef ENABLE_RABBITMQ
/// RabbitMQ specific functions
int transport_rabbitmq_init(transport_connection_t *conn);
int transport_rabbitmq_connect(transport_connection_t *conn);
int transport_rabbitmq_send(transport_connection_t *conn, const char *json_data);
void transport_rabbitmq_cleanup(transport_connection_t *conn);
#endif

/// TCP/UDP specific functions
int transport_socket_init(transport_connection_t *conn);
int transport_socket_connect(transport_connection_t *conn);
int transport_socket_send(transport_connection_t *conn, const char *json_data);
void transport_socket_cleanup(transport_connection_t *conn);

#endif // CLIENT_TRANSPORT_H

