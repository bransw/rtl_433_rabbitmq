/** @file
    Device decoder interface for rtl_433_server.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef DEVICE_DECODER_H
#define DEVICE_DECODER_H

#include "pulse_data.h"
#include "data.h"

/// Device decoder callbacks
typedef struct device_decoder_callbacks {
    void (*device_decoded)(const char *source_id, const char *device_name, 
                          int device_id, data_t *device_data, float confidence,
                          const pulse_data_t *pulse_data, void *user_data);
    void (*unknown_signal)(const char *source_id, const pulse_data_t *pulse_data,
                          const char *raw_data_hex, float signal_strength,
                          const char *analysis_notes, void *user_data);
    void *user_data;
} device_decoder_callbacks_t;

/// Device decoder structure
typedef struct device_decoder {
    device_decoder_callbacks_t callbacks;
    void *decoder_data;
} device_decoder_t;

/// Initialize device decoder
int device_decoder_init(device_decoder_t *decoder, device_decoder_callbacks_t *callbacks);

/// Decode pulse data
int device_decoder_decode(device_decoder_t *decoder, const char *source_id, 
                         const pulse_data_t *pulse_data);

/// Cleanup device decoder
void device_decoder_cleanup(device_decoder_t *decoder);

#endif // DEVICE_DECODER_H
