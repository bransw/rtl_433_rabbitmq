/*
 * Test program to decode RTL433Message from hex data
 * and restore pulses from hex_string
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Include our ASN.1 pulse library
#include "shared/include/rtl433_asn1_pulse.h"

// ASN.1 generated headers
#include "RTL433Message.h"
#include "SignalMessage.h"
#include "PulsesData.h" 
#include "SignalData.h"
#include "ModulationType.h"
#include "RFParameters.h"
#include "SignalQuality.h"
#include "TimingInfo.h"
#include "asn_application.h"
#include "per_encoder.h"
#include "per_decoder.h"

// Function to convert hex string to binary data
unsigned char* hex_to_binary_data(const char *hex_string, size_t *binary_size) {
    if (!hex_string || !binary_size) {
        return NULL;
    }
    
    size_t hex_len = strlen(hex_string);
    if (hex_len % 2 != 0) {
        fprintf(stderr, "❌ Hex string length must be even\n");
        return NULL;
    }
    
    *binary_size = hex_len / 2;
    unsigned char *binary = malloc(*binary_size);
    if (!binary) {
        fprintf(stderr, "❌ Memory allocation failed\n");
        return NULL;
    }
    
    for (size_t i = 0; i < *binary_size; i++) {
        char hex_byte[3] = {hex_string[i*2], hex_string[i*2+1], '\0'};
        binary[i] = (unsigned char)strtol(hex_byte, NULL, 16);
    }
    
    return binary;
}

int main() {
    printf("🔄 RTL433Message Decoder Test Program\n");
    printf("=====================================\n\n");
    
    // Test hex data provided by user
    const char *hex_data = "1D9648020022555882001800300064004640404040D0C840C048C840C8C840C0404840C048C8C048C8C048C8404040C8C04048404040C040404040404040404048C8404040C8C8C84040AA9C33BA2FFE33BAE13E33B8EABE0007A11FC05C0F1015B6B0680EC0068F06F05C0F00945DDFE0001707200008F8E000080000F2244852E7EAAB80";
    
    printf("📦 Input hex data (%zu chars):\n%s\n\n", strlen(hex_data), hex_data);
    
    // Convert hex to binary
    size_t binary_size;
    unsigned char *binary_data = hex_to_binary_data(hex_data, &binary_size);
    if (!binary_data) {
        fprintf(stderr, "❌ Failed to convert hex to binary\n");
        return 1;
    }
    
    printf("📦 Binary data size: %zu bytes\n\n", binary_size);
    
    // Create ASN.1 buffer structure
    rtl433_asn1_buffer_t buffer = {
        .buffer = binary_data,
        .buffer_size = binary_size,
        .result = RTL433_ASN1_OK
    };
    
    // Decode the RTL433Message
    printf("🔄 Decoding RTL433Message...\n");
    pulse_data_t *decoded_pulse_data = decode(&buffer);
    
    if (!decoded_pulse_data) {
        fprintf(stderr, "❌ Failed to decode RTL433Message\n");
        free(binary_data);
        return 1;
    }
    
    printf("✅ RTL433Message decoded successfully!\n\n");
    
    // Print decoded pulse data
    printf("📊 Decoded Pulse Data:\n");
    print_pulse_data(decoded_pulse_data);
    printf("\n");
    
    // Now let's try to decode as pulse_data_ex to get hex_string
    printf("🔄 Attempting to extract pulse_data_ex with hex_string...\n");
    
    // Decode RTL433Message directly to get access to hex_string
    RTL433Message_t *rtl433_msg = NULL;
    asn_dec_rval_t decode_result = uper_decode_complete(
        NULL, &asn_DEF_RTL433Message, (void**)&rtl433_msg, 
        buffer.buffer, buffer.buffer_size);
    
    if (decode_result.code != RC_OK || !rtl433_msg) {
        fprintf(stderr, "❌ Failed to decode RTL433Message for hex_string extraction\n");
        free(decoded_pulse_data);
        free(binary_data);
        return 1;
    }
    
    // Extract pulse_data_ex
    pulse_data_ex_t *pulse_data_ex = extract_pulse_data(rtl433_msg);
    if (!pulse_data_ex) {
        fprintf(stderr, "❌ Failed to extract pulse_data_ex\n");
        ASN_STRUCT_FREE(asn_DEF_RTL433Message, rtl433_msg);
        free(decoded_pulse_data);
        free(binary_data);
        return 1;
    }
    
    printf("✅ pulse_data_ex extracted successfully!\n\n");
    
    // Print extended pulse data
    printf("📊 Extended Pulse Data:\n");
    print_pulse_data_ex(pulse_data_ex);
    printf("\n");
    
    // Check if we have hex_string for pulse reconstruction
    const char *hex_string = pulse_data_ex_get_hex_string(pulse_data_ex);
    if (hex_string && strlen(hex_string) > 0) {
        printf("🔄 Found hex_string, attempting pulse reconstruction...\n");
        printf("📦 Hex string (%zu chars): %.100s%s\n", 
               strlen(hex_string), hex_string, strlen(hex_string) > 100 ? "..." : "");
        
        // Create a copy of pulse_data for reconstruction attempt
        pulse_data_t reconstruction_test = pulse_data_ex->pulse_data;
        
        // Clear existing pulses to test reconstruction
        reconstruction_test.num_pulses = 0;
        memset(reconstruction_test.pulse, 0, sizeof(reconstruction_test.pulse));
        
        printf("🔧 Original pulses cleared, attempting reconstruction from hex_string...\n");
        
        // Try rfraw_parse_pulses_only function
        if (rfraw_parse_pulses_only(&reconstruction_test, hex_string)) {
            printf("✅ Pulse reconstruction successful!\n");
            printf("📊 Reconstructed %u pulses\n", reconstruction_test.num_pulses);
            
            // Compare original vs reconstructed pulses
            printf("\n🔍 Comparison (Original vs Reconstructed):\n");
            printf("Original pulses (%u): ", pulse_data_ex->pulse_data.num_pulses);
            for (unsigned i = 0; i < pulse_data_ex->pulse_data.num_pulses && i < 10; i++) {
                printf("%d ", pulse_data_ex->pulse_data.pulse[i]);
            }
            if (pulse_data_ex->pulse_data.num_pulses > 10) printf("...");
            printf("\n");
            
            printf("Reconstructed pulses (%u): ", reconstruction_test.num_pulses);
            for (unsigned i = 0; i < reconstruction_test.num_pulses && i < 10; i++) {
                printf("%d ", reconstruction_test.pulse[i]);
            }
            if (reconstruction_test.num_pulses > 10) printf("...");
            printf("\n");
            
            // Check if pulses match
            bool pulses_match = true;
            if (pulse_data_ex->pulse_data.num_pulses == reconstruction_test.num_pulses) {
                for (unsigned i = 0; i < pulse_data_ex->pulse_data.num_pulses; i++) {
                    if (pulse_data_ex->pulse_data.pulse[i] != reconstruction_test.pulse[i]) {
                        pulses_match = false;
                        break;
                    }
                }
            } else {
                pulses_match = false;
            }
            
            if (pulses_match) {
                printf("✅ Pulses match perfectly!\n");
            } else {
                printf("⚠️ Pulses don't match - reconstruction differs from original\n");
            }
            
        } else {
            printf("❌ Pulse reconstruction failed\n");
        }
        
    } else {
        printf("⚠️ No hex_string found for pulse reconstruction\n");
    }
    
    printf("\n🎯 Test completed!\n");
    
    // Cleanup
    pulse_data_ex_free(pulse_data_ex);
    free(pulse_data_ex);
    ASN_STRUCT_FREE(asn_DEF_RTL433Message, rtl433_msg);
    free(decoded_pulse_data);
    free(binary_data);
    
    return 0;
}
