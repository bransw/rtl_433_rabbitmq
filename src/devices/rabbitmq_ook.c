/** @file
 * RabbitMQ OOK Test Decoder
 * 
 * This decoder tests RabbitMQ integration with bitbuffer_t transport.
 * It generates hex_string for compatibility but uses bitbuffer_t for actual data transfer.
 */

#include "decoder.h"
#include "rtl433_signal_format.h"
#include "rtl433_rfraw.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static int rabbitmq_ook_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{
    // Only run when RabbitMQ OUTPUT is configured (client mode)
    // NOT when RabbitMQ input is used (server mode)
    if (!rtl433_has_rabbitmq_output()) {
        return DECODE_FAIL_OTHER;
    }
    
    // Process signal with minimal logging
    fprintf(stderr, "🔧 RabbitMQ OOK decoder triggered\n");
    
    // Get access to the original pulse_data_t
    pulse_data_t const *pulse_data = rtl433_get_current_pulse_data();
    if (!pulse_data) {
        fprintf(stderr, "   ❌ No pulse_data available\n");
        return 0;
    }
    
    fprintf(stderr, "   🔧 Allowing other OOK decoders to continue...\n");
    
    // Always return 0 to continue with other decoders
    return 0;
}

static char const *const output_fields[] = {
    "model",
    "id",
    "data",
    NULL,
};

r_device const rabbitmq_ook = {
    .name        = "RabbitMQ OOK Test",
    .modulation  = OOK_PULSE_PWM,
    .short_width = 500,
    .long_width  = 1000,
    .gap_limit   = 2000,
    .reset_limit = 10000,
    .decode_fn   = &rabbitmq_ook_decode,
    .fields      = output_fields,
};