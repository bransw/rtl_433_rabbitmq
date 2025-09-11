/** @file
    Device decoder implementation.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "device_decoder.h"
#include <stdlib.h>
#include <string.h>

/// Initialize device decoder
int device_decoder_init(device_decoder_t *decoder, device_decoder_callbacks_t *callbacks)
{
    if (!decoder || !callbacks) {
        return -1;
    }
    
    memset(decoder, 0, sizeof(device_decoder_t));
    decoder->callbacks = *callbacks;
    
    return 0;
}

/// Decode pulse data
int device_decoder_decode(device_decoder_t *decoder, const char *source_id, 
                         const pulse_data_t *pulse_data)
{
    if (!decoder || !source_id || !pulse_data) {
        return -1;
    }
    
    // TODO: Implement actual device decoding using rtl_433 decoders
    // For now, treat all signals as unknown
    
    if (decoder->callbacks.unknown_signal) {
        decoder->callbacks.unknown_signal(source_id, pulse_data, NULL, 
                                         pulse_data->rssi_db, 
                                         "No decoder implementation", 
                                         decoder->callbacks.user_data);
    }
    
    return 0;
}

/// Cleanup device decoder
void device_decoder_cleanup(device_decoder_t *decoder)
{
    if (!decoder) {
        return;
    }
    
    memset(decoder, 0, sizeof(device_decoder_t));
}
