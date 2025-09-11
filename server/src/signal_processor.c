/** @file
    Signal processor implementation.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "signal_processor.h"
#include "pulse_data.h"
#include <stdlib.h>
#include <string.h>

/// Initialize signal processor
int signal_processor_init(signal_processor_t *processor, server_config_t *config,
                         signal_processor_callbacks_t *callbacks)
{
    if (!processor || !config || !callbacks) {
        return -1;
    }
    
    memset(processor, 0, sizeof(signal_processor_t));
    processor->config = config;
    processor->callbacks = *callbacks;
    
    // Initialize device decoder
    device_decoder_callbacks_t decoder_callbacks = *callbacks;
    if (device_decoder_init(&processor->decoder, &decoder_callbacks) != 0) {
        return -1;
    }
    
    return 0;
}

/// Process JSON signal data
int signal_processor_process_json(signal_processor_t *processor, const char *source_id,
                                 const char *json_data)
{
    if (!processor || !source_id || !json_data) {
        return -1;
    }
    
    // TODO: Parse JSON data and extract pulse_data_t
    // For now, create a dummy pulse_data structure
    pulse_data_t pulse_data = {0};
    pulse_data.num_pulses = 0;
    pulse_data.rssi_db = -20.0;
    pulse_data.snr_db = 10.0;
    pulse_data.noise_db = -30.0;
    
    // Decode the signal
    return device_decoder_decode(&processor->decoder, source_id, &pulse_data);
}

/// Cleanup signal processor
void signal_processor_cleanup(signal_processor_t *processor)
{
    if (!processor) {
        return;
    }
    
    device_decoder_cleanup(&processor->decoder);
    memset(processor, 0, sizeof(signal_processor_t));
}
