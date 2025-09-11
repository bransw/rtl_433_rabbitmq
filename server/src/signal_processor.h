/** @file
    Signal processor for rtl_433_server.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef SIGNAL_PROCESSOR_H
#define SIGNAL_PROCESSOR_H

#include "server_config.h"
#include "device_decoder.h"

/// Signal processor callbacks
typedef device_decoder_callbacks_t signal_processor_callbacks_t;

/// Signal processor structure
typedef struct signal_processor {
    server_config_t *config;
    device_decoder_t decoder;
    signal_processor_callbacks_t callbacks;
} signal_processor_t;

/// Initialize signal processor
int signal_processor_init(signal_processor_t *processor, server_config_t *config,
                         signal_processor_callbacks_t *callbacks);

/// Process JSON signal data
int signal_processor_process_json(signal_processor_t *processor, const char *source_id,
                                 const char *json_data);

/// Cleanup signal processor
void signal_processor_cleanup(signal_processor_t *processor);

#endif // SIGNAL_PROCESSOR_H
