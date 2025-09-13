/** @file
    Queue manager for rtl_433_server.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef QUEUE_MANAGER_H
#define QUEUE_MANAGER_H

#include "server_config.h"
#include "pulse_data.h"
#include "data.h"
#include <pthread.h>
#include <time.h>

/// Message types in queues
typedef enum {
    QUEUE_MSG_DECODED_DEVICE,    // Successfully decoded device
    QUEUE_MSG_UNKNOWN_SIGNAL,    // Unknown signal
    QUEUE_MSG_RAW_PULSE_DATA,    // Raw pulse data
    QUEUE_MSG_STATISTICS,        // Statistics
    QUEUE_MSG_ERROR,             // Error message
    QUEUE_MSG_HEARTBEAT          // Heartbeat message
} queue_message_type_t;

/// Queue message structure
typedef struct queue_message {
    queue_message_type_t type;
    uint64_t timestamp_us;
    char *source_id;             // Source identifier (client)
    
    union {
        // For decoded devices
        struct {
            char *device_name;
            int device_id;
            data_t *device_data;     // Decoded device data
            float confidence;        // Decoding confidence (0.0-1.0)
            pulse_data_t pulse_data; // Original pulse data
        } decoded_device;
        
        // For unknown signals
        struct {
            pulse_data_t pulse_data;
            char *raw_data_hex;      // Raw data in hex
            float signal_strength;   // Signal strength
            char *analysis_notes;    // Analysis notes
        } unknown_signal;
        
        // For statistics
        struct {
            char *stats_json;
        } statistics;
        
        // For errors
        struct {
            int error_code;
            char *error_message;
        } error;
    } data;
    
    struct queue_message *next;  // For linked list
} queue_message_t;

/// Queue structure
typedef struct message_queue {
    char *name;
    output_queue_config_t *config;
    
    // Message queue
    queue_message_t *head;
    queue_message_t *tail;
    int size;
    int max_size;
    
    // Synchronization
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    
    // Statistics
    uint64_t messages_sent;
    uint64_t messages_dropped;
    uint64_t bytes_sent;
    time_t created_time;
    time_t last_message_time;
    
    // State
    int active;
    int connected;
    void *transport_data;        // Transport data
    
    // Batch processing
    queue_message_t **batch_buffer;
    int batch_size;
    int batch_count;
    time_t batch_start_time;
    
    pthread_t worker_thread;
} message_queue_t;

/// Queue manager
typedef struct queue_manager {
    message_queue_t *ready_queue;     // Queue for ready devices
    message_queue_t *unknown_queue;   // Queue for unknown signals
    
    list_t additional_queues;         // Additional queues
    
    // Configuration
    server_config_t *config;
    
    // Synchronization
    pthread_mutex_t manager_mutex;
    
    // Statistics
    uint64_t total_messages_processed;
    uint64_t total_bytes_processed;
    time_t start_time;
    
    int shutdown_requested;
} queue_manager_t;

/// Initialize queue manager
int queue_manager_init(queue_manager_t *manager, server_config_t *config);

/// Start queue manager
int queue_manager_start(queue_manager_t *manager);

/// Stop queue manager
void queue_manager_stop(queue_manager_t *manager);

/// Free queue manager resources
void queue_manager_cleanup(queue_manager_t *manager);

/// Check if queue manager is running
int queue_manager_is_running(const queue_manager_t *manager);

/// Send message to ready devices queue
int queue_manager_send_decoded_device(queue_manager_t *manager, 
                                     const char *source_id,
                                     const char *device_name,
                                     int device_id,
                                     data_t *device_data,
                                     float confidence,
                                     const pulse_data_t *pulse_data);

/// Send message to unknown signals queue
int queue_manager_send_unknown_signal(queue_manager_t *manager,
                                     const char *source_id,
                                     const pulse_data_t *pulse_data,
                                     const char *raw_data_hex,
                                     float signal_strength,
                                     const char *analysis_notes);

/// Send statistics
int queue_manager_send_statistics(queue_manager_t *manager,
                                 const char *source_id,
                                 const char *stats_json);

/// Send error message
int queue_manager_send_error(queue_manager_t *manager,
                            const char *source_id,
                            int error_code,
                            const char *error_message);

/// Get queue statistics
char *queue_manager_get_statistics(queue_manager_t *manager);

/// Functions for working with individual queues

/// Create queue
message_queue_t *message_queue_create(const char *name, output_queue_config_t *config);

/// Destroy queue
void message_queue_destroy(message_queue_t *queue);

/// Send message to queue
int message_queue_send(message_queue_t *queue, queue_message_t *message);

/// Get message from queue (blocking call)
queue_message_t *message_queue_receive(message_queue_t *queue, int timeout_ms);

/// Check queue size
int message_queue_size(message_queue_t *queue);

/// Clear queue
void message_queue_clear(message_queue_t *queue);

/// Functions for working with messages

/// Create message
queue_message_t *queue_message_create(queue_message_type_t type);

/// Destroy message
void queue_message_destroy(queue_message_t *message);

/// Serialize message to JSON
char *queue_message_to_json(const queue_message_t *message);

/// Deserialize message from JSON
queue_message_t *queue_message_from_json(const char *json);

/// Copy message
queue_message_t *queue_message_copy(const queue_message_t *message);

#endif // QUEUE_MANAGER_H

