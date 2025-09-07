/** @file
    RabbitMQ output for rtl_433 events

    Copyright (C) 2024

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

// note: our unit header includes unistd.h for gethostname() via data.h
#include "output_rabbitmq.h"
#include "optparse.h"
#include "bit_util.h"
#include "logger.h"
#include "fatal.h"
#include "r_util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "mongoose.h"

/* RabbitMQ client abstraction using AMQP 0.9.1 protocol */

typedef struct rabbitmq_client {
    struct mg_connect_opts connect_opts;
    struct mg_connection *conn;
    struct mg_connection *timer;
    int reconnect_delay;
    int prev_status;
    char address[253 + 6 + 1]; // dns max + port
    char username[256];
    char password[256];
    char vhost[256];
    char exchange[256];
    char result_queue[256];
    char unknown_queue[256];
    int connected;
    int channel_id;
} rabbitmq_client_t;

/* AMQP 0.9.1 protocol constants */
#define AMQP_PROTOCOL_HEADER "AMQP\x00\x00\x09\x01"
#define AMQP_FRAME_METHOD 1
#define AMQP_FRAME_CONTENT_HEADER 2
#define AMQP_FRAME_BODY 3
#define AMQP_FRAME_HEARTBEAT 8

/* AMQP method classes */
#define AMQP_CONNECTION_CLASS 10
#define AMQP_CHANNEL_CLASS 20
#define AMQP_EXCHANGE_CLASS 40
#define AMQP_QUEUE_CLASS 50
#define AMQP_BASIC_CLASS 60

/* AMQP methods */
#define AMQP_CONNECTION_START 10
#define AMQP_CONNECTION_START_OK 11
#define AMQP_CONNECTION_TUNE 30
#define AMQP_CONNECTION_TUNE_OK 31
#define AMQP_CONNECTION_OPEN 40
#define AMQP_CONNECTION_OPEN_OK 41
#define AMQP_CONNECTION_CLOSE 50
#define AMQP_CONNECTION_CLOSE_OK 51

#define AMQP_CHANNEL_OPEN 10
#define AMQP_CHANNEL_OPEN_OK 11
#define AMQP_CHANNEL_CLOSE 40
#define AMQP_CHANNEL_CLOSE_OK 41

#define AMQP_EXCHANGE_DECLARE 10
#define AMQP_EXCHANGE_DECLARE_OK 11

#define AMQP_QUEUE_DECLARE 10
#define AMQP_QUEUE_DECLARE_OK 11
#define AMQP_QUEUE_BIND 20
#define AMQP_QUEUE_BIND_OK 21

#define AMQP_BASIC_PUBLISH 40
#define AMQP_BASIC_ACK 80

/* Helper functions for AMQP protocol */

static void amqp_write_octet(struct mbuf *mbuf, uint8_t value)
{
    mbuf_append(mbuf, &value, 1);
}

static void amqp_write_short(struct mbuf *mbuf, uint16_t value)
{
    uint8_t bytes[2];
    bytes[0] = (value >> 8) & 0xFF;
    bytes[1] = value & 0xFF;
    mbuf_append(mbuf, bytes, 2);
}

static void amqp_write_long(struct mbuf *mbuf, uint32_t value)
{
    uint8_t bytes[4];
    bytes[0] = (value >> 24) & 0xFF;
    bytes[1] = (value >> 16) & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = value & 0xFF;
    mbuf_append(mbuf, bytes, 4);
}

static void amqp_write_longlong(struct mbuf *mbuf, uint64_t value)
{
    uint8_t bytes[8];
    bytes[0] = (value >> 56) & 0xFF;
    bytes[1] = (value >> 48) & 0xFF;
    bytes[2] = (value >> 40) & 0xFF;
    bytes[3] = (value >> 32) & 0xFF;
    bytes[4] = (value >> 24) & 0xFF;
    bytes[5] = (value >> 16) & 0xFF;
    bytes[6] = (value >> 8) & 0xFF;
    bytes[7] = value & 0xFF;
    mbuf_append(mbuf, bytes, 8);
}

static void amqp_write_shortstr(struct mbuf *mbuf, const char *str)
{
    size_t len = str ? strlen(str) : 0;
    if (len > 255) len = 255;
    amqp_write_octet(mbuf, (uint8_t)len);
    if (len > 0) {
        mbuf_append(mbuf, str, len);
    }
}

static void amqp_write_longstr(struct mbuf *mbuf, const char *str)
{
    size_t len = str ? strlen(str) : 0;
    amqp_write_long(mbuf, (uint32_t)len);
    if (len > 0) {
        mbuf_append(mbuf, str, len);
    }
}

static void amqp_write_table(struct mbuf *mbuf, const char *key, const char *value)
{
    if (!key || !value) return;
    
    amqp_write_shortstr(mbuf, key);
    amqp_write_octet(mbuf, 'S'); // string type
    amqp_write_longstr(mbuf, value);
}

static void amqp_send_frame(struct mg_connection *nc, uint8_t type, uint16_t channel, const void *payload, size_t payload_len)
{
    struct mbuf *mbuf = &nc->send_mbuf;
    size_t frame_size = 1 + 2 + 4 + payload_len + 1; // type + channel + size + payload + end
    
    amqp_write_octet(mbuf, type);
    amqp_write_short(mbuf, channel);
    amqp_write_long(mbuf, (uint32_t)payload_len);
    
    if (payload_len > 0) {
        mbuf_append(mbuf, payload, payload_len);
    }
    
    amqp_write_octet(mbuf, 0xCE); // frame end
}

static void amqp_send_method(struct mg_connection *nc, uint16_t channel, uint16_t class_id, uint16_t method_id, const void *payload, size_t payload_len)
{
    struct mbuf method_payload;
    mbuf_init(&method_payload, 0);
    
    amqp_write_short(&method_payload, class_id);
    amqp_write_short(&method_payload, method_id);
    
    if (payload_len > 0) {
        mbuf_append(&method_payload, payload, payload_len);
    }
    
    amqp_send_frame(nc, AMQP_FRAME_METHOD, channel, method_payload.buf, method_payload.len);
    mbuf_free(&method_payload);
}

static void rabbitmq_send_connection_start_ok(rabbitmq_client_t *ctx)
{
    struct mbuf payload;
    mbuf_init(&payload, 0);
    
    // connection.start-ok
    amqp_write_table(&payload, "product", "rtl_433");
    amqp_write_table(&payload, "version", "1.0");
    amqp_write_table(&payload, "platform", "C");
    amqp_write_table(&payload, "copyright", "rtl_433");
    amqp_write_table(&payload, "information", "rtl_433 RabbitMQ client");
    amqp_write_octet(&payload, 0); // end of table
    
    amqp_write_shortstr(&payload, "AMQPLAIN"); // mechanism
    amqp_write_longstr(&payload, ""); // response (empty for AMQPLAIN)
    amqp_write_shortstr(&payload, "en_US"); // locale
    
    amqp_send_method(ctx->conn, 0, AMQP_CONNECTION_CLASS, AMQP_CONNECTION_START_OK, payload.buf, payload.len);
    mbuf_free(&payload);
}

static void rabbitmq_send_connection_tune_ok(rabbitmq_client_t *ctx)
{
    struct mbuf payload;
    mbuf_init(&payload, 0);
    
    // connection.tune-ok
    amqp_write_short(&payload, 0); // channel_max
    amqp_write_long(&payload, 131072); // frame_max
    amqp_write_short(&payload, 60); // heartbeat
    
    amqp_send_method(ctx->conn, 0, AMQP_CONNECTION_CLASS, AMQP_CONNECTION_TUNE_OK, payload.buf, payload.len);
    mbuf_free(&payload);
}

static void rabbitmq_send_connection_open(rabbitmq_client_t *ctx)
{
    struct mbuf payload;
    mbuf_init(&payload, 0);
    
    // connection.open
    amqp_write_shortstr(&payload, ctx->vhost);
    amqp_write_shortstr(&payload, ""); // capabilities
    amqp_write_octet(&payload, 0); // insist
    
    amqp_send_method(ctx->conn, 0, AMQP_CONNECTION_CLASS, AMQP_CONNECTION_OPEN, payload.buf, payload.len);
    mbuf_free(&payload);
}

static void rabbitmq_send_channel_open(rabbitmq_client_t *ctx)
{
    struct mbuf payload;
    mbuf_init(&payload, 0);
    
    // channel.open
    amqp_write_shortstr(&payload, ""); // out-of-band
    
    amqp_send_method(ctx->conn, ctx->channel_id, AMQP_CHANNEL_CLASS, AMQP_CHANNEL_OPEN, payload.buf, payload.len);
    mbuf_free(&payload);
}

static void rabbitmq_send_exchange_declare(rabbitmq_client_t *ctx)
{
    struct mbuf payload;
    mbuf_init(&payload, 0);
    
    // exchange.declare
    amqp_write_short(&payload, 0); // reserved1
    amqp_write_shortstr(&payload, ctx->exchange);
    amqp_write_shortstr(&payload, "direct"); // type
    amqp_write_octet(&payload, 0); // passive
    amqp_write_octet(&payload, 1); // durable
    amqp_write_octet(&payload, 0); // auto_delete
    amqp_write_octet(&payload, 0); // internal
    amqp_write_octet(&payload, 0); // nowait
    amqp_write_table(&payload, "", ""); // arguments (empty table)
    amqp_write_octet(&payload, 0); // end of table
    
    amqp_send_method(ctx->conn, ctx->channel_id, AMQP_EXCHANGE_CLASS, AMQP_EXCHANGE_DECLARE, payload.buf, payload.len);
    mbuf_free(&payload);
}

static void rabbitmq_send_queue_declare(rabbitmq_client_t *ctx, const char *queue_name)
{
    struct mbuf payload;
    mbuf_init(&payload, 0);
    
    // queue.declare
    amqp_write_short(&payload, 0); // reserved1
    amqp_write_shortstr(&payload, queue_name);
    amqp_write_octet(&payload, 0); // passive
    amqp_write_octet(&payload, 1); // durable
    amqp_write_octet(&payload, 0); // exclusive
    amqp_write_octet(&payload, 0); // auto_delete
    amqp_write_octet(&payload, 0); // nowait
    amqp_write_table(&payload, "", ""); // arguments (empty table)
    amqp_write_octet(&payload, 0); // end of table
    
    amqp_send_method(ctx->conn, ctx->channel_id, AMQP_QUEUE_CLASS, AMQP_QUEUE_DECLARE, payload.buf, payload.len);
    mbuf_free(&payload);
}

static void rabbitmq_send_queue_bind(rabbitmq_client_t *ctx, const char *queue_name, const char *routing_key)
{
    struct mbuf payload;
    mbuf_init(&payload, 0);
    
    // queue.bind
    amqp_write_short(&payload, 0); // reserved1
    amqp_write_shortstr(&payload, queue_name);
    amqp_write_shortstr(&payload, ctx->exchange);
    amqp_write_shortstr(&payload, routing_key);
    amqp_write_octet(&payload, 0); // nowait
    amqp_write_table(&payload, "", ""); // arguments (empty table)
    amqp_write_octet(&payload, 0); // end of table
    
    amqp_send_method(ctx->conn, ctx->channel_id, AMQP_QUEUE_CLASS, AMQP_QUEUE_BIND, payload.buf, payload.len);
    mbuf_free(&payload);
}

static void rabbitmq_send_basic_publish(rabbitmq_client_t *ctx, const char *routing_key, const char *body)
{
    struct mbuf payload;
    mbuf_init(&payload, 0);
    
    // basic.publish
    amqp_write_short(&payload, 0); // reserved1
    amqp_write_shortstr(&payload, ctx->exchange);
    amqp_write_shortstr(&payload, routing_key);
    amqp_write_octet(&payload, 0); // mandatory
    amqp_write_octet(&payload, 0); // immediate
    
    amqp_send_method(ctx->conn, ctx->channel_id, AMQP_BASIC_CLASS, AMQP_BASIC_PUBLISH, payload.buf, payload.len);
    mbuf_free(&payload);
    
    // Send content header
    struct mbuf header;
    mbuf_init(&header, 0);
    
    amqp_write_short(&header, 0); // class_id
    amqp_write_short(&header, 0); // weight
    amqp_write_longlong(&header, strlen(body)); // body_size
    amqp_write_short(&header, 0); // property_flags
    
    amqp_send_frame(ctx->conn, AMQP_FRAME_CONTENT_HEADER, ctx->channel_id, header.buf, header.len);
    mbuf_free(&header);
    
    // Send body
    amqp_send_frame(ctx->conn, AMQP_FRAME_BODY, ctx->channel_id, body, strlen(body));
}

static void rabbitmq_client_event(struct mg_connection *nc, int ev, void *ev_data)
{
    rabbitmq_client_t *ctx = (rabbitmq_client_t *)nc->user_data;
    
    switch (ev) {
    case MG_EV_CONNECT: {
        int connect_status = *(int *)ev_data;
        if (connect_status == 0) {
            print_log(LOG_NOTICE, "RabbitMQ", "RabbitMQ Connected...");
            // Send AMQP protocol header
            mg_send(nc, AMQP_PROTOCOL_HEADER, strlen(AMQP_PROTOCOL_HEADER));
        }
        else {
            if (ctx && ctx->prev_status != connect_status) {
                print_logf(LOG_WARNING, "RabbitMQ", "RabbitMQ connect error: %s", strerror(connect_status));
            }
        }
        if (ctx) {
            ctx->prev_status = connect_status;
        }
        break;
    }
    case MG_EV_RECV: {
        if (!ctx) break;
        
        struct mbuf *mbuf = &nc->recv_mbuf;
        size_t available = mbuf->len;
        
        while (available >= 7) { // minimum frame size
            uint8_t frame_type = mbuf->buf[0];
            uint16_t channel = (mbuf->buf[1] << 8) | mbuf->buf[2];
            uint32_t frame_size = (mbuf->buf[3] << 24) | (mbuf->buf[4] << 16) | (mbuf->buf[5] << 8) | mbuf->buf[6];
            
            if (available < 7 + frame_size + 1) break; // incomplete frame
            
            if (frame_type == AMQP_FRAME_METHOD) {
                if (frame_size >= 4) {
                    uint16_t class_id = (mbuf->buf[7] << 8) | mbuf->buf[8];
                    uint16_t method_id = (mbuf->buf[9] << 8) | mbuf->buf[10];
                    
                    if (class_id == AMQP_CONNECTION_CLASS) {
                        if (method_id == AMQP_CONNECTION_START) {
                            rabbitmq_send_connection_start_ok(ctx);
                        }
                        else if (method_id == AMQP_CONNECTION_TUNE) {
                            rabbitmq_send_connection_tune_ok(ctx);
                        }
                        else if (method_id == AMQP_CONNECTION_OPEN_OK) {
                            print_log(LOG_NOTICE, "RabbitMQ", "RabbitMQ Connection established.");
                            ctx->connected = 1;
                            rabbitmq_send_channel_open(ctx);
                        }
                    }
                    else if (class_id == AMQP_CHANNEL_CLASS) {
                        if (method_id == AMQP_CHANNEL_OPEN_OK) {
                            print_log(LOG_NOTICE, "RabbitMQ", "RabbitMQ Channel opened.");
                            rabbitmq_send_exchange_declare(ctx);
                        }
                    }
                    else if (class_id == AMQP_EXCHANGE_CLASS) {
                        if (method_id == AMQP_EXCHANGE_DECLARE_OK) {
                            print_log(LOG_NOTICE, "RabbitMQ", "RabbitMQ Exchange declared.");
                            rabbitmq_send_queue_declare(ctx, ctx->result_queue);
                        }
                    }
                    else if (class_id == AMQP_QUEUE_CLASS) {
                        if (method_id == AMQP_QUEUE_DECLARE_OK) {
                            static int queue_count = 0;
                            queue_count++;
                            if (queue_count == 1) {
                                rabbitmq_send_queue_bind(ctx, ctx->result_queue, "detected");
                            }
                            else if (queue_count == 2) {
                                rabbitmq_send_queue_bind(ctx, ctx->unknown_queue, "undetected");
                                print_log(LOG_NOTICE, "RabbitMQ", "RabbitMQ Setup complete.");
                            }
                        }
                        else if (method_id == AMQP_QUEUE_BIND_OK) {
                            static int bind_count = 0;
                            bind_count++;
                            if (bind_count == 1) {
                                rabbitmq_send_queue_declare(ctx, ctx->unknown_queue);
                            }
                        }
                    }
                }
            }
            
            // Remove processed frame
            mbuf_remove(mbuf, 7 + frame_size + 1);
            available = mbuf->len;
        }
        break;
    }
    case MG_EV_CLOSE:
        if (!ctx) break;
        ctx->conn = NULL;
        ctx->connected = 0;
        if (!ctx->timer) break;
        if (ctx->prev_status == 0) {
            print_log(LOG_WARNING, "RabbitMQ", "RabbitMQ Connection lost, reconnecting...");
        }
        mg_set_timer(ctx->timer, mg_time() + ctx->reconnect_delay);
        if (ctx->reconnect_delay < 60) {
            ctx->reconnect_delay = (ctx->reconnect_delay + 1) * 3 / 2;
        }
        break;
    }
}

static void rabbitmq_client_timer(struct mg_connection *nc, int ev, void *ev_data)
{
    rabbitmq_client_t *ctx = (rabbitmq_client_t *)nc->user_data;
    (void)ev_data;
    
    switch (ev) {
    case MG_EV_TIMER: {
        char const *error_string = NULL;
        ctx->connect_opts.error_string = &error_string;
        ctx->conn = mg_connect_opt(nc->mgr, ctx->address, rabbitmq_client_event, ctx->connect_opts);
        ctx->connect_opts.error_string = NULL;
        if (!ctx->conn) {
            print_logf(LOG_WARNING, "RabbitMQ", "RabbitMQ connect (%s) failed%s%s", ctx->address,
                    error_string ? ": " : "", error_string ? error_string : "");
        }
        break;
    }
    }
}

static rabbitmq_client_t *rabbitmq_client_init(struct mg_mgr *mgr, char const *host, char const *port, char const *user, char const *pass, char const *vhost)
{
    rabbitmq_client_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        FATAL_CALLOC("rabbitmq_client_init()");
    
    snprintf(ctx->username, sizeof(ctx->username), "%s", user ? user : "guest");
    snprintf(ctx->password, sizeof(ctx->password), "%s", pass ? pass : "guest");
    snprintf(ctx->vhost, sizeof(ctx->vhost), "%s", vhost ? vhost : "/");
    snprintf(ctx->exchange, sizeof(ctx->exchange), "rtl_433");
    snprintf(ctx->result_queue, sizeof(ctx->result_queue), "result_queue");
    snprintf(ctx->unknown_queue, sizeof(ctx->unknown_queue), "unknown_queue");
    ctx->channel_id = 1;
    
    // if the host is an IPv6 address it needs quoting
    if (strchr(host, ':'))
        snprintf(ctx->address, sizeof(ctx->address), "[%s]:%s", host, port);
    else
        snprintf(ctx->address, sizeof(ctx->address), "%s:%s", host, port);
    
    ctx->connect_opts.user_data = ctx;
    
    // add dummy socket to receive timer events
    struct mg_add_sock_opts opts = {.user_data = ctx};
    ctx->timer = mg_add_sock_opt(mgr, INVALID_SOCKET, rabbitmq_client_timer, opts);
    
    char const *error_string = NULL;
    ctx->connect_opts.error_string = &error_string;
    ctx->conn = mg_connect_opt(mgr, ctx->address, rabbitmq_client_event, ctx->connect_opts);
    ctx->connect_opts.error_string = NULL;
    if (!ctx->conn) {
        print_logf(LOG_FATAL, "RabbitMQ", "RabbitMQ connect (%s) failed%s%s", ctx->address,
                error_string ? ": " : "", error_string ? error_string : "");
        exit(1);
    }
    
    return ctx;
}

static void rabbitmq_client_publish(rabbitmq_client_t *ctx, const char *routing_key, const char *message)
{
    if (!ctx->conn || !ctx->connected)
        return;
    
    rabbitmq_send_basic_publish(ctx, routing_key, message);
}

static void rabbitmq_client_free(rabbitmq_client_t *ctx)
{
    if (ctx && ctx->conn) {
        ctx->conn->user_data = NULL;
        ctx->conn->flags |= MG_F_CLOSE_IMMEDIATELY;
    }
    free(ctx);
}

/* RabbitMQ printer */

typedef struct {
    struct data_output output;
    rabbitmq_client_t *rmqc;
    char hostname[64];
} data_output_rabbitmq_t;

static void R_API_CALLCONV print_rabbitmq_data(data_output_t *output, data_t *data, char const *format)
{
    UNUSED(format);
    data_output_rabbitmq_t *rabbitmq = (data_output_rabbitmq_t *)output;
    
    // Check if this is a detected packet (has model field) or undetected packet
    data_t *data_model = NULL;
    for (data_t *d = data; d; d = d->next) {
        if (!strcmp(d->key, "model")) {
            data_model = d;
            break;
        }
    }
    
    // Create JSON message
    char message[2048];
    data_print_jsons(data, message, sizeof(message));
    
    // Send to appropriate queue
    if (data_model) {
        // Detected packet - send to result_queue
        rabbitmq_client_publish(rabbitmq->rmqc, "detected", message);
    } else {
        // Undetected packet - send to unknown_queue
        rabbitmq_client_publish(rabbitmq->rmqc, "undetected", message);
    }
}

static void R_API_CALLCONV print_rabbitmq_string(data_output_t *output, char const *str, char const *format)
{
    UNUSED(format);
    data_output_rabbitmq_t *rabbitmq = (data_output_rabbitmq_t *)output;
    // For string output, treat as undetected packet
    rabbitmq_client_publish(rabbitmq->rmqc, "undetected", str);
}

static void R_API_CALLCONV print_rabbitmq_double(data_output_t *output, double data, char const *format)
{
    char str[20];
    if (data > 1e7 || data < 1e-4) {
        snprintf(str, sizeof(str), "%g", data);
    }
    else {
        int ret = snprintf(str, sizeof(str), "%.5f", data);
        char *p = str + ret - 1;
        while (*p == '0' && p[-1] != '.') {
            *p-- = '\0';
        }
    }
    print_rabbitmq_string(output, str, format);
}

static void R_API_CALLCONV print_rabbitmq_int(data_output_t *output, int data, char const *format)
{
    char str[20];
    snprintf(str, sizeof(str), "%d", data);
    print_rabbitmq_string(output, str, format);
}

static void R_API_CALLCONV data_output_rabbitmq_free(data_output_t *output)
{
    data_output_rabbitmq_t *rabbitmq = (data_output_rabbitmq_t *)output;
    
    if (!rabbitmq)
        return;
    
    rabbitmq_client_free(rabbitmq->rmqc);
    free(rabbitmq);
}

struct data_output *data_output_rabbitmq_create(struct mg_mgr *mgr, char *param, char const *dev_hint)
{
    data_output_rabbitmq_t *rabbitmq = calloc(1, sizeof(data_output_rabbitmq_t));
    if (!rabbitmq)
        FATAL_CALLOC("data_output_rabbitmq_create()");
    
    gethostname(rabbitmq->hostname, sizeof(rabbitmq->hostname) - 1);
    rabbitmq->hostname[sizeof(rabbitmq->hostname) - 1] = '\0';
    char *dot = strchr(rabbitmq->hostname, '.');
    if (dot)
        *dot = '\0';
    
    // get user and pass from env vars if available
    char const *user = getenv("RABBITMQ_USERNAME");
    char const *pass = getenv("RABBITMQ_PASSWORD");
    char const *vhost = getenv("RABBITMQ_VHOST");
    
    // parse host and port
    param = arg_param(param); // strip scheme
    char const *host = "localhost";
    char const *port = "5672";
    char *opts = hostport_param(param, &host, &port);
    print_logf(LOG_CRITICAL, "RabbitMQ", "Publishing RabbitMQ data to %s port %s", host, port);
    
    // parse options
    char *key, *val;
    while (getkwargs(&opts, &key, &val)) {
        key = remove_ws(key);
        val = trim_ws(val);
        if (!key || !*key)
            continue;
        else if (!strcasecmp(key, "u") || !strcasecmp(key, "user"))
            user = val;
        else if (!strcasecmp(key, "p") || !strcasecmp(key, "pass"))
            pass = val;
        else if (!strcasecmp(key, "v") || !strcasecmp(key, "vhost"))
            vhost = val;
        else if (!strcasecmp(key, "e") || !strcasecmp(key, "exchange"))
            // TODO: implement exchange option
            continue;
        else if (!strcasecmp(key, "rq") || !strcasecmp(key, "result_queue"))
            // TODO: implement result_queue option
            continue;
        else if (!strcasecmp(key, "uq") || !strcasecmp(key, "unknown_queue"))
            // TODO: implement unknown_queue option
            continue;
        else {
            print_logf(LOG_FATAL, __func__, "Invalid key \"%s\" option.", key);
            exit(1);
        }
    }
    
    rabbitmq->output.print_data   = print_rabbitmq_data;
    rabbitmq->output.print_string = print_rabbitmq_string;
    rabbitmq->output.print_double = print_rabbitmq_double;
    rabbitmq->output.print_int    = print_rabbitmq_int;
    rabbitmq->output.output_free  = data_output_rabbitmq_free;
    
    rabbitmq->rmqc = rabbitmq_client_init(mgr, host, port, user, pass, vhost);
    
    return (struct data_output *)rabbitmq;
}

