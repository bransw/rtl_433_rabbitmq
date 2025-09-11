/** @file
    Реализация транспортного слоя для rtl_433_client.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "client_transport.h"
#include "logger.h"
#include "r_util.h"

#ifdef ENABLE_MQTT
#include <MQTTClient.h>
#endif

#include <curl/curl.h>

#ifdef ENABLE_RABBITMQ
#include <amqp.h>
#include <amqp_tcp_socket.h>
#endif

/// Структура для HTTP ответа
struct http_response {
    char *data;
    size_t size;
};

/// Callback для получения HTTP ответа
static size_t http_write_callback(void *contents, size_t size, size_t nmemb, struct http_response *response)
{
    size_t realsize = size * nmemb;
    char *ptr = realloc(response->data, response->size + realsize + 1);
    
    if (!ptr) {
        print_log(LOG_ERROR, "Transport", "Failed to allocate memory for HTTP response");
        return 0;
    }
    
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;
    
    return realsize;
}

/// Инициализировать транспортное соединение
int transport_init(transport_connection_t *conn, transport_config_t *config)
{
    if (!conn || !config) {
        return -1;
    }
    
    memset(conn, 0, sizeof(transport_connection_t));
    conn->config = config;
    conn->connected = 0;
    
    switch (config->type) {
        case TRANSPORT_HTTP:
            return transport_http_init(conn);
            
#ifdef ENABLE_MQTT
        case TRANSPORT_MQTT:
            return transport_mqtt_init(conn);
#endif

#ifdef ENABLE_RABBITMQ
        case TRANSPORT_RABBITMQ:
            return transport_rabbitmq_init(conn);
#endif

        case TRANSPORT_TCP:
        case TRANSPORT_UDP:
            return transport_socket_init(conn);
            
        default:
            print_logf(LOG_ERROR, "Transport", "Unsupported transport type: %d", config->type);
            return -1;
    }
}

/// Подключиться к серверу
int transport_connect(transport_connection_t *conn)
{
    if (!conn || !conn->config) {
        return -1;
    }
    
    switch (conn->config->type) {
        case TRANSPORT_HTTP:
            // HTTP не требует постоянного соединения
            conn->connected = 1;
            return 0;
            
#ifdef ENABLE_MQTT
        case TRANSPORT_MQTT:
            return transport_mqtt_connect(conn);
#endif

#ifdef ENABLE_RABBITMQ
        case TRANSPORT_RABBITMQ:
            return transport_rabbitmq_connect(conn);
#endif

        case TRANSPORT_TCP:
        case TRANSPORT_UDP:
            return transport_socket_connect(conn);
            
        default:
            return -1;
    }
}

/// Helper functions for transport protocols
static int send_http_data(transport_connection_t *conn, const char *json_str)
{
    return transport_http_send(conn, json_str);
}

static int send_mqtt_data(transport_connection_t *conn, const char *json_str)
{
#ifdef ENABLE_MQTT
    return transport_mqtt_send(conn, json_str);
#else
    (void)conn; (void)json_str;
    print_log(LOG_ERROR, "Transport", "MQTT support not compiled");
    return -1;
#endif
}

static int send_rabbitmq_data(transport_connection_t *conn, const char *json_str)
{
#ifdef ENABLE_RABBITMQ
    return transport_rabbitmq_send(conn, json_str);
#else
    (void)conn; (void)json_str;
    print_log(LOG_ERROR, "Transport", "RabbitMQ support not compiled");
    return -1;
#endif
}

static int send_socket_data(transport_connection_t *conn, const char *json_str)
{
    return transport_socket_send(conn, json_str);
}

/// Send pulse data 
int transport_send_pulse_data(transport_connection_t *conn, const pulse_data_t *pulse_data)
{
    if (!conn || !pulse_data || !pulse_data->num_pulses) {
        return -1;
    }
    
    // Convert pulse_data to data_t and then to JSON
    data_t *data = pulse_data_print_data(pulse_data);
    if (!data) {
        print_log(LOG_ERROR, "Transport", "Failed to convert pulse data");
        return -1;
    }
    
    // Create JSON string from data_t
    char json_buffer[8192];
    size_t json_len = data_print_jsons(data, json_buffer, sizeof(json_buffer));
    if (json_len == 0) {
        print_log(LOG_ERROR, "Transport", "Failed to serialize pulse data to JSON");
        data_free(data);
        return -1;
    }
    
    char *json_str = strdup(json_buffer);
    data_free(data);
    
    if (!json_str) {
        print_log(LOG_ERROR, "Transport", "Failed to allocate memory for JSON");
        return -1;
    }
    
    int result = 0;
    switch (conn->config->type) {
        case TRANSPORT_HTTP:
            result = send_http_data(conn, json_str);
            break;
        case TRANSPORT_MQTT:
            result = send_mqtt_data(conn, json_str);
            break;
        case TRANSPORT_RABBITMQ:
            result = send_rabbitmq_data(conn, json_str);
            break;
        case TRANSPORT_TCP:
        case TRANSPORT_UDP:
            result = send_socket_data(conn, json_str);
            break;
        default:
            print_logf(LOG_ERROR, "Transport", "Unsupported transport type: %d", conn->config->type);
            result = -1;
            break;
    }
    
    free(json_str);
    return result;
}

/// Отправить демодулированные данные
int transport_send_demod_data(transport_connection_t *conn, const demod_data_t *data)
{
    if (!conn || !data || !conn->connected) {
        return -1;
    }
    
    // Сериализуем данные в JSON
    char *json_data = demod_data_to_json(data);
    if (!json_data) {
        print_log(LOG_ERROR, "Transport", "Failed to serialize data");
        return -1;
    }
    
    int result = -1;
    
    switch (conn->config->type) {
        case TRANSPORT_HTTP:
            result = transport_http_send(conn, json_data);
            break;
            
#ifdef ENABLE_MQTT
        case TRANSPORT_MQTT:
            result = transport_mqtt_send(conn, json_data);
            break;
#endif

#ifdef ENABLE_RABBITMQ
        case TRANSPORT_RABBITMQ:
            result = transport_rabbitmq_send(conn, json_data);
            break;
#endif

        case TRANSPORT_TCP:
        case TRANSPORT_UDP:
            result = transport_socket_send(conn, json_data);
            break;
            
        default:
            print_logf(LOG_ERROR, "Transport", "Unsupported transport type: %d", conn->config->type);
    }
    
    free(json_data);
    return result;
}

/// Отправить батч демодулированных данных
int transport_send_demod_batch(transport_connection_t *conn, const demod_data_t *data_array, int count)
{
    if (!conn || !data_array || count <= 0 || !conn->connected) {
        return -1;
    }
    
    // Сериализуем батч в JSON
    char *json_data = demod_data_batch_to_json(data_array, count);
    if (!json_data) {
        print_log(LOG_ERROR, "Transport", "Failed to serialize batch data");
        return -1;
    }
    
    int result = -1;
    
    switch (conn->config->type) {
        case TRANSPORT_HTTP:
            result = transport_http_send(conn, json_data);
            break;
            
#ifdef ENABLE_MQTT
        case TRANSPORT_MQTT:
            result = transport_mqtt_send(conn, json_data);
            break;
#endif

#ifdef ENABLE_RABBITMQ
        case TRANSPORT_RABBITMQ:
            result = transport_rabbitmq_send(conn, json_data);
            break;
#endif

        case TRANSPORT_TCP:
        case TRANSPORT_UDP:
            result = transport_socket_send(conn, json_data);
            break;
    }
    
    free(json_data);
    return result;
}

/// Проверить состояние соединения
int transport_is_connected(const transport_connection_t *conn)
{
    return conn ? conn->connected : 0;
}

/// Отключиться от сервера
void transport_disconnect(transport_connection_t *conn)
{
    if (!conn) {
        return;
    }
    
    conn->connected = 0;
    
    switch (conn->config->type) {
        case TRANSPORT_HTTP:
            // HTTP не требует отключения
            break;
            
#ifdef ENABLE_MQTT
        case TRANSPORT_MQTT:
            transport_mqtt_cleanup(conn);
            break;
#endif

#ifdef ENABLE_RABBITMQ
        case TRANSPORT_RABBITMQ:
            transport_rabbitmq_cleanup(conn);
            break;
#endif

        case TRANSPORT_TCP:
        case TRANSPORT_UDP:
            transport_socket_cleanup(conn);
            break;
    }
}

/// Освободить ресурсы транспорта
void transport_cleanup(transport_connection_t *conn)
{
    if (!conn) {
        return;
    }
    
    transport_disconnect(conn);
    
    switch (conn->config->type) {
        case TRANSPORT_HTTP:
            transport_http_cleanup(conn);
            break;
            
#ifdef ENABLE_MQTT
        case TRANSPORT_MQTT:
            transport_mqtt_cleanup(conn);
            break;
#endif

#ifdef ENABLE_RABBITMQ
        case TRANSPORT_RABBITMQ:
            transport_rabbitmq_cleanup(conn);
            break;
#endif

        case TRANSPORT_TCP:
        case TRANSPORT_UDP:
            transport_socket_cleanup(conn);
            break;
    }
}

/// HTTP специфичные функции
int transport_http_init(transport_connection_t *conn)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    CURL *curl = curl_easy_init();
    if (!curl) {
        print_log(LOG_ERROR, "Transport", "Failed to initialize CURL");
        return -1;
    }
    
    conn->connection_data = curl;
    return 0;
}

int transport_http_send(transport_connection_t *conn, const char *json_data)
{
    if (!conn || !conn->connection_data || !json_data) {
        return -1;
    }
    
    CURL *curl = (CURL *)conn->connection_data;
    struct http_response response = {0};
    
    // Формируем URL
    char url[1024];
    snprintf(url, sizeof(url), "%s://%s:%d%s", 
             conn->config->ssl_enabled ? "https" : "http",
             conn->config->host, 
             conn->config->port,
             conn->config->topic_queue ? conn->config->topic_queue : "/api/signals");
    
    // Настраиваем CURL
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    // Устанавливаем заголовки
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    if (conn->config->username && conn->config->password) {
        curl_easy_setopt(curl, CURLOPT_USERNAME, conn->config->username);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, conn->config->password);
    }
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Выполняем запрос
    CURLcode res = curl_easy_perform(curl);
    
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    curl_slist_free_all(headers);
    
    if (response.data) {
        free(response.data);
    }
    
    if (res != CURLE_OK) {
        print_logf(LOG_ERROR, "Transport", "HTTP request failed: %s", curl_easy_strerror(res));
        return -1;
    }
    
    if (response_code < 200 || response_code >= 300) {
        print_logf(LOG_ERROR, "Transport", "HTTP server returned code: %ld", response_code);
        return -1;
    }
    
    return 0;
}

void transport_http_cleanup(transport_connection_t *conn)
{
    if (conn && conn->connection_data) {
        curl_easy_cleanup((CURL *)conn->connection_data);
        conn->connection_data = NULL;
    }
    curl_global_cleanup();
}

/// Функции для работы с demod_data
demod_data_t *demod_data_create(void)
{
    demod_data_t *data = calloc(1, sizeof(demod_data_t));
    if (!data) {
        return NULL;
    }
    
    pulse_data_clear(&data->pulse_data);
    return data;
}

void demod_data_free(demod_data_t *data)
{
    if (!data) {
        return;
    }
    
    free(data->device_id);
    free(data->raw_data_hex);
    
    if (data->bitbuffer) {
        // TODO: освободить bitbuffer
        free(data->bitbuffer);
    }
    
    free(data);
}

void demod_data_set_pulse_data(demod_data_t *data, const pulse_data_t *pulse_data)
{
    if (!data || !pulse_data) {
        return;
    }
    
    memcpy(&data->pulse_data, pulse_data, sizeof(pulse_data_t));
}

void demod_data_set_bitbuffer(demod_data_t *data, const bitbuffer_t *bitbuffer)
{
    if (!data || !bitbuffer) {
        return;
    }
    
    // TODO: реализовать копирование bitbuffer
    data->bitbuffer = malloc(sizeof(bitbuffer_t));
    if (data->bitbuffer) {
        memcpy(data->bitbuffer, bitbuffer, sizeof(bitbuffer_t));
    }
}

char *demod_data_to_json(const demod_data_t *data)
{
    if (!data) {
        return NULL;
    }
    
    // Простая JSON сериализация (в реальной реализации лучше использовать библиотеку)
    char *json = malloc(4096);
    if (!json) {
        return NULL;
    }
    
    snprintf(json, 4096,
        "{\n"
        "  \"device_id\": \"%s\",\n"
        "  \"timestamp_us\": %lu,\n"
        "  \"frequency\": %u,\n"
        "  \"sample_rate\": %u,\n"
        "  \"rssi_db\": %.2f,\n"
        "  \"snr_db\": %.2f,\n"
        "  \"noise_db\": %.2f,\n"
        "  \"ook_detected\": %d,\n"
        "  \"fsk_detected\": %d,\n"
        "  \"pulse_count\": %u,\n"
        "  \"raw_data_hex\": \"%s\"\n"
        "}",
        data->device_id ? data->device_id : "unknown",
        data->timestamp_us,
        data->frequency,
        data->sample_rate,
        data->rssi_db,
        data->snr_db,
        data->noise_db,
        data->ook_detected,
        data->fsk_detected,
        data->pulse_data.num_pulses,
        data->raw_data_hex ? data->raw_data_hex : ""
    );
    
    return json;
}

char *demod_data_batch_to_json(const demod_data_t *data_array, int count)
{
    if (!data_array || count <= 0) {
        return NULL;
    }
    
    // Простая JSON сериализация батча
    char *json = malloc(count * 4096 + 1024);
    if (!json) {
        return NULL;
    }
    
    strcpy(json, "{\n  \"batch\": [\n");
    
    for (int i = 0; i < count; i++) {
        char *item_json = demod_data_to_json(&data_array[i]);
        if (item_json) {
            strcat(json, "    ");
            strcat(json, item_json);
            if (i < count - 1) {
                strcat(json, ",");
            }
            strcat(json, "\n");
            free(item_json);
        }
    }
    
    strcat(json, "  ]\n}");
    
    return json;
}

// Заглушки для других транспортов (MQTT, RabbitMQ, TCP/UDP)
// В реальной реализации здесь должен быть полный код

#ifdef ENABLE_MQTT
int transport_mqtt_init(transport_connection_t *conn)
{
    if (!conn || !conn->config) {
        print_log(LOG_ERROR, "Transport", "Invalid connection or config for MQTT");
        return -1;
    }
    
    // Create MQTT client ID
    char client_id[128];
    snprintf(client_id, sizeof(client_id), "rtl_433_client_%ld", (long)time(NULL));
    
    // Create MQTT client
    MQTTClient client;
    char broker_uri[256];
    snprintf(broker_uri, sizeof(broker_uri), "tcp://%s:%d", 
             conn->config->host, conn->config->port);
    
    int rc = MQTTClient_create(&client, broker_uri, client_id, 
                               MQTTCLIENT_PERSISTENCE_NONE, NULL);
    
    if (rc != MQTTCLIENT_SUCCESS) {
        print_logf(LOG_ERROR, "Transport", "Failed to create MQTT client: %d", rc);
        return -1;
    }
    
    conn->connection_data = client;
    
    print_logf(LOG_INFO, "Transport", "MQTT client initialized for %s", broker_uri);
    return 0;
}

int transport_mqtt_connect(transport_connection_t *conn)
{
    if (!conn || !conn->connection_data || !conn->config) {
        return -1;
    }
    
    MQTTClient client = (MQTTClient)conn->connection_data;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    
    // Configure connection options
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    
    // Set credentials if provided
    if (conn->config->username && conn->config->password) {
        conn_opts.username = conn->config->username;
        conn_opts.password = conn->config->password;
    }
    
    // Set SSL if enabled
    if (conn->config->ssl_enabled) {
        MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
        conn_opts.ssl = &ssl_opts;
    }
    
    int rc = MQTTClient_connect(client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        print_logf(LOG_ERROR, "Transport", "Failed to connect to MQTT broker: %d", rc);
        return -1;
    }
    
    conn->connected = 1;
    print_logf(LOG_INFO, "Transport", "Connected to MQTT broker %s:%d", 
               conn->config->host, conn->config->port);
    return 0;
}

int transport_mqtt_send(transport_connection_t *conn, const char *json_data)
{
    if (!conn || !conn->connection_data || !json_data || !conn->connected) {
        return -1;
    }
    
    MQTTClient client = (MQTTClient)conn->connection_data;
    
    // Use topic from config or default
    const char *topic = conn->config->topic_queue ? conn->config->topic_queue : "rtl_433/signals";
    
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = (void*)json_data;
    pubmsg.payloadlen = strlen(json_data);
    pubmsg.qos = 1;  // QoS 1 - at least once delivery
    pubmsg.retained = 0;
    
    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    
    if (rc != MQTTCLIENT_SUCCESS) {
        print_logf(LOG_ERROR, "Transport", "Failed to publish MQTT message: %d", rc);
        return -1;
    }
    
    // Wait for message delivery (optional, with timeout)
    rc = MQTTClient_waitForCompletion(client, token, 5000); // 5 second timeout
    if (rc != MQTTCLIENT_SUCCESS) {
        print_logf(LOG_WARNING, "Transport", "MQTT message delivery timeout: %d", rc);
    }
    
    print_logf(LOG_DEBUG, "Transport", "MQTT message sent to topic: %s", topic);
    return 0;
}

void transport_mqtt_cleanup(transport_connection_t *conn)
{
    if (!conn || !conn->connection_data) {
        return;
    }
    
    MQTTClient client = (MQTTClient)conn->connection_data;
    
    if (conn->connected) {
        MQTTClient_disconnect(client, 10000);  // 10 second timeout
        conn->connected = 0;
    }
    
    MQTTClient_destroy(&client);
    conn->connection_data = NULL;
    
    print_log(LOG_INFO, "Transport", "MQTT client cleaned up");
}
#endif

#ifdef ENABLE_RABBITMQ

// RabbitMQ connection data structure
typedef struct {
    amqp_connection_state_t conn;
    amqp_socket_t *socket;
    int channel;
    char *exchange;
    char *queue;
    char *routing_key;
} rabbitmq_data_t;

int transport_rabbitmq_init(transport_connection_t *conn)
{
    if (!conn || !conn->config) {
        print_log(LOG_ERROR, "Transport", "Invalid connection or config for RabbitMQ");
        return -1;
    }
    
    // Allocate RabbitMQ data structure
    rabbitmq_data_t *rmq = calloc(1, sizeof(rabbitmq_data_t));
    if (!rmq) {
        print_log(LOG_ERROR, "Transport", "Failed to allocate RabbitMQ data");
        return -1;
    }
    
    // Create connection
    rmq->conn = amqp_new_connection();
    if (!rmq->conn) {
        print_log(LOG_ERROR, "Transport", "Failed to create RabbitMQ connection");
        free(rmq);
        return -1;
    }
    
    // Create socket
    rmq->socket = amqp_tcp_socket_new(rmq->conn);
    if (!rmq->socket) {
        print_log(LOG_ERROR, "Transport", "Failed to create RabbitMQ socket");
        amqp_destroy_connection(rmq->conn);
        free(rmq);
        return -1;
    }
    
    // Set defaults
    rmq->channel = 1;
    rmq->exchange = strdup(conn->config->topic_queue ? conn->config->topic_queue : "rtl_433");
    rmq->queue = strdup("signals");
    rmq->routing_key = strdup("signals");
    
    conn->connection_data = rmq;
    
    print_logf(LOG_INFO, "Transport", "RabbitMQ transport initialized for %s:%d", 
               conn->config->host, conn->config->port);
    return 0;
}

int transport_rabbitmq_connect(transport_connection_t *conn)
{
    if (!conn || !conn->connection_data || !conn->config) {
        return -1;
    }
    
    rabbitmq_data_t *rmq = (rabbitmq_data_t *)conn->connection_data;
    
    // Connect to RabbitMQ server
    int status = amqp_socket_open(rmq->socket, conn->config->host, conn->config->port);
    if (status) {
        print_logf(LOG_ERROR, "Transport", "Failed to connect to RabbitMQ: %s:%d", 
                   conn->config->host, conn->config->port);
        return -1;
    }
    
    // Login
    const char *username = conn->config->username ? conn->config->username : "guest";
    const char *password = conn->config->password ? conn->config->password : "guest";
    
    amqp_rpc_reply_t reply = amqp_login(rmq->conn, "/", 0, 131072, 0, 
                                        AMQP_SASL_METHOD_PLAIN, 
                                        username, password);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        print_logf(LOG_ERROR, "Transport", "RabbitMQ login failed for user: %s", username);
        return -1;
    }
    
    // Open channel
    amqp_channel_open(rmq->conn, rmq->channel);
    reply = amqp_get_rpc_reply(rmq->conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        print_log(LOG_ERROR, "Transport", "Failed to open RabbitMQ channel");
        return -1;
    }
    
    // Declare exchange
    amqp_exchange_declare(rmq->conn, rmq->channel,
                         amqp_cstring_bytes(rmq->exchange),
                         amqp_cstring_bytes("direct"),
                         0, 1, 0, 0, amqp_empty_table);
    reply = amqp_get_rpc_reply(rmq->conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        print_logf(LOG_WARNING, "Transport", "Failed to declare exchange: %s", rmq->exchange);
        // Continue anyway, exchange might already exist
    }
    
    // Declare queue
    amqp_queue_declare_ok_t *queue_ok = amqp_queue_declare(rmq->conn, rmq->channel,
                                                          amqp_cstring_bytes(rmq->queue),
                                                          0, 1, 0, 0, amqp_empty_table);
    reply = amqp_get_rpc_reply(rmq->conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        print_logf(LOG_WARNING, "Transport", "Failed to declare queue: %s", rmq->queue);
        // Continue anyway, queue might already exist
    }
    
    // Bind queue to exchange
    amqp_queue_bind(rmq->conn, rmq->channel,
                   amqp_cstring_bytes(rmq->queue),
                   amqp_cstring_bytes(rmq->exchange),
                   amqp_cstring_bytes(rmq->routing_key),
                   amqp_empty_table);
    reply = amqp_get_rpc_reply(rmq->conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        print_log(LOG_WARNING, "Transport", "Failed to bind queue to exchange");
        // Continue anyway
    }
    
    conn->connected = 1;
    print_logf(LOG_INFO, "Transport", "Connected to RabbitMQ broker %s:%d, exchange: %s, queue: %s", 
               conn->config->host, conn->config->port, rmq->exchange, rmq->queue);
    return 0;
}

int transport_rabbitmq_send(transport_connection_t *conn, const char *json_data)
{
    if (!conn || !conn->connection_data || !json_data || !conn->connected) {
        return -1;
    }
    
    rabbitmq_data_t *rmq = (rabbitmq_data_t *)conn->connection_data;
    
    // Create message properties
    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
    props.content_type = amqp_cstring_bytes("application/json");
    props.delivery_mode = 2; // Persistent message
    
    // Publish message
    int result = amqp_basic_publish(rmq->conn, rmq->channel,
                                   amqp_cstring_bytes(rmq->exchange),
                                   amqp_cstring_bytes(rmq->routing_key),
                                   0, 0, &props,
                                   amqp_cstring_bytes(json_data));
    
    if (result != AMQP_STATUS_OK) {
        print_logf(LOG_ERROR, "Transport", "Failed to publish RabbitMQ message: %d", result);
        return -1;
    }
    
    print_logf(LOG_DEBUG, "Transport", "RabbitMQ message sent to exchange: %s, routing_key: %s", 
               rmq->exchange, rmq->routing_key);
    return 0;
}

void transport_rabbitmq_cleanup(transport_connection_t *conn)
{
    if (!conn || !conn->connection_data) {
        return;
    }
    
    rabbitmq_data_t *rmq = (rabbitmq_data_t *)conn->connection_data;
    
    if (conn->connected && rmq->conn) {
        // Close channel
        amqp_channel_close(rmq->conn, rmq->channel, AMQP_REPLY_SUCCESS);
        
        // Close connection
        amqp_connection_close(rmq->conn, AMQP_REPLY_SUCCESS);
        conn->connected = 0;
    }
    
    if (rmq->conn) {
        amqp_destroy_connection(rmq->conn);
    }
    
    // Free allocated strings
    free(rmq->exchange);
    free(rmq->queue);
    free(rmq->routing_key);
    free(rmq);
    
    conn->connection_data = NULL;
    print_log(LOG_INFO, "Transport", "RabbitMQ transport cleaned up");
}
#endif

int transport_socket_init(transport_connection_t *conn)
{
    // TODO: реализовать TCP/UDP сокеты
    print_log(LOG_WARNING, "Transport", "Socket transport not implemented");
    return -1;
}

int transport_socket_connect(transport_connection_t *conn)
{
    return -1;
}

int transport_socket_send(transport_connection_t *conn, const char *json_data)
{
    return -1;
}

void transport_socket_cleanup(transport_connection_t *conn)
{
    // TODO: реализовать очистку сокетов
}



