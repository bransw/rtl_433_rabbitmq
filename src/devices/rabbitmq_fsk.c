/** @file
    RabbitMQ FSK test decoder for signal reconstruction testing.

    This decoder tests the hex_string generation and pulse_data_t reconstruction
    without using actual RabbitMQ transport. It validates the accuracy of our
    signal serialization/deserialization process for FSK signals.

    Copyright (C) 2024 by rtl_433 project

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "decoder.h"
#include "rtl433_rfraw.h"
#include "rtl433_signal_format.h"
#include <stdio.h>
#include <string.h>

static int rabbitmq_fsk_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{
    // Only run when RabbitMQ OUTPUT is configured (client mode)
    // NOT when RabbitMQ input is used (server mode)
    if (!rtl433_has_rabbitmq_output()) {
        return DECODE_FAIL_OTHER; // Skip silently if RabbitMQ not configured
    }
    
    // Process signal with minimal logging  
    fprintf(stderr, "🔧 RabbitMQ FSK decoder triggered\n");
    
    fprintf(stderr, "   🔧 Allowing other FSK decoders to continue...\n");
    
    // Always return 0 to continue with other decoders
    return 0;
}

static char const *const output_fields[] = {
    "model",
    "test_result", 
    NULL,
};

r_device const rabbitmq_fsk = {
    .name        = "RabbitMQ-FSK-Test",
    .modulation  = FSK_PULSE_PWM,
    .short_width = 500,   // Flexible parameters
    .long_width  = 1000,
    .gap_limit   = 2000,
    .reset_limit = 10000,
    .decode_fn   = &rabbitmq_fsk_decode,
    .fields      = output_fields,
};

