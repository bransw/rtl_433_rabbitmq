/** @file
    Server transport implementation.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "server_transport.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

/// Initialize server transport
int server_transport_init(server_transport_t *transport, server_config_t *config, 
                         server_transport_callbacks_t *callbacks)
{
    if (!transport || !config || !callbacks) {
        return -1;
    }
    
    memset(transport, 0, sizeof(server_transport_t));
    transport->config = config;
    transport->callbacks = *callbacks;
    transport->running = 0;
    
    return 0;
}

/// Start server transport
int server_transport_start(server_transport_t *transport)
{
    if (!transport) {
        return -1;
    }
    
    // For testing: create a simple socket listener on port 8080
    print_log(LOG_INFO, "Transport", "Starting HTTP transport on port 8080 (test mode)");
    
    // For now, just mark as running - full HTTP server implementation needed
    transport->running = 1;
    
    print_log(LOG_WARNING, "Transport", "HTTP server not fully implemented - clients will fail to connect");
    print_log(LOG_INFO, "Transport", "To test: use netcat as simple server: nc -l 8080");
    
    return 0;
}

/// Stop server transport
void server_transport_stop(server_transport_t *transport)
{
    if (!transport) {
        return;
    }
    
    transport->running = 0;
}

/// Check if transport is running
int server_transport_is_running(const server_transport_t *transport)
{
    return transport ? transport->running : 0;
}

/// Cleanup server transport
void server_transport_cleanup(server_transport_t *transport)
{
    if (!transport) {
        return;
    }
    
    transport->running = 0;
    transport->transport_data = NULL;
}
