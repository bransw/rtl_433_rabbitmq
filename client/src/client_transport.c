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
    // TODO: реализовать MQTT
    print_log(LOG_WARNING, "Transport", "MQTT transport not implemented");
    return -1;
}

int transport_mqtt_connect(transport_connection_t *conn)
{
    return -1;
}

int transport_mqtt_send(transport_connection_t *conn, const char *json_data)
{
    return -1;
}

void transport_mqtt_cleanup(transport_connection_t *conn)
{
    // TODO: реализовать очистку MQTT
}
#endif

#ifdef ENABLE_RABBITMQ
int transport_rabbitmq_init(transport_connection_t *conn)
{
    // TODO: реализовать RabbitMQ
    print_log(LOG_WARNING, "Transport", "RabbitMQ transport not implemented");
    return -1;
}

int transport_rabbitmq_connect(transport_connection_t *conn)
{
    return -1;
}

int transport_rabbitmq_send(transport_connection_t *conn, const char *json_data)
{
    return -1;
}

void transport_rabbitmq_cleanup(transport_connection_t *conn)
{
    // TODO: реализовать очистку RabbitMQ
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



