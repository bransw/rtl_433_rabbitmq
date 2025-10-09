/**
 * @file test_asn1_detected_reader.c
 * @brief Test program to read and decode DetectedMessage from asn1_detected queue
 * 
 * Usage: ./test_asn1_detected_reader [rabbitmq_url]
 * Example: ./test_asn1_detected_reader asn1://guest:guest@localhost:5672/asn1_detected
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

// RabbitMQ
#include <amqp.h>
#include <amqp_tcp_socket.h>

// ASN.1 generated headers
#include "RTL433Message.h"
#include "DetectedMessage.h"
#include "DeviceInfo.h"
#include "DeviceData.h"
#include "DeviceDataField.h"
#include "DeviceFieldValue.h"
#include "DetectionMetadata.h"

// ASN.1 decoder
#include "per_decoder.h"

static volatile int g_exit = 0;

static void signal_handler(int sig)
{
    (void)sig;
    g_exit = 1;
    fprintf(stderr, "\n🛑 Signal caught, exiting...\n");
}

/**
 * Print ASN.1 OCTET_STRING as null-terminated string
 */
static void print_octet_string(const OCTET_STRING_t *str)
{
    if (!str || !str->buf) {
        printf("(null)");
        return;
    }
    
    // Print as string (assuming UTF-8)
    for (size_t i = 0; i < str->size; i++) {
        printf("%c", str->buf[i]);
    }
}

/**
 * Print DeviceFieldValue based on its type
 */
static void print_field_value(const DeviceFieldValue_t *value)
{
    if (!value) {
        printf("(null)");
        return;
    }
    
    switch (value->present) {
        case DeviceFieldValue_PR_integerValue:
            printf("%ld", value->choice.integerValue);
            break;
            
        case DeviceFieldValue_PR_realValue:
            printf("%.2f", value->choice.realValue);
            break;
            
        case DeviceFieldValue_PR_stringValue:
            print_octet_string(&value->choice.stringValue);
            break;
            
        case DeviceFieldValue_PR_booleanValue:
            printf("%s", value->choice.booleanValue ? "true" : "false");
            break;
            
        case DeviceFieldValue_PR_bytesValue:
            printf("0x");
            for (size_t i = 0; i < value->choice.bytesValue.size && i < 16; i++) {
                printf("%02X", value->choice.bytesValue.buf[i]);
            }
            if (value->choice.bytesValue.size > 16) {
                printf("...");
            }
            break;
            
        default:
            printf("(unknown type)");
            break;
    }
}

/**
 * Decode and print DetectedMessage
 */
static void print_detected_message(const DetectedMessage_t *msg)
{
    if (!msg) {
        printf("❌ NULL DetectedMessage\n");
        return;
    }
    
    printf("\n📦 ============ DETECTED MESSAGE ============\n");
    
    // Package ID
    if (msg->packageId) {
        printf("📋 Package ID: %lu\n", *msg->packageId);
    }
    
    // Timestamp
    if (msg->timestamp) {
        printf("🕐 Timestamp: ");
        print_octet_string(msg->timestamp);
        printf("\n");
    }
    
    // Device Info
    printf("\n🔍 DEVICE INFO:\n");
    printf("  Model: ");
    print_octet_string(&msg->deviceInfo.model);
    printf("\n");
    
    if (msg->deviceInfo.deviceType) {
        printf("  Type: ");
        print_octet_string(msg->deviceInfo.deviceType);
        printf("\n");
    }
    
    if (msg->deviceInfo.deviceId) {
        printf("  ID: ");
        print_octet_string(msg->deviceInfo.deviceId);
        printf("\n");
    }
    
    if (msg->deviceInfo.protocol) {
        printf("  Protocol: ");
        print_octet_string(msg->deviceInfo.protocol);
        printf("\n");
    }
    
    // Device Data
    if (msg->deviceData.list.count > 0) {
        printf("\n📊 DEVICE DATA (%d fields):\n", msg->deviceData.list.count);
        
        for (int i = 0; i < msg->deviceData.list.count; i++) {
            DeviceDataField_t *field = msg->deviceData.list.array[i];
            if (field) {
                printf("  ");
                print_octet_string(&field->fieldName);
                printf(": ");
                print_field_value(&field->fieldValue);
                printf("\n");
            }
        }
    }
    
    // Detection Metadata
    if (msg->detectionMeta) {
        printf("\n🎯 DETECTION METADATA:\n");
        
        if (msg->detectionMeta->confidence) {
            printf("  Confidence: %.2f\n", *msg->detectionMeta->confidence);
        }
        
        if (msg->detectionMeta->errorCorrection) {
            printf("  Error Correction: ");
            print_octet_string(msg->detectionMeta->errorCorrection);
            printf("\n");
        }
        
        if (msg->detectionMeta->protocolVersion) {
            printf("  Protocol Version: %ld\n", *msg->detectionMeta->protocolVersion);
        }
        
        if (msg->detectionMeta->rawBits) {
            printf("  Raw Bits: %ld\n", *msg->detectionMeta->rawBits);
        }
    }
    
    printf("============================================\n\n");
}

/**
 * Decode ASN.1 binary data to DetectedMessage
 */
static int decode_detected_message(const uint8_t *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) {
        fprintf(stderr, "❌ Invalid buffer\n");
        return -1;
    }
    
    printf("🔄 Decoding ASN.1 buffer (%zu bytes)...\n", buffer_size);
    
    // Decode RTL433Message wrapper
    RTL433Message_t *rtl_msg = NULL;
    asn_dec_rval_t decode_result = uper_decode_complete(NULL,
                                                        &asn_DEF_RTL433Message,
                                                        (void **)&rtl_msg,
                                                        buffer,
                                                        buffer_size);
    
    if (decode_result.code != RC_OK) {
        fprintf(stderr, "❌ Failed to decode RTL433Message: code=%d, consumed=%zu\n",
                decode_result.code, decode_result.consumed);
        return -1;
    }
    
    printf("✅ RTL433Message decoded (present=%d)\n", rtl_msg->present);
    
    // Check message type
    if (rtl_msg->present != RTL433Message_PR_detectedMessage) {
        fprintf(stderr, "❌ Not a DetectedMessage (present=%d)\n", rtl_msg->present);
        
        if (rtl_msg->present == RTL433Message_PR_signalMessage) {
            fprintf(stderr, "ℹ️  This is a SignalMessage (use test_asn1_signal_reader)\n");
        }
        
        ASN_STRUCT_FREE(asn_DEF_RTL433Message, rtl_msg);
        return -1;
    }
    
    // Extract DetectedMessage
    DetectedMessage_t *detected_msg = &rtl_msg->choice.detectedMessage;
    
    // Print the message
    print_detected_message(detected_msg);
    
    // Cleanup
    ASN_STRUCT_FREE(asn_DEF_RTL433Message, rtl_msg);
    
    return 0;
}

/**
 * Process received message
 */
static void process_message(const uint8_t *data, size_t size, int *message_count)
{
    (*message_count)++;
    
    printf("\n📨 Received message #%d (%zu bytes)\n", *message_count, size);
    
    // Print hex dump (first 64 bytes)
    printf("🔍 HEX: ");
    size_t hex_len = size < 64 ? size : 64;
    for (size_t i = 0; i < hex_len; i++) {
        printf("%02X", data[i]);
    }
    if (size > 64) {
        printf("... (%zu more bytes)", size - 64);
    }
    printf("\n");
    
    // Decode and print
    int result = decode_detected_message(data, size);
    
    if (result != 0) {
        fprintf(stderr, "⚠️  Failed to decode message #%d\n", *message_count);
    }
}

int main(int argc, char *argv[])
{
    const char *queue_name = "asn1_detected";
    const char *hostname = "localhost";
    int port = 5672;
    const char *username = "guest";
    const char *password = "guest";
    
    if (argc > 1) {
        queue_name = argv[1];
    }
    
    printf("🚀 ASN.1 DetectedMessage Reader\n");
    printf("================================\n");
    printf("Queue: %s\n", queue_name);
    printf("Host: %s:%d\n", hostname, port);
    printf("Press Ctrl+C to exit\n\n");
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Connect to RabbitMQ
    amqp_connection_state_t conn = amqp_new_connection();
    amqp_socket_t *socket = amqp_tcp_socket_new(conn);
    
    if (!socket) {
        fprintf(stderr, "❌ Failed to create TCP socket\n");
        return 1;
    }
    
    printf("🔌 Connecting to RabbitMQ...\n");
    
    int status = amqp_socket_open(socket, hostname, port);
    if (status) {
        fprintf(stderr, "❌ Failed to open TCP socket\n");
        return 1;
    }
    
    amqp_rpc_reply_t reply = amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, 
                                        username, password);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        fprintf(stderr, "❌ Failed to login\n");
        return 1;
    }
    
    amqp_channel_open(conn, 1);
    reply = amqp_get_rpc_reply(conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        fprintf(stderr, "❌ Failed to open channel\n");
        return 1;
    }
    
    printf("✅ Connected!\n");
    printf("👂 Listening for messages from queue '%s'...\n\n", queue_name);
    
    // Start consuming
    amqp_basic_consume(conn, 1, amqp_cstring_bytes(queue_name), 
                      amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
    reply = amqp_get_rpc_reply(conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        fprintf(stderr, "❌ Failed to start consuming\n");
        return 1;
    }
    
    int message_count = 0;
    
    // Receive messages loop
    while (!g_exit) {
        amqp_envelope_t envelope;
        amqp_maybe_release_buffers(conn);
        
        struct timeval timeout = {1, 0}; // 1 second timeout
        reply = amqp_consume_message(conn, &envelope, &timeout, 0);
        
        if (reply.reply_type == AMQP_RESPONSE_NORMAL) {
            // Process message
            process_message(envelope.message.body.bytes, 
                          envelope.message.body.len,
                          &message_count);
            
            amqp_destroy_envelope(&envelope);
        }
        else if (reply.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION &&
                 reply.library_error == AMQP_STATUS_TIMEOUT) {
            // Timeout - continue
            continue;
        }
        else if (!g_exit) {
            fprintf(stderr, "⚠️  Receive error: %d\n", reply.reply_type);
            break;
        }
    }
    
    printf("\n🧹 Cleaning up...\n");
    amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conn);
    
    printf("✅ Done! Processed %d messages\n", message_count);
    
    return 0;
}

