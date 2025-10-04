/*
 * Test utility for native ASN.1 signal encoding/decoding
 * Uses rtl433_asn1_pulse library for extended pulse data support
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "rtl433_asn1_pulse.h"
#include "RTL433Message.h"
#include "test_asn1_decoder.h"

// Function to show usage
void show_usage(const char *program_name) {
    printf("🔧 ASN.1 Decoder Test Utility\n");
    printf("==============================\n\n");
    
    printf("Usage:\n");
    printf("  %s [options]\n\n", program_name);
    
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -j, --json <JSON>       Encode JSON data to ASN.1 (RTL433Message)\n");
    printf("  -f, --file <FILE>       Read JSON from file\n");
    printf("  -d, --decode <HEX>      Decode hex ASN.1 data (RTL433Message)\n");
    printf("  -t, --type <TYPE>       PDU type for decoding (default: RTL433Message)\n");
    printf("  -r, --rtl433            Force RTL433Message wrapper (now default)\n");
    printf("  -x, --hex               Show hex output for encoding\n");
    printf("  -v, --verbose           Verbose output\n\n");
    
    printf("Examples:\n");
    printf("  # Run default tests\n");
    printf("  %s\n\n", program_name);
    
    printf("  # Encode JSON to ASN.1\n");
    printf("  %s --json '{\"offset\":12345,\"sample_rate\":250000,\"pulses\":[220,144,52,92]}'\n\n", program_name);
    
    printf("  # Read JSON from file\n");
    printf("  %s --file signal_data.json\n\n", program_name);
    
    printf("  # Decode hex ASN.1 data\n");
    printf("  %s --decode 60000A8290000003D08F00001B80120006800B84E2186A00258032003E804B005780640070807D00898096020CEE8BFF8CEEB18FF01B03A403E8F5C41A03B400ADE3541703BC3B1CAC705D69466C9D78\n\n", program_name);
    
    printf("  # Use RTL433Message wrapper\n");
    printf("  %s --rtl433 --json '{\"offset\":12345,\"sample_rate\":250000,\"pulses\":[220,144]}'\n\n", program_name);
    
    printf("JSON Format (rtl_433 compatible):\n");
    printf("{\n");
    printf("  \"time\": \"@0.262144s\",\n");
    printf("  \"mod\": \"FSK\",\n");
    printf("  \"count\": 58,\n");
    printf("  \"offset\": 47161,\n");
    printf("  \"rate_Hz\": 250000,\n");
    printf("  \"freq_Hz\": 433920000,\n");
    printf("  \"freq1_Hz\": 433942688,\n");
    printf("  \"freq2_Hz\": 433878368,\n");
    printf("  \"rssi_dB\": -2.714,\n");
    printf("  \"snr_dB\": 6.559,\n");
    printf("  \"noise_dB\": -9.273,\n");
    printf("  \"ook_low_estimate\": 1937,\n");
    printf("  \"ook_high_estimate\": 8770,\n");
    printf("  \"fsk_f1_est\": 5951,\n");
    printf("  \"fsk_f2_est\": -10916,\n");
    printf("  \"hex_string\": \"AAB1040030006000C8008C80...\",\n");
    printf("  \"pulses\": [40, 60, 40, 64, 40, 52, 52, 96, 200, 104, ...]\n");
    printf("}\n\n");
}

// Test-specific helper function for file reading
char* read_file_to_string(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "❌ read_file_to_string: Cannot open file '%s'\n", filename);
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fprintf(stderr, "❌ read_file_to_string: Invalid file size\n");
        fclose(file);
        return NULL;
    }
    
    // Allocate buffer
    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fprintf(stderr, "❌ read_file_to_string: Memory allocation failed\n");
        fclose(file);
        return NULL;
    }
    
    // Read file
    size_t bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    
    fclose(file);
    
    printf("✅ File '%s' read successfully (%zu bytes)\n", filename, bytes_read);
    return buffer;
}

int main(int argc, char *argv[])
{
    // Default options
    bool show_help = false;
    bool use_rtl433_wrapper = false;
    bool show_hex = false;
    bool verbose = false;
    char *json_string = NULL;
    char *json_file = NULL;
    char *hex_decode = NULL;
    char *pdu_type = "SignalMessage";
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help = true;
        } else if (strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--json") == 0) {
            if (i + 1 < argc) {
                json_string = argv[++i];
            } else {
                fprintf(stderr, "❌ Error: --json requires a JSON string argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--file") == 0) {
            if (i + 1 < argc) {
                json_file = argv[++i];
            } else {
                fprintf(stderr, "❌ Error: --file requires a filename argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--decode") == 0) {
            if (i + 1 < argc) {
                hex_decode = argv[++i];
            } else {
                fprintf(stderr, "❌ Error: --decode requires a hex string argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--type") == 0) {
            if (i + 1 < argc) {
                pdu_type = argv[++i];
            } else {
                fprintf(stderr, "❌ Error: --type requires a PDU type argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rtl433") == 0) {
            use_rtl433_wrapper = true;
        } else if (strcmp(argv[i], "-x") == 0 || strcmp(argv[i], "--hex") == 0) {
            show_hex = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else {
            fprintf(stderr, "❌ Error: Unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Use --help for usage information\n");
            return 1;
        }
    }
    
    // Show help if requested
    if (show_help) {
        show_usage(argv[0]);
        return 0;
    }
    
    // Handle different modes
    if (hex_decode) {
        // Decode hex ASN.1 data
        printf("🔧 ASN.1 Decoder Test Utility - Hex Decode Mode\n");
        printf("=================================================\n\n");
        decode_hex_asn1(hex_decode, use_rtl433_wrapper);
        return 0;
    }
    
    if (json_string || json_file) {
        // Encode JSON data
        printf("🔧 ASN.1 Decoder Test Utility - JSON Encode Mode\n");
        printf("==================================================\n\n");
        
        char *json_data = NULL;
        
        if (json_file) {
            json_data = read_file_to_string(json_file);
            if (!json_data) {
                return 1;
            }
        } else {
            json_data = json_string;
        }
        
        // Parse JSON to pulse_data_ex_t
        pulse_data_ex_t *pulse_data_ex = parse_json_to_pulse_data_ex(json_data);
        if (!pulse_data_ex) {
            if (json_file && json_data) free(json_data);
            return 1;
        }
        
        if (verbose) {
            printf("\n");
        }
        
        // Encode the data (always uses RTL433Message now)
        rtl433_asn1_buffer_t encoded = encode(pulse_data_ex);
        
        if (encoded.result == RTL433_ASN1_OK) {
            printf("✅ Encoding successful: %zu bytes\n", encoded.buffer_size);
            
            if (show_hex || verbose) {
                printf("🔍 ASN.1 HEX (%zu bytes): ", encoded.buffer_size);
                const unsigned char *bytes = (const unsigned char *)encoded.buffer;
                for (size_t i = 0; i < encoded.buffer_size; i++) {
                    printf("%02X", bytes[i]);
                }
                printf("\n");
            }
            
            // Test decode to verify (always RTL433Message now)
            if (verbose) {
                printf("\n🔄 Verifying decode...\n");
                pulse_data_t *decoded = decode(&encoded);
                if (decoded) {
                    printf("✅ Decode verification successful\n");
                    free(decoded);
                } else {
                    printf("❌ Decode verification failed\n");
                }
            }
            
            if (encoded.buffer) free(encoded.buffer);
        } else {
            printf("❌ Encoding failed: %d\n", encoded.result);
        }
        
        pulse_data_ex_free(pulse_data_ex);
        free(pulse_data_ex);
        if (json_file && json_data) free(json_data);
        return 0;
    }
    
    printf("\n🎉 All testing complete!\n");
    return 0;
}
