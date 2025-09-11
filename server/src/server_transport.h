/** @file
    Server transport layer for rtl_433_server.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef SERVER_TRANSPORT_H
#define SERVER_TRANSPORT_H

#include "server_config.h"

/// Transport callbacks structure
typedef struct server_transport_callbacks {
    void (*signal_received)(const char *source_id, const char *json_data, void *user_data);
    void *user_data;
} server_transport_callbacks_t;

/// Server transport structure
typedef struct server_transport {
    server_config_t *config;
    server_transport_callbacks_t callbacks;
    int running;
    void *transport_data;
} server_transport_t;

/// Initialize server transport
int server_transport_init(server_transport_t *transport, server_config_t *config, 
                         server_transport_callbacks_t *callbacks);

/// Start server transport
int server_transport_start(server_transport_t *transport);

/// Stop server transport
void server_transport_stop(server_transport_t *transport);

/// Check if transport is running
int server_transport_is_running(const server_transport_t *transport);

/// Cleanup server transport
void server_transport_cleanup(server_transport_t *transport);

#endif // SERVER_TRANSPORT_H
