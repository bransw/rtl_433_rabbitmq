/*
 * RTL433 ASN.1 Pulse Data Extended Support
 * Implementation of extended pulse data structures and ASN.1 encoding/decoding
 */

#include "rtl433_asn1_pulse.h"

// === pulse_data_ex_t Management Functions ===

void pulse_data_ex_init(pulse_data_ex_t *data_ex) {
    if (!data_ex) return;
    memset(&data_ex->pulse_data, 0, sizeof(pulse_data_t));
    data_ex->hex_string = NULL;
    data_ex->modulation_type = NULL;
    data_ex->timestamp_str = NULL;
}

void pulse_data_ex_free(pulse_data_ex_t *data_ex) {
    if (!data_ex) return;
    if (data_ex->hex_string) {
        free(data_ex->hex_string);
        data_ex->hex_string = NULL;
    }
    if (data_ex->modulation_type) {
        free(data_ex->modulation_type);
        data_ex->modulation_type = NULL;
    }
    if (data_ex->timestamp_str) {
        free(data_ex->timestamp_str);
        data_ex->timestamp_str = NULL;
    }
}

int pulse_data_ex_set_hex_string(pulse_data_ex_t *data_ex, const char *hex_string) {
    if (!data_ex || !hex_string) return -1;
    
    // Free existing string
    if (data_ex->hex_string) {
        free(data_ex->hex_string);
    }
    
    // Allocate and copy new string
    size_t len = strlen(hex_string);
    data_ex->hex_string = malloc(len + 1);
    if (!data_ex->hex_string) {
        return -1;
    }
    
    strcpy(data_ex->hex_string, hex_string);
    return 0;
}

const char* pulse_data_ex_get_hex_string(const pulse_data_ex_t *data_ex) {
    if (!data_ex) return NULL;
    return data_ex->hex_string;
}

int pulse_data_ex_set_modulation_type(pulse_data_ex_t *data_ex, const char *modulation_type) {
    if (!data_ex || !modulation_type) return -1;
    
    // Free existing string
    if (data_ex->modulation_type) {
        free(data_ex->modulation_type);
    }
    
    // Allocate and copy new string
    size_t len = strlen(modulation_type);
    data_ex->modulation_type = malloc(len + 1);
    if (!data_ex->modulation_type) {
        return -1;
    }
    
    strcpy(data_ex->modulation_type, modulation_type);
    return 0;
}

const char* pulse_data_ex_get_modulation_type(const pulse_data_ex_t *data_ex) {
    if (!data_ex) return NULL;
    return data_ex->modulation_type;
}

int pulse_data_ex_set_timestamp_str(pulse_data_ex_t *data_ex, const char *timestamp_str) {
    if (!data_ex || !timestamp_str) return -1;
    
    // Free existing string
    if (data_ex->timestamp_str) {
        free(data_ex->timestamp_str);
    }
    
    // Allocate and copy new string
    size_t len = strlen(timestamp_str);
    data_ex->timestamp_str = malloc(len + 1);
    if (!data_ex->timestamp_str) {
        return -1;
    }
    
    strcpy(data_ex->timestamp_str, timestamp_str);
    return 0;
}

const char* pulse_data_ex_get_timestamp_str(const pulse_data_ex_t *data_ex) {
    if (!data_ex) return NULL;
    return data_ex->timestamp_str;
}

// === Hex String Utility Functions ===

char** split_hex_string(const char *hex_string, int *count) {
    if (!hex_string || !count) {
        if (count) *count = 0;
        return NULL;
    }
    
    // Count number of '+' delimiters to determine array size
    int delimiter_count = 0;
    const char *ptr = hex_string;
    while ((ptr = strchr(ptr, '+')) != NULL) {
        delimiter_count++;
        ptr++;
    }
    
    *count = delimiter_count + 1; // Number of strings = delimiters + 1
    
    // Allocate array of string pointers
    char **result = malloc(*count * sizeof(char*));
    if (!result) {
        *count = 0;
        return NULL;
    }
    
    // Split the string
    char *hex_copy = strdup(hex_string);
    if (!hex_copy) {
        free(result);
        *count = 0;
        return NULL;
    }
    
    char *token = strtok(hex_copy, "+");
    int i = 0;
    while (token && i < *count) {
        result[i] = strdup(token);
        if (!result[i]) {
            // Cleanup on error
            for (int j = 0; j < i; j++) {
                free(result[j]);
            }
            free(result);
            free(hex_copy);
            *count = 0;
            return NULL;
        }
        token = strtok(NULL, "+");
        i++;
    }
    
    free(hex_copy);
    return result;
}

void free_hex_strings_array(char **hex_strings, int count) {
    if (!hex_strings) return;
    
    for (int i = 0; i < count; i++) {
        if (hex_strings[i]) {
            free(hex_strings[i]);
        }
    }
    free(hex_strings);
}

unsigned char* hex_to_binary(const char *hex_string, size_t *binary_size) {
    if (!hex_string || !binary_size) {
        return NULL;
    }
    
    size_t hex_len = strlen(hex_string);
    if (hex_len % 2 != 0) {
        fprintf(stderr, "❌ hex_to_binary: Hex string length must be even\n");
        return NULL;
    }
    
    *binary_size = hex_len / 2;
    unsigned char *binary = malloc(*binary_size);
    if (!binary) {
        fprintf(stderr, "❌ hex_to_binary: Memory allocation failed\n");
        return NULL;
    }
    
    for (size_t i = 0; i < *binary_size; i++) {
        char hex_byte[3] = {hex_string[i*2], hex_string[i*2+1], '\0'};
        binary[i] = (unsigned char)strtol(hex_byte, NULL, 16);
    }
    
    return binary;
}

// === ASN.1 Data Preparation Functions ===

// Helper function to convert timestamp string to epoch timestamp
unsigned long *create_epoch_timestamp_from_string(const char *timestamp_str) {
    if (!timestamp_str || strlen(timestamp_str) == 0) {
        return NULL;
    }
    
    unsigned long *epoch_timestamp = calloc(1, sizeof(unsigned long));
    if (!epoch_timestamp) {
        return NULL;
    }
    
    // Parse timestamp string like "@0.262144s"
    if (timestamp_str[0] == '@') {
        double time_seconds = strtod(timestamp_str + 1, NULL);
        
        // For rtl_433, timestamps are relative to capture start
        // We'll use the fractional seconds as milliseconds offset from a base epoch
        // Base epoch: January 1, 2024 00:00:00 UTC = 1704067200
        const unsigned long base_epoch = 1704067200; // 2024-01-01 00:00:00 UTC
        
        // Add the time offset (convert to seconds and add to base)
        *epoch_timestamp = base_epoch + (unsigned long)time_seconds;
    } else {
        // If it's already a number, try to parse it directly
        *epoch_timestamp = (unsigned long)strtoul(timestamp_str, NULL, 10);
    }
    
    return epoch_timestamp;
}

int rtl433_message_set_package_id(RTL433Message_t *rtl433_msg, unsigned long package_id) {
    if (!rtl433_msg) {
        return -1;
    }
    
    if (rtl433_msg->present != RTL433Message_PR_signalMessage) {
        return -1;
    }
    
    SignalMessage_t *signal_msg = &rtl433_msg->choice.signalMessage;
    
    // Free existing packageId if present
    if (signal_msg->packageId) {
        free(signal_msg->packageId);
    }
    
    // Allocate and set new packageId
    signal_msg->packageId = calloc(1, sizeof(unsigned long));
    if (!signal_msg->packageId) {
        return -1;
    }
    
    *(signal_msg->packageId) = package_id;
    printf("📦 Set packageId: %lu (independent from offset)\n", package_id);
    
    return 0;
}

RTL433Message_t *prepare_pulse_data(pulse_data_ex_t *pulse_ex) {
    if (!pulse_ex) {
        fprintf(stderr, "❌ prepare_pulse_data: NULL pulse data ex\n");
        return NULL;
    }
    
    pulse_data_t *pulse = &pulse_ex->pulse_data;
    
    printf("🔄 Preparing RTL433Message from pulse_data_ex (offset: %lu, pulses: %u)\n", 
           pulse->offset, pulse->num_pulses);
    
    // Create RTL433Message wrapper
    RTL433Message_t *rtl433_msg = calloc(1, sizeof(RTL433Message_t));
    if (!rtl433_msg) {
        fprintf(stderr, "❌ prepare_pulse_data: RTL433Message allocation failed\n");
        return NULL;
    }
    
    // Create SignalMessage structure
    SignalMessage_t *signal_msg = calloc(1, sizeof(SignalMessage_t));
    if (!signal_msg) {
        fprintf(stderr, "❌ prepare_pulse_data: SignalMessage allocation failed\n");
        free(rtl433_msg);
        return NULL;
    }
    
    // packageId is independent - can be set separately if needed
    // For now, we don't set packageId automatically - it should be set by caller if needed
    signal_msg->packageId = NULL; // No automatic package ID
    printf("📦 No automatic packageId set (independent from offset)\n");
    
    // Set timestamp if available
    if (pulse_ex->timestamp_str && strlen(pulse_ex->timestamp_str) > 0) {
        signal_msg->timestamp = create_epoch_timestamp_from_string(pulse_ex->timestamp_str);
        if (signal_msg->timestamp) {
            printf("📅 Setting epoch timestamp: %s -> %lu\n", pulse_ex->timestamp_str, *(signal_msg->timestamp));
        }
    }
    
    // Create PulsesData for signal data
    PulsesData_t *pulses_data = calloc(1, sizeof(PulsesData_t));
    if (!pulses_data) {
        fprintf(stderr, "❌ prepare_pulse_data: PulsesData allocation failed\n");
        free(signal_msg);
        free(rtl433_msg);
        return NULL;
    }
    
    // Set pulses data
    pulses_data->count = pulse->num_pulses;
    pulses_data->sampleRate = pulse->sample_rate;
    
    // Fill pulses array ONLY if no hex_string is present
    if (!pulse_ex->hex_string || strlen(pulse_ex->hex_string) == 0) {
        if (pulse->num_pulses > 0) {
            for (unsigned int i = 0; i < pulse->num_pulses; i++) {
                long *pulse_val = calloc(1, sizeof(long));
                if (pulse_val) {
                    *pulse_val = pulse->pulse[i];
                    ASN_SEQUENCE_ADD(&pulses_data->pulses.list, pulse_val);
                }
            }
        }
    }
    
    // Handle signal data based on hex_string presence
    if (pulse_ex->hex_string && strlen(pulse_ex->hex_string) > 0) {
        // Check if hex_string contains multiple signals (separated by '+')
        if (strchr(pulse_ex->hex_string, '+')) {
            printf("📦 Using hexStrings for multiple signals\n");
            
            // Split hex_string by '+'
            int hex_count = 0;
            char **hex_array = split_hex_string(pulse_ex->hex_string, &hex_count);
            
            if (hex_array && hex_count > 0) {
                signal_msg->signalData.present = SignalData_PR_hexStrings;
                
                // Convert each hex string to OCTET_STRING_t and add to sequence
                for (int i = 0; i < hex_count; i++) {
                    if (hex_array[i] && strlen(hex_array[i]) > 0) {
                        // Convert hex string to binary
                        size_t binary_size = 0;
                        unsigned char *binary_data = hex_to_binary(hex_array[i], &binary_size);
                        
                        if (binary_data && binary_size > 0) {
                            OCTET_STRING_t *octet_str = calloc(1, sizeof(OCTET_STRING_t));
                            if (octet_str) {
                                OCTET_STRING_fromBuf(octet_str, (const char*)binary_data, binary_size);
                                ASN_SEQUENCE_ADD(&signal_msg->signalData.choice.hexStrings.list, octet_str);
                            }
                            free(binary_data);
                        }
                    }
                }
                
                free_hex_strings_array(hex_array, hex_count);
                printf("📦 Added %d hex strings to SignalData\n", hex_count);
            } else {
                // Fallback to pulses array if hex parsing failed
                signal_msg->signalData.present = SignalData_PR_pulsesArray;
                signal_msg->signalData.choice.pulsesArray = *pulses_data;
            }
        } else {
            printf("📦 Using hexString for single signal\n");
            
            // Single hex string
            size_t binary_size = 0;
            unsigned char *binary_data = hex_to_binary(pulse_ex->hex_string, &binary_size);
            
            if (binary_data && binary_size > 0) {
                signal_msg->signalData.present = SignalData_PR_hexString;
                OCTET_STRING_fromBuf(&signal_msg->signalData.choice.hexString, 
                                   (const char*)binary_data, binary_size);
                free(binary_data);
                printf("📦 Added single hex string to SignalData (%zu bytes)\n", binary_size);
            } else {
                // Fallback to pulses array
                signal_msg->signalData.present = SignalData_PR_pulsesArray;
                signal_msg->signalData.choice.pulsesArray = *pulses_data;
            }
        }
    } else {
        // No hex string data, use pulses array
        signal_msg->signalData.present = SignalData_PR_pulsesArray;
        signal_msg->signalData.choice.pulsesArray = *pulses_data;
    }
    
    // Set modulation type based on pulse_data_ex->modulation_type
    if (pulse_ex->modulation_type) {
        if (strcasecmp(pulse_ex->modulation_type, "FSK") == 0) {
            signal_msg->modulation = ModulationType_fsk;
            printf("📡 Setting modulation to FSK\n");
        } else if (strcasecmp(pulse_ex->modulation_type, "ASK") == 0) {
            signal_msg->modulation = ModulationType_ask;
            printf("📡 Setting modulation to ASK\n");
        } else {
            signal_msg->modulation = ModulationType_ook;
            printf("📡 Setting modulation to OOK (default for '%s')\n", pulse_ex->modulation_type);
        }
    } else {
        signal_msg->modulation = ModulationType_ook;
        printf("📡 Setting modulation to OOK (no modulation_type specified)\n");
    }
    
    // Set RF parameters
    signal_msg->frequency.centerFreq = pulse->centerfreq_hz;
    if (pulse->freq1_hz > 0) {
        signal_msg->frequency.freq1 = calloc(1, sizeof(unsigned long));
        if (signal_msg->frequency.freq1) {
            *(signal_msg->frequency.freq1) = pulse->freq1_hz;
        }
    }
    if (pulse->freq2_hz > 0) {
        signal_msg->frequency.freq2 = calloc(1, sizeof(unsigned long));
        if (signal_msg->frequency.freq2) {
            *(signal_msg->frequency.freq2) = pulse->freq2_hz;
        }
    }
    
    // Set signal quality (these are pointers in ASN.1, need to allocate)
    signal_msg->signalQuality = calloc(1, sizeof(SignalQuality_t));
    if (signal_msg->signalQuality) {
        signal_msg->signalQuality->rssiDb = calloc(1, sizeof(double));
        signal_msg->signalQuality->snrDb = calloc(1, sizeof(double));
        signal_msg->signalQuality->noiseDb = calloc(1, sizeof(double));
        if (signal_msg->signalQuality->rssiDb) *(signal_msg->signalQuality->rssiDb) = pulse->rssi_db;
        if (signal_msg->signalQuality->snrDb) *(signal_msg->signalQuality->snrDb) = pulse->snr_db;
        if (signal_msg->signalQuality->noiseDb) *(signal_msg->signalQuality->noiseDb) = pulse->noise_db;
    }
    
    // Set timing info (this is a pointer in ASN.1, need to allocate)
    signal_msg->timingInfo = calloc(1, sizeof(TimingInfo_t));
    if (signal_msg->timingInfo) {
        // Set offset field in TimingInfo (always set if present, independent of packageId)
        signal_msg->timingInfo->offset = calloc(1, sizeof(unsigned long));
        if (signal_msg->timingInfo->offset) {
            *(signal_msg->timingInfo->offset) = (unsigned long)pulse->offset;
            printf("📍 Setting offset in TimingInfo: %lu (independent from packageId)\n", *(signal_msg->timingInfo->offset));
        }
        
        signal_msg->timingInfo->ookLowEstimate = calloc(1, sizeof(long));
        signal_msg->timingInfo->ookHighEstimate = calloc(1, sizeof(long));
        signal_msg->timingInfo->fskF1Est = calloc(1, sizeof(long));
        signal_msg->timingInfo->fskF2Est = calloc(1, sizeof(long));
        if (signal_msg->timingInfo->ookLowEstimate) *(signal_msg->timingInfo->ookLowEstimate) = pulse->ook_low_estimate;
        if (signal_msg->timingInfo->ookHighEstimate) *(signal_msg->timingInfo->ookHighEstimate) = pulse->ook_high_estimate;
        if (signal_msg->timingInfo->fskF1Est) *(signal_msg->timingInfo->fskF1Est) = pulse->fsk_f1_est;
        if (signal_msg->timingInfo->fskF2Est) *(signal_msg->timingInfo->fskF2Est) = pulse->fsk_f2_est;
    }
    
    // Set RTL433Message to contain SignalMessage
    rtl433_msg->present = RTL433Message_PR_signalMessage;
    rtl433_msg->choice.signalMessage = *signal_msg;
    
    // Clean up temporary structures (but not the data they point to)
    free(pulses_data);
    free(signal_msg);
    
    return rtl433_msg;
}

pulse_data_ex_t *extract_pulse_data(RTL433Message_t *rtl433_msg) {
    if (!rtl433_msg) {
        fprintf(stderr, "❌ extract_pulse_data: NULL RTL433Message\n");
        return NULL;
    }
    
    if (rtl433_msg->present != RTL433Message_PR_signalMessage) {
        fprintf(stderr, "❌ extract_pulse_data: RTL433Message does not contain SignalMessage (present: %d)\n", rtl433_msg->present);
        return NULL;
    }
    
    SignalMessage_t *signal_msg = &rtl433_msg->choice.signalMessage;
    
    printf("🔄 Extracting pulse_data_ex from RTL433Message (packageId: %lu)\n", signal_msg->packageId);
    
    // Allocate pulse_data_ex_t
    pulse_data_ex_t *pulse_data_ex = calloc(1, sizeof(pulse_data_ex_t));
    if (!pulse_data_ex) {
        fprintf(stderr, "❌ extract_pulse_data: Memory allocation failed\n");
        return NULL;
    }
    
    pulse_data_t *pulse_data = &pulse_data_ex->pulse_data;
    
    // Extract basic fields
    pulse_data->centerfreq_hz = signal_msg->frequency.centerFreq;
    
    // packageId is independent - not related to offset
    if (signal_msg->packageId) {
        printf("📦 PackageId present: %lu (stored separately, not used for offset)\n", *(signal_msg->packageId));
        // packageId is just a message sequence number, not used for pulse_data
    } else {
        printf("📦 No packageId present\n");
    }
    
    // Initialize offset to 0
    pulse_data->offset = 0;
    
    // Extract timestamp if present
    if (signal_msg->timestamp) {
        // Convert epoch timestamp back to string format
        unsigned long epoch_value = *(signal_msg->timestamp);
        
        // Convert back to relative time format for rtl_433 compatibility
        const unsigned long base_epoch = 1704067200; // 2024-01-01 00:00:00 UTC
        if (epoch_value >= base_epoch) {
            unsigned long relative_seconds = epoch_value - base_epoch;
            char timestamp_buffer[32];
            snprintf(timestamp_buffer, sizeof(timestamp_buffer), "@%lu.000s", relative_seconds);
            pulse_data_ex_set_timestamp_str(pulse_data_ex, timestamp_buffer);
            printf("📅 Extracted epoch timestamp: %lu -> %s\n", epoch_value, timestamp_buffer);
        } else {
            // Direct epoch value
            char timestamp_buffer[32];
            snprintf(timestamp_buffer, sizeof(timestamp_buffer), "%lu", epoch_value);
            pulse_data_ex_set_timestamp_str(pulse_data_ex, timestamp_buffer);
            printf("📅 Extracted epoch timestamp: %lu\n", epoch_value);
        }
    }
    
    // Extract optional frequency fields
    if (signal_msg->frequency.freq1) {
        pulse_data->freq1_hz = *(signal_msg->frequency.freq1);
    }
    if (signal_msg->frequency.freq2) {
        pulse_data->freq2_hz = *(signal_msg->frequency.freq2);
    }
    
    // Extract signal quality (check if pointer is not NULL)
    if (signal_msg->signalQuality) {
        if (signal_msg->signalQuality->rssiDb) pulse_data->rssi_db = *(signal_msg->signalQuality->rssiDb);
        if (signal_msg->signalQuality->snrDb) pulse_data->snr_db = *(signal_msg->signalQuality->snrDb);
        if (signal_msg->signalQuality->noiseDb) pulse_data->noise_db = *(signal_msg->signalQuality->noiseDb);
    }
    
    // Extract timing info (check if pointer is not NULL)
    if (signal_msg->timingInfo) {
        // Extract offset from TimingInfo (proper location for signal position data)
        if (signal_msg->timingInfo->offset) {
            pulse_data->offset = *(signal_msg->timingInfo->offset);
            printf("📍 Extracted offset from TimingInfo: %lu\n", pulse_data->offset);
        }
        
        if (signal_msg->timingInfo->ookLowEstimate) pulse_data->ook_low_estimate = *(signal_msg->timingInfo->ookLowEstimate);
        if (signal_msg->timingInfo->ookHighEstimate) pulse_data->ook_high_estimate = *(signal_msg->timingInfo->ookHighEstimate);
        if (signal_msg->timingInfo->fskF1Est) pulse_data->fsk_f1_est = *(signal_msg->timingInfo->fskF1Est);
        if (signal_msg->timingInfo->fskF2Est) pulse_data->fsk_f2_est = *(signal_msg->timingInfo->fskF2Est);
    }
    
    // Extract modulation type
    switch (signal_msg->modulation) {
        case ModulationType_fsk:
            pulse_data_ex_set_modulation_type(pulse_data_ex, "FSK");
            break;
        case ModulationType_ask:
            pulse_data_ex_set_modulation_type(pulse_data_ex, "ASK");
            break;
        case ModulationType_ook:
        default:
            pulse_data_ex_set_modulation_type(pulse_data_ex, "OOK");
            break;
    }
    
    // Extract signal data based on present type
    if (signal_msg->signalData.present == SignalData_PR_hexStrings) {
        // Multiple hex strings - reconstruct with '+' separator
        printf("📦 Extracting %d hex strings from SignalData\n", 
               signal_msg->signalData.choice.hexStrings.list.count);
        
        // Calculate total size needed for hex string
        size_t total_hex_size = 0;
        for (int i = 0; i < signal_msg->signalData.choice.hexStrings.list.count; i++) {
            OCTET_STRING_t *octet_str = signal_msg->signalData.choice.hexStrings.list.array[i];
            if (octet_str && octet_str->buf) {
                total_hex_size += octet_str->size * 2; // 2 hex chars per byte
                if (i > 0) total_hex_size += 1; // '+' separator
            }
        }
        total_hex_size += 1; // null terminator
        
        // Allocate hex string buffer
        char *hex_string = malloc(total_hex_size);
        if (hex_string) {
            hex_string[0] = '\0';
            
            for (int i = 0; i < signal_msg->signalData.choice.hexStrings.list.count; i++) {
                OCTET_STRING_t *octet_str = signal_msg->signalData.choice.hexStrings.list.array[i];
                if (octet_str && octet_str->buf) {
                    if (i > 0) strcat(hex_string, "+");
                    
                    // Convert binary to hex string
                    size_t current_len = strlen(hex_string);
                    for (int j = 0; j < octet_str->size; j++) {
                        sprintf(hex_string + current_len + (j * 2), "%02X", octet_str->buf[j]);
                    }
                }
            }
            
            pulse_data_ex_set_hex_string(pulse_data_ex, hex_string);
            printf("📦 Reconstructed hex_string: %.100s%s\n", 
                   hex_string, strlen(hex_string) > 100 ? "..." : "");
            free(hex_string);
        }
        
    } else if (signal_msg->signalData.present == SignalData_PR_hexString) {
        // Single hex string
        OCTET_STRING_t *octet_str = &signal_msg->signalData.choice.hexString;
        if (octet_str && octet_str->buf) {
            printf("📦 Extracting single hex string from SignalData (%zu bytes)\n", octet_str->size);
            
            // Convert binary to hex string
            char *hex_string = malloc(octet_str->size * 2 + 1);
            if (hex_string) {
                for (int i = 0; i < octet_str->size; i++) {
                    sprintf(hex_string + (i * 2), "%02X", octet_str->buf[i]);
                }
                hex_string[octet_str->size * 2] = '\0';
                
                pulse_data_ex_set_hex_string(pulse_data_ex, hex_string);
                printf("📦 Extracted hex_string: %.100s%s\n", 
                       hex_string, strlen(hex_string) > 100 ? "..." : "");
                free(hex_string);
            }
        }
        
    } else if (signal_msg->signalData.present == SignalData_PR_pulsesArray) {
        // Pulses array
        PulsesData_t *pulses_data = &signal_msg->signalData.choice.pulsesArray;
        printf("📦 Extracting %d pulses from SignalData\n", pulses_data->pulses.list.count);
        
        pulse_data->num_pulses = pulses_data->pulses.list.count;
        pulse_data->sample_rate = pulses_data->sampleRate;
        
        if (pulse_data->num_pulses > PD_MAX_PULSES) {
            printf("⚠️  Warning: truncating %u pulses to %d\n", 
                   pulse_data->num_pulses, PD_MAX_PULSES);
            pulse_data->num_pulses = PD_MAX_PULSES;
        }
        
        for (int i = 0; i < pulse_data->num_pulses; i++) {
            long *pulse_val = pulses_data->pulses.list.array[i];
            if (pulse_val) {
                pulse_data->pulse[i] = *pulse_val;
            }
        }
    }
    
    return pulse_data_ex;
}

// === ASN.1 Encoding/Decoding Functions ===

rtl433_asn1_buffer_t encode(pulse_data_ex_t *pulse_ex) {
    rtl433_asn1_buffer_t result = {0};
    
    if (!pulse_ex) {
        fprintf(stderr, "❌ encode: NULL pulse data ex\n");
        result.result = RTL433_ASN1_ERROR_INVALID_DATA;
        return result;
    }
    
    RTL433Message_t *rtl433_msg = prepare_pulse_data(pulse_ex);
    if (!rtl433_msg) {
        result.result = RTL433_ASN1_ERROR_MEMORY;
        return result;
    }
    
    // Debug: Print ASN.1 structure before encoding
    printf("🔍 ASN.1 Structure before encoding:\n");
    asn_fprint(stdout, &asn_DEF_RTL433Message, rtl433_msg);
    printf("\n");
    
    // Encode using native ASN.1 UPER encoder (RTL433Message)
    asn_enc_rval_t encode_result = uper_encode(&asn_DEF_RTL433Message, NULL, rtl433_msg, 
                                               NULL, NULL);
    
    if (encode_result.encoded != -1) {
        // If encoding successful, encode to buffer
        ssize_t buffer_size = uper_encode_to_new_buffer(
            &asn_DEF_RTL433Message, NULL, rtl433_msg, (void**)&result.buffer);
        if (buffer_size > 0) {
            encode_result.encoded = buffer_size * 8; // Convert bytes to bits
        } else {
            encode_result.encoded = -1;
        }
    }
    
    if (encode_result.encoded == -1) {
        fprintf(stderr, "❌ encode: UPER encoding failed\n");
        result.result = RTL433_ASN1_ERROR_ENCODE;
        result.buffer_size = 0;
    } else {
        result.buffer_size = (encode_result.encoded + 7) / 8; // Convert bits to bytes
        result.result = RTL433_ASN1_OK;
        printf("✅ RTL433Message encode successful: %zu bytes\n", result.buffer_size);
        
        // Print hex debug
        if (result.buffer && result.buffer_size > 0) {
            printf("🔍 RTL433Message HEX (%zu bytes): ", result.buffer_size);
            const unsigned char *bytes = (const unsigned char *)result.buffer;
            for (size_t i = 0; i < result.buffer_size; i++) {
                printf("%02X", bytes[i]);
            }
            printf("\n");
        }
    }
    
    // Cleanup (ASN.1 structures)
    ASN_STRUCT_FREE(asn_DEF_RTL433Message, rtl433_msg);
    
    return result;
}

pulse_data_t* decode(const rtl433_asn1_buffer_t *buffer) {
    if (!buffer || !buffer->buffer || buffer->buffer_size == 0) {
        fprintf(stderr, "❌ decode: Invalid buffer\n");
        return NULL;
    }
    
    printf("🔄 Decoding RTL433Message buffer using native functions (%zu bytes)\n", buffer->buffer_size);
    
    // Decode using native ASN.1 UPER decoder (RTL433Message)
    RTL433Message_t *rtl433_msg = NULL;
    asn_dec_rval_t decode_result = uper_decode_complete(
        NULL, &asn_DEF_RTL433Message, (void**)&rtl433_msg, 
        buffer->buffer, buffer->buffer_size);
    
    if (decode_result.code != RC_OK) {
        fprintf(stderr, "❌ decode: RTL433Message UPER decoding failed (code: %d)\n", decode_result.code);
        if (rtl433_msg) ASN_STRUCT_FREE(asn_DEF_RTL433Message, rtl433_msg);
        return NULL;
    }
    
    if (!rtl433_msg) {
        fprintf(stderr, "❌ decode: NULL RTL433Message after decode\n");
        return NULL;
    }
    
    // Check if RTL433Message contains SignalMessage
    if (rtl433_msg->present != RTL433Message_PR_signalMessage) {
        fprintf(stderr, "❌ decode: RTL433Message does not contain SignalMessage (present: %d)\n", rtl433_msg->present);
        ASN_STRUCT_FREE(asn_DEF_RTL433Message, rtl433_msg);
        return NULL;
    }
    
    // Debug: Print full RTL433Message structure after decoding
    printf("🔍 Full RTL433Message structure after decoding:\n");
    asn_fprint(stdout, &asn_DEF_RTL433Message, rtl433_msg);
    printf("\n");
    
    // Use extract_pulse_data to get full pulse_data_ex_t with timestamp
    pulse_data_ex_t *pulse_data_ex = extract_pulse_data(rtl433_msg);
    if (!pulse_data_ex) {
        fprintf(stderr, "❌ decode: Failed to extract pulse data\n");
        ASN_STRUCT_FREE(asn_DEF_RTL433Message, rtl433_msg);
        return NULL;
    }
    
    // Allocate pulse_data_t for return (copy from pulse_data_ex)
    pulse_data_t *pulse_data = malloc(sizeof(pulse_data_t));
    if (!pulse_data) {
        fprintf(stderr, "❌ decode: Memory allocation failed\n");
        pulse_data_ex_free(pulse_data_ex);
        free(pulse_data_ex);
        ASN_STRUCT_FREE(asn_DEF_RTL433Message, rtl433_msg);
        return NULL;
    }
    
    // Copy pulse_data from pulse_data_ex
    *pulse_data = pulse_data_ex->pulse_data;
    
    printf("✅ RTL433Message decode successful: offset=%lu, pulses=%u, freq=%.0f\n", 
           pulse_data->offset, pulse_data->num_pulses, pulse_data->centerfreq_hz);
    
    // Cleanup
    pulse_data_ex_free(pulse_data_ex);
    free(pulse_data_ex);
    ASN_STRUCT_FREE(asn_DEF_RTL433Message, rtl433_msg);
    
    return pulse_data;
}

// === Print Functions ===

void print_pulse_data(const pulse_data_t *pulse_data)
{
    if (!pulse_data) {
        printf("❌ NULL pulse data\n");
        return;
    }
    
    printf("📊 Pulse Data Summary:\n");
    printf("  Offset: %lu\n", pulse_data->offset);
    printf("  Num pulses: %u\n", pulse_data->num_pulses);
    printf("  Sample rate: %u Hz\n", pulse_data->sample_rate);
    printf("  Depth bits: %u\n", pulse_data->depth_bits);
    printf("  Center freq: %.0f Hz\n", pulse_data->centerfreq_hz);
    printf("  Freq1: %.0f Hz\n", pulse_data->freq1_hz);
    printf("  Freq2: %.0f Hz\n", pulse_data->freq2_hz);
    printf("  RSSI: %.2f dB\n", pulse_data->rssi_db);
    printf("  SNR: %.2f dB\n", pulse_data->snr_db);
    printf("  Noise: %.2f dB\n", pulse_data->noise_db);
    printf("  OOK low: %d\n", pulse_data->ook_low_estimate);
    printf("  OOK high: %d\n", pulse_data->ook_high_estimate);
    printf("  FSK F1: %d\n", pulse_data->fsk_f1_est);
    printf("  FSK F2: %d\n", pulse_data->fsk_f2_est);
    
    if (pulse_data->num_pulses > 0) {
        printf("  First 10 pulses: ");
        for (int i = 0; i < pulse_data->num_pulses && i < 10; i++) {
            printf("%d ", pulse_data->pulse[i]);
        }
        if (pulse_data->num_pulses > 10) {
            printf("...");
        }
        printf("\n");
    }
}

void print_pulse_data_ex(const pulse_data_ex_t *pulse_data_ex)
{
    if (!pulse_data_ex) {
        printf("❌ NULL pulse data ex\n");
        return;
    }
    
    const pulse_data_t *pulse_data = &pulse_data_ex->pulse_data;
    
    printf("📊 Pulse Data Ex Summary:\n");
    printf("  Offset: %lu\n", pulse_data->offset);
    printf("  Num pulses: %u\n", pulse_data->num_pulses);
    printf("  Sample rate: %u Hz\n", pulse_data->sample_rate);
    printf("  Depth bits: %u\n", pulse_data->depth_bits);
    printf("  Center freq: %.0f Hz\n", pulse_data->centerfreq_hz);
    printf("  Freq1: %.0f Hz\n", pulse_data->freq1_hz);
    printf("  Freq2: %.0f Hz\n", pulse_data->freq2_hz);
    printf("  RSSI: %.2f dB\n", pulse_data->rssi_db);
    printf("  SNR: %.2f dB\n", pulse_data->snr_db);
    printf("  Noise: %.2f dB\n", pulse_data->noise_db);
    printf("  OOK low: %d\n", pulse_data->ook_low_estimate);
    printf("  OOK high: %d\n", pulse_data->ook_high_estimate);
    printf("  FSK F1: %d\n", pulse_data->fsk_f1_est);
    printf("  FSK F2: %d\n", pulse_data->fsk_f2_est);
    
    // Print modulation type if available
    if (pulse_data_ex->modulation_type && strlen(pulse_data_ex->modulation_type) > 0) {
        printf("  Modulation: %s\n", pulse_data_ex->modulation_type);
    } else {
        printf("  Modulation: (unknown)\n");
    }
    
    // Print timestamp if available
    if (pulse_data_ex->timestamp_str && strlen(pulse_data_ex->timestamp_str) > 0) {
        printf("  Timestamp: %s\n", pulse_data_ex->timestamp_str);
    } else {
        printf("  Timestamp: (none)\n");
    }
    
    // Print hex string if available
    if (pulse_data_ex->hex_string && strlen(pulse_data_ex->hex_string) > 0) {
        printf("  Hex String (%zu bytes): %s\n", strlen(pulse_data_ex->hex_string), pulse_data_ex->hex_string);
    } else {
        printf("  Hex String: (none)\n");
    }
    
    if (pulse_data->num_pulses > 0) {
        printf("  First 10 pulses: ");
        for (int i = 0; i < pulse_data->num_pulses && i < 10; i++) {
            printf("%d ", pulse_data->pulse[i]);
        }
        if (pulse_data->num_pulses > 10) {
            printf("...");
        }
        printf("\n");
    }
}

// === JSON Parsing Functions ===

pulse_data_ex_t* parse_json_to_pulse_data_ex(const char *json_string) {
    if (!json_string) {
        fprintf(stderr, "❌ parse_json_to_pulse_data: NULL JSON string\n");
        return NULL;
    }
    
    printf("🔄 Parsing JSON to pulse_data_t\n");
    
    // Parse JSON
    json_object *json_obj = json_tokener_parse(json_string);
    if (!json_obj) {
        fprintf(stderr, "❌ parse_json_to_pulse_data: Invalid JSON format\n");
        return NULL;
    }
    
    // Allocate pulse_data_ex_t
    pulse_data_ex_t *pulse_data_ex = calloc(1, sizeof(pulse_data_ex_t));
    if (!pulse_data_ex) {
        fprintf(stderr, "❌ parse_json_to_pulse_data_ex: Memory allocation failed\n");
        json_object_put(json_obj);
        return NULL;
    }
    
    pulse_data_t *pulse_data = &pulse_data_ex->pulse_data;
    
    // Parse JSON fields
    json_object *field;
    
    // offset (package_id equivalent)
    if (json_object_object_get_ex(json_obj, "offset", &field)) {
        pulse_data->offset = json_object_get_int64(field);
    } else if (json_object_object_get_ex(json_obj, "package_id", &field)) {
        pulse_data->offset = json_object_get_int64(field);
    }
    
    // sample_rate (rate_Hz is primary field name in rtl_433)
    if (json_object_object_get_ex(json_obj, "rate_Hz", &field)) {
        pulse_data->sample_rate = json_object_get_int(field);
    } else if (json_object_object_get_ex(json_obj, "sample_rate", &field)) {
        pulse_data->sample_rate = json_object_get_int(field);
    }
    
    // count field (rtl_433 uses this for pulse count)
    if (json_object_object_get_ex(json_obj, "count", &field)) {
        pulse_data->num_pulses = json_object_get_int(field);
    }
    
    // time field (rtl_433 format: "@0.262144s")
    if (json_object_object_get_ex(json_obj, "time", &field)) {
        const char *time_str = json_object_get_string(field);
        if (time_str && strlen(time_str) > 1) {
            // Store the original timestamp string
            pulse_data_ex_set_timestamp_str(pulse_data_ex, time_str);
            
            // Parse time string like "@0.262144s" - extract the number
            if (time_str[0] == '@') {
                double time_seconds = strtod(time_str + 1, NULL);
                // Convert to microseconds and store in start_ago (approximate)
                pulse_data->start_ago = (uint64_t)(time_seconds * 1000000);
            }
        }
    }
    
    // mod field (modulation type: "FSK", "OOK", "ASK", etc.)
    if (json_object_object_get_ex(json_obj, "mod", &field)) {
        const char *mod_str = json_object_get_string(field);
        if (mod_str) {
            pulse_data_ex_set_modulation_type(pulse_data_ex, mod_str);
            printf("📡 Modulation type: %s\n", mod_str);
        }
    }
    
    // depth_bits
    if (json_object_object_get_ex(json_obj, "depth_bits", &field)) {
        pulse_data->depth_bits = json_object_get_int(field);
    }
    
    // start_ago
    if (json_object_object_get_ex(json_obj, "start_ago", &field)) {
        pulse_data->start_ago = json_object_get_int64(field);
    }
    
    // end_ago
    if (json_object_object_get_ex(json_obj, "end_ago", &field)) {
        pulse_data->end_ago = json_object_get_int64(field);
    }
    
    // frequencies (rtl_433 format)
    if (json_object_object_get_ex(json_obj, "freq_Hz", &field)) {
        pulse_data->centerfreq_hz = json_object_get_double(field);
    } else if (json_object_object_get_ex(json_obj, "centerfreq_hz", &field)) {
        pulse_data->centerfreq_hz = json_object_get_double(field);
    }
    
    if (json_object_object_get_ex(json_obj, "freq1_Hz", &field)) {
        pulse_data->freq1_hz = json_object_get_double(field);
    } else if (json_object_object_get_ex(json_obj, "freq1_hz", &field)) {
        pulse_data->freq1_hz = json_object_get_double(field);
    }
    
    if (json_object_object_get_ex(json_obj, "freq2_Hz", &field)) {
        pulse_data->freq2_hz = json_object_get_double(field);
    } else if (json_object_object_get_ex(json_obj, "freq2_hz", &field)) {
        pulse_data->freq2_hz = json_object_get_double(field);
    }
    
    // signal quality
    if (json_object_object_get_ex(json_obj, "rssi_db", &field)) {
        pulse_data->rssi_db = json_object_get_double(field);
    } else if (json_object_object_get_ex(json_obj, "rssi_dB", &field)) {
        pulse_data->rssi_db = json_object_get_double(field);
    }
    
    if (json_object_object_get_ex(json_obj, "snr_db", &field)) {
        pulse_data->snr_db = json_object_get_double(field);
    } else if (json_object_object_get_ex(json_obj, "snr_dB", &field)) {
        pulse_data->snr_db = json_object_get_double(field);
    }
    
    if (json_object_object_get_ex(json_obj, "noise_db", &field)) {
        pulse_data->noise_db = json_object_get_double(field);
    } else if (json_object_object_get_ex(json_obj, "noise_dB", &field)) {
        pulse_data->noise_db = json_object_get_double(field);
    }
    
    if (json_object_object_get_ex(json_obj, "range_db", &field)) {
        pulse_data->range_db = json_object_get_double(field);
    } else if (json_object_object_get_ex(json_obj, "range_dB", &field)) {
        pulse_data->range_db = json_object_get_double(field);
    }
    
    // FSK/OOK estimates
    if (json_object_object_get_ex(json_obj, "ook_low_estimate", &field)) {
        pulse_data->ook_low_estimate = json_object_get_int(field);
    }
    
    if (json_object_object_get_ex(json_obj, "ook_high_estimate", &field)) {
        pulse_data->ook_high_estimate = json_object_get_int(field);
    }
    
    if (json_object_object_get_ex(json_obj, "fsk_f1_est", &field)) {
        pulse_data->fsk_f1_est = json_object_get_int(field);
    }
    
    if (json_object_object_get_ex(json_obj, "fsk_f2_est", &field)) {
        pulse_data->fsk_f2_est = json_object_get_int(field);
    }
    
    // Parse pulses array
    if (json_object_object_get_ex(json_obj, "pulses", &field)) {
        if (json_object_is_type(field, json_type_array)) {
            int array_len = json_object_array_length(field);
            
            // Use count field if available, otherwise use array length
            if (pulse_data->num_pulses == 0) {
                pulse_data->num_pulses = (array_len > 1200) ? 1200 : array_len;
            } else {
                // Verify count matches array length
                if (pulse_data->num_pulses != array_len) {
                    printf("⚠️  Warning: count field (%u) doesn't match pulses array length (%d)\n", 
                           pulse_data->num_pulses, array_len);
                    pulse_data->num_pulses = (array_len > 1200) ? 1200 : array_len;
                }
            }
            
            for (int i = 0; i < pulse_data->num_pulses && i < array_len; i++) {
                json_object *pulse_val = json_object_array_get_idx(field, i);
                if (pulse_val) {
                    pulse_data->pulse[i] = json_object_get_int(pulse_val);
                }
            }
        }
    }
    
    // Parse gaps array (optional)
    if (json_object_object_get_ex(json_obj, "gaps", &field)) {
        if (json_object_is_type(field, json_type_array)) {
            int array_len = json_object_array_length(field);
            int gap_count = (array_len > 1200) ? 1200 : array_len;
            
            for (int i = 0; i < gap_count; i++) {
                json_object *gap_val = json_object_array_get_idx(field, i);
                if (gap_val) {
                    pulse_data->gap[i] = json_object_get_int(gap_val);
                }
            }
        }
    }
    
    // Set default values if not provided
    if (pulse_data->sample_rate == 0) {
        pulse_data->sample_rate = 250000; // Default sample rate
    }
    
    if (pulse_data->num_pulses == 0) {
        // Create default pulse data if no pulses provided
        pulse_data->num_pulses = 16;
        int default_pulses[] = {220, 144, 52, 92, 10000, 50000, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200};
        memcpy(pulse_data->pulse, default_pulses, sizeof(default_pulses));
    }
    
    // Ensure minimum pulse count for ASN.1 constraints
    if (pulse_data->num_pulses < 16) {
        printf("⚠️  Warning: Pulse count (%u) is less than ASN.1 minimum (16), padding with zeros\n", pulse_data->num_pulses);
        for (int i = pulse_data->num_pulses; i < 16; i++) {
            pulse_data->pulse[i] = 100; // Default pulse value
        }
        pulse_data->num_pulses = 16;
    }
    
    // Parse hex_string if available
    if (json_object_object_get_ex(json_obj, "hex_string", &field)) {
        const char *hex_str = json_object_get_string(field);
        if (hex_str && strlen(hex_str) > 0) {
            pulse_data_ex_set_hex_string(pulse_data_ex, hex_str);
        }
    }
    
    json_object_put(json_obj);
    
    printf("✅ JSON parsed successfully: offset=%lu, pulses=%u, freq=%.0f\n", 
           pulse_data->offset, pulse_data->num_pulses, pulse_data->centerfreq_hz);
    
    return pulse_data_ex;
}

// === Utility Functions ===

void decode_hex_asn1(const char *hex_string, bool use_rtl433_wrapper) {
    if (!hex_string) {
        fprintf(stderr, "❌ decode_hex_asn1: NULL hex string\n");
        return;
    }
    
    printf("🔄 Decoding hex ASN.1 data (%zu chars)\n", strlen(hex_string));
    
    // Convert hex to binary
    size_t binary_size;
    unsigned char *binary_data = hex_to_binary(hex_string, &binary_size);
    if (!binary_data) {
        return;
    }
    
    printf("📦 Binary size: %zu bytes\n", binary_size);
    
    // Create buffer structure
    rtl433_asn1_buffer_t buffer = {
        .buffer = binary_data,
        .buffer_size = binary_size,
        .result = RTL433_ASN1_OK
    };
    
    if (use_rtl433_wrapper) {
        // Decode as RTL433Message
        RTL433Message_t *rtl433_msg = NULL;
        asn_dec_rval_t decode_result = uper_decode_complete(
            NULL, &asn_DEF_RTL433Message, (void**)&rtl433_msg, 
            buffer.buffer, buffer.buffer_size);
        
        if (decode_result.code != RC_OK) {
            fprintf(stderr, "❌ RTL433Message decoding failed (code: %d)\n", decode_result.code);
            free(binary_data);
            return;
        }
        
        if (!rtl433_msg) {
            fprintf(stderr, "❌ NULL RTL433Message after decode\n");
            free(binary_data);
            return;
        }
        
        printf("✅ RTL433Message decoded successfully\n");
        printf("📋 Message type: %d\n", rtl433_msg->present);
        
        // Debug: Print full RTL433Message structure
        printf("🔍 Full RTL433Message structure:\n");
        asn_fprint(stdout, &asn_DEF_RTL433Message, rtl433_msg);
        printf("\n");
        
        if (rtl433_msg->present == RTL433Message_PR_signalMessage) {
            SignalMessage_t *signal_msg = &rtl433_msg->choice.signalMessage;
            printf("📊 SignalMessage details:\n");
            printf("  Package ID: %lu\n", signal_msg->packageId);
            printf("  Center freq: %.0f Hz\n", signal_msg->frequency.centerFreq);
            printf("  Modulation: %ld\n", signal_msg->modulation);
            
            if (signal_msg->signalData.present == SignalData_PR_pulsesArray) {
                PulsesData_t *pulses_data = &signal_msg->signalData.choice.pulsesArray;
                printf("  Pulse count: %ld\n", pulses_data->count);
                printf("  Sample rate: %lu Hz\n", pulses_data->sampleRate);
                
                printf("  First 10 pulses: ");
                int pulse_count = pulses_data->pulses.list.count;
                for (int i = 0; i < pulse_count && i < 10; i++) {
                    if (pulses_data->pulses.list.array[i]) {
                        printf("%ld ", *(pulses_data->pulses.list.array[i]));
                    }
                }
                if (pulse_count > 10) printf("...");
                printf("\n");
            }
            
            if (signal_msg->signalQuality) {
                printf("  Signal Quality:\n");
                if (signal_msg->signalQuality->rssiDb) {
                    printf("    RSSI: %.2f dB\n", *(signal_msg->signalQuality->rssiDb));
                }
                if (signal_msg->signalQuality->snrDb) {
                    printf("    SNR: %.2f dB\n", *(signal_msg->signalQuality->snrDb));
                }
                if (signal_msg->signalQuality->noiseDb) {
                    printf("    Noise: %.2f dB\n", *(signal_msg->signalQuality->noiseDb));
                }
            }
            
            if (signal_msg->timingInfo) {
                printf("  Timing Info:\n");
                if (signal_msg->timingInfo->ookLowEstimate) {
                    printf("    OOK Low: %ld\n", *(signal_msg->timingInfo->ookLowEstimate));
                }
                if (signal_msg->timingInfo->ookHighEstimate) {
                    printf("    OOK High: %ld\n", *(signal_msg->timingInfo->ookHighEstimate));
                }
                if (signal_msg->timingInfo->fskF1Est) {
                    printf("    FSK F1: %ld\n", *(signal_msg->timingInfo->fskF1Est));
                }
                if (signal_msg->timingInfo->fskF2Est) {
                    printf("    FSK F2: %ld\n", *(signal_msg->timingInfo->fskF2Est));
                }
            }
        }
        
        ASN_STRUCT_FREE(asn_DEF_RTL433Message, rtl433_msg);
    } else {
        // Decode as RTL433Message (default behavior)
        pulse_data_t *decoded = decode(&buffer);
        if (decoded) {
            printf("✅ RTL433Message decoded successfully\n");
            print_pulse_data(decoded);
            free(decoded);
        } else {
            printf("❌ RTL433Message decoding failed\n");
        }
    }
    
    free(binary_data);
}

rtl433_asn1_buffer_t encode_rtl433_message(pulse_data_t *pulse) {
    rtl433_asn1_buffer_t result = {0};
    
    if (!pulse) {
        fprintf(stderr, "❌ encode_rtl433_message: NULL pulse data\n");
        result.result = RTL433_ASN1_ERROR_INVALID_DATA;
        return result;
    }
    
    // Create pulse_data_ex_t wrapper for the existing pulse_data_t
    pulse_data_ex_t pulse_ex;
    pulse_data_ex_init(&pulse_ex);
    pulse_ex.pulse_data = *pulse; // Copy pulse data
    
    // Use the main encode function
    result = encode(&pulse_ex);
    
    // Clean up
    pulse_data_ex_free(&pulse_ex);
    
    return result;
}
