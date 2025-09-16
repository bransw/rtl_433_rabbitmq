/** @file
 * Implementation of the common transport module for rtl_433
 */

#include "rtl433_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <json-c/json.h>

#ifdef ENABLE_RABBITMQ
#include <amqp.h>
#include <amqp_tcp_socket.h>
#endif

/// Global transport statistics
static rtl433_transport_stats_t g_transport_stats = {0};

/// Unique ID generator
static uint64_t g_next_package_id = 1;

// === INITIALIZATION AND CONFIGURATION ===

int rtl433_transport_config_init(rtl433_transport_config_t *config)
{
    if (!config) return -1;
    
    memset(config, 0, sizeof(rtl433_transport_config_t));
    
    // Дефолтные значения
    config->type = RTL433_TRANSPORT_RABBITMQ;
    config->host = strdup("localhost");
    config->port = 5672;
    config->username = strdup("guest");
    config->password = strdup("guest");
    config->exchange = strdup("rtl_433");
    config->queue = strdup("signals");
    config->ssl_enabled = false;
    config->timeout_ms = 5000;
    config->reconnect_interval_s = 5;
    
    return 0;
}

void rtl433_transport_config_free(rtl433_transport_config_t *config)
{
    if (!config) return;
    
    free(config->host);
    free(config->username);
    free(config->password);
    free(config->exchange);
    free(config->queue);
    free(config->topic);
    
    memset(config, 0, sizeof(rtl433_transport_config_t));
}

int rtl433_transport_parse_url(const char *url, rtl433_transport_config_t *config)
{
    if (!url || !config) return -1;
    
    // Парсинг URL вида: amqp://user:pass@host:port/queue
    char *url_copy = strdup(url);
    char *saveptr;
    
    // Определение типа транспорта
    if (strncmp(url, "amqp://", 7) == 0) {
        config->type = RTL433_TRANSPORT_RABBITMQ;
        char *rest = url_copy + 7;  // Пропускаем "amqp://"
        
        // Парсинг user:pass@host:port/queue
        char *at_pos = strchr(rest, '@');
        if (at_pos) {
            *at_pos = '\0';
            
            // Парсинг user:pass
            char *colon_pos = strchr(rest, ':');
            if (colon_pos) {
                *colon_pos = '\0';
                free(config->username);
                config->username = strdup(rest);
                free(config->password);
                config->password = strdup(colon_pos + 1);
            } else {
                free(config->username);
                config->username = strdup(rest);
            }
            rest = at_pos + 1;
        }
        
        // Парсинг host:port/queue
        char *slash_pos = strchr(rest, '/');
        if (slash_pos) {
            *slash_pos = '\0';
            free(config->queue);
            config->queue = strdup(slash_pos + 1);
        }
        
        // Парсинг host:port
        char *colon_pos = strchr(rest, ':');
        if (colon_pos) {
            *colon_pos = '\0';
            config->port = atoi(colon_pos + 1);
        }
        
        free(config->host);
        config->host = strdup(rest);
        
    } else if (strncmp(url, "mqtt://", 7) == 0) {
        config->type = RTL433_TRANSPORT_MQTT;
        // TODO: Реализовать парсинг MQTT URL
    } else {
        free(url_copy);
        return -1;
    }
    
    free(url_copy);
    return 0;
}

// === CONNECTION AND CONNECTION MANAGEMENT ===

#ifdef ENABLE_RABBITMQ
typedef struct {
    amqp_connection_state_t conn;
    amqp_socket_t *socket;
    amqp_channel_t channel;
} rabbitmq_connection_data_t;

static int rtl433_transport_rabbitmq_connect(rtl433_transport_connection_t *conn)
{
    rabbitmq_connection_data_t *rabbitmq = calloc(1, sizeof(rabbitmq_connection_data_t));
    if (!rabbitmq) return -1;
    
    rabbitmq->conn = amqp_new_connection();
    rabbitmq->socket = amqp_tcp_socket_new(rabbitmq->conn);
    rabbitmq->channel = 1;
    
    if (!rabbitmq->socket) {
        amqp_destroy_connection(rabbitmq->conn);
        free(rabbitmq);
        return -1;
    }
    
    // Подключение к серверу
    int status = amqp_socket_open(rabbitmq->socket, conn->config->host, conn->config->port);
    if (status != AMQP_STATUS_OK) {
        amqp_destroy_connection(rabbitmq->conn);
        free(rabbitmq);
        return -1;
    }
    
    // Логин
    amqp_rpc_reply_t reply = amqp_login(rabbitmq->conn, "/", 0, 131072, 0,
                                       AMQP_SASL_METHOD_PLAIN,
                                       conn->config->username,
                                       conn->config->password);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        amqp_connection_close(rabbitmq->conn, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(rabbitmq->conn);
        free(rabbitmq);
        return -1;
    }
    
    // Открытие канала
    amqp_channel_open(rabbitmq->conn, rabbitmq->channel);
    reply = amqp_get_rpc_reply(rabbitmq->conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        amqp_connection_close(rabbitmq->conn, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(rabbitmq->conn);
        free(rabbitmq);
        return -1;
    }
    
    // Объявление exchange
    amqp_exchange_declare(rabbitmq->conn, rabbitmq->channel,
                         amqp_cstring_bytes(conn->config->exchange),
                         amqp_cstring_bytes("direct"),
                         0, 1, 0, 0, amqp_empty_table);
    
    // Проверка результата объявления exchange
    reply = amqp_get_rpc_reply(rabbitmq->conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        amqp_connection_close(rabbitmq->conn, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(rabbitmq->conn);
        free(rabbitmq);
        return -1;
    }
    
    // Note: Queues are now created on-demand by rtl433_transport_send_message_to_queue()
    
    conn->connection_data = rabbitmq;
    conn->connected = true;
    
    return 0;
}

static void rtl433_transport_rabbitmq_disconnect(rtl433_transport_connection_t *conn)
{
    if (!conn->connection_data) return;
    
    rabbitmq_connection_data_t *rabbitmq = (rabbitmq_connection_data_t*)conn->connection_data;
    
    amqp_channel_close(rabbitmq->conn, rabbitmq->channel, AMQP_REPLY_SUCCESS);
    amqp_connection_close(rabbitmq->conn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(rabbitmq->conn);
    
    free(rabbitmq);
    conn->connection_data = NULL;
    conn->connected = false;
}
#endif

int rtl433_transport_connect(rtl433_transport_connection_t *conn, rtl433_transport_config_t *config)
{
    if (!conn || !config) return -1;
    
    conn->config = config;
    conn->connected = false;
    conn->last_error = 0;
    
    switch (config->type) {
#ifdef ENABLE_RABBITMQ
        case RTL433_TRANSPORT_RABBITMQ:
            return rtl433_transport_rabbitmq_connect(conn);
#endif
        case RTL433_TRANSPORT_MQTT:
            // TODO: Реализовать MQTT
            return -1;
        case RTL433_TRANSPORT_TCP:
        case RTL433_TRANSPORT_UDP:
            // TODO: Реализовать TCP/UDP
            return -1;
        default:
            return -1;
    }
}

void rtl433_transport_disconnect(rtl433_transport_connection_t *conn)
{
    if (!conn) return;
    
    switch (conn->config->type) {
#ifdef ENABLE_RABBITMQ
        case RTL433_TRANSPORT_RABBITMQ:
            rtl433_transport_rabbitmq_disconnect(conn);
            break;
#endif
        default:
            break;
    }
}

bool rtl433_transport_is_connected(rtl433_transport_connection_t *conn)
{
    return conn && conn->connected;
}

// === MESSAGE HANDLING ===

rtl433_message_t* rtl433_message_create_from_pulse_data(pulse_data_t *pulse_data, 
                                                       const char *modulation,
                                                       uint64_t package_id)
{
    if (!pulse_data || !modulation) return NULL;
    
    rtl433_message_t *msg = calloc(1, sizeof(rtl433_message_t));
    if (!msg) return NULL;
    
    msg->package_id = package_id;
    msg->modulation = strdup(modulation);
    msg->timestamp = (uint32_t)time(NULL);
    
    // Копируем pulse_data
    msg->pulse_data = calloc(1, sizeof(pulse_data_t));
    if (!msg->pulse_data) {
        free(msg->modulation);
        free(msg);
        return NULL;
    }
    
    memcpy(msg->pulse_data, pulse_data, sizeof(pulse_data_t));
    
    return msg;
}

char* rtl433_message_to_json(rtl433_message_t *message)
{
    if (!message || !message->pulse_data) return NULL;
    
    json_object *root = json_object_new_object();
    
    // Базовые поля
    json_object_object_add(root, "package_id", json_object_new_int64(message->package_id));
    json_object_object_add(root, "mod", json_object_new_string(message->modulation));
    json_object_object_add(root, "timestamp", json_object_new_int(message->timestamp));
    
    // Pulse data
    pulse_data_t *pd = message->pulse_data;
    json_object_object_add(root, "count", json_object_new_int(pd->num_pulses));
    json_object_object_add(root, "freq_Hz", json_object_new_int64((int64_t)pd->centerfreq_hz));
    json_object_object_add(root, "rate_Hz", json_object_new_int(pd->sample_rate));
    json_object_object_add(root, "rssi_dB", json_object_new_double(pd->rssi_db));
    json_object_object_add(root, "snr_dB", json_object_new_double(pd->snr_db));
    json_object_object_add(root, "noise_dB", json_object_new_double(pd->noise_db));
    
    if (pd->freq1_hz != 0) {
        json_object_object_add(root, "freq1_Hz", json_object_new_double(pd->freq1_hz));
    }
    if (pd->freq2_hz != 0) {
        json_object_object_add(root, "freq2_Hz", json_object_new_double(pd->freq2_hz));
    }
    
    // Массив импульсов
    json_object *pulses_array = json_object_new_array();
    for (unsigned i = 0; i < pd->num_pulses; i++) {
        json_object_array_add(pulses_array, json_object_new_int(pd->pulse[i]));
        json_object_array_add(pulses_array, json_object_new_int(pd->gap[i]));
    }
    json_object_object_add(root, "pulses", pulses_array);
    
    // Hex string если есть
    if (message->hex_string) {
        json_object_object_add(root, "hex_string", json_object_new_string(message->hex_string));
    }
    
    const char *json_str = json_object_to_json_string(root);
    char *result = strdup(json_str);
    
    json_object_put(root);
    return result;
}

void rtl433_message_free(rtl433_message_t *message)
{
    if (!message) return;
    
    free(message->modulation);
    free(message->hex_string);
    free(message->pulse_data);
    free(message->metadata);
    free(message);
}

// === SENDING AND RECEIVING ===

#ifdef ENABLE_RABBITMQ
static int rtl433_transport_rabbitmq_send(rtl433_transport_connection_t *conn, rtl433_message_t *message)
{
    if (!conn->connection_data) return -1;
    
    rabbitmq_connection_data_t *rabbitmq = (rabbitmq_connection_data_t*)conn->connection_data;
    
    char *json_str = rtl433_message_to_json(message);
    if (!json_str) return -1;
    
    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
    props.content_type = amqp_cstring_bytes("application/json");
    props.delivery_mode = 2; // persistent
    
    int result = amqp_basic_publish(rabbitmq->conn, rabbitmq->channel,
                                   amqp_cstring_bytes(conn->config->exchange),
                                   amqp_cstring_bytes(conn->config->queue),
                                   0, 0, &props,
                                   amqp_cstring_bytes(json_str));
    
    free(json_str);
    
    if (result == AMQP_STATUS_OK) {
        g_transport_stats.messages_sent++;
        return 0;
    } else {
        g_transport_stats.send_errors++;
        return -1;
    }
}

static int rtl433_transport_rabbitmq_send_to_queue(rtl433_transport_connection_t *conn, rtl433_message_t *message, const char *queue_name)
{
    if (!conn->connection_data || !queue_name) return -1;
    
    rabbitmq_connection_data_t *rabbitmq = (rabbitmq_connection_data_t*)conn->connection_data;
    
    // Ensure the queue exists and is bound
    amqp_queue_declare(rabbitmq->conn, rabbitmq->channel,
                      amqp_cstring_bytes(queue_name),
                      0, 1, 0, 0, amqp_empty_table);
    
    amqp_queue_bind(rabbitmq->conn, rabbitmq->channel,
                   amqp_cstring_bytes(queue_name),
                   amqp_cstring_bytes(conn->config->exchange),
                   amqp_cstring_bytes(queue_name), // routing key = queue name
                   amqp_empty_table);
    
    char *json_str = rtl433_message_to_json(message);
    if (!json_str) return -1;
    
    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
    props.content_type = amqp_cstring_bytes("application/json");
    props.delivery_mode = 2; // persistent
    
    int result = amqp_basic_publish(rabbitmq->conn, rabbitmq->channel,
                                   amqp_cstring_bytes(conn->config->exchange),
                                   amqp_cstring_bytes(queue_name), // Use specified queue name
                                   0, 0, &props,
                                   amqp_cstring_bytes(json_str));
    
    free(json_str);
    
    if (result == AMQP_STATUS_OK) {
        g_transport_stats.messages_sent++;
        return 0;
    } else {
        g_transport_stats.send_errors++;
        return -1;
    }
}
#endif

int rtl433_transport_send_message(rtl433_transport_connection_t *conn, rtl433_message_t *message)
{
    if (!conn || !message || !conn->connected) return -1;
    
    switch (conn->config->type) {
#ifdef ENABLE_RABBITMQ
        case RTL433_TRANSPORT_RABBITMQ:
            return rtl433_transport_rabbitmq_send(conn, message);
#endif
        default:
            return -1;
    }
}

static int rtl433_transport_rabbitmq_send_raw_to_queue(rtl433_transport_connection_t *conn, const char *json_data, const char *queue_name)
{
    if (!conn->connection_data || !queue_name || !json_data) return -1;
    
    rabbitmq_connection_data_t *rabbitmq = (rabbitmq_connection_data_t*)conn->connection_data;
    
    
    // Ensure the queue exists and is bound
    amqp_queue_declare(rabbitmq->conn, rabbitmq->channel,
                      amqp_cstring_bytes(queue_name),
                      0, 1, 0, 0, amqp_empty_table);
    
    // Check queue declaration result
    amqp_rpc_reply_t reply = amqp_get_rpc_reply(rabbitmq->conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        g_transport_stats.send_errors++;
        return -1;
    }
    
    amqp_queue_bind(rabbitmq->conn, rabbitmq->channel,
                   amqp_cstring_bytes(queue_name),
                   amqp_cstring_bytes(conn->config->exchange),
                   amqp_cstring_bytes(queue_name), // routing key = queue name
                   amqp_empty_table);
    
    // Check queue bind result
    reply = amqp_get_rpc_reply(rabbitmq->conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        g_transport_stats.send_errors++;
        return -1;
    }
    
    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
    props.content_type = amqp_cstring_bytes("application/json");
    props.delivery_mode = 2; // persistent
    
    int result = amqp_basic_publish(rabbitmq->conn, rabbitmq->channel,
                                   amqp_cstring_bytes(conn->config->exchange),
                                   amqp_cstring_bytes(queue_name), // Use specified queue name
                                   0, 0, &props,
                                   amqp_cstring_bytes(json_data));
    
    if (result != AMQP_STATUS_OK) {
        g_transport_stats.send_errors++;
        return -1;
    }
    
    g_transport_stats.messages_sent++;
    return 0;
}

int rtl433_transport_send_message_to_queue(rtl433_transport_connection_t *conn, rtl433_message_t *message, const char *queue_name)
{
    if (!conn || !message || !conn->connected || !queue_name) return -1;
    
    switch (conn->config->type) {
#ifdef ENABLE_RABBITMQ
        case RTL433_TRANSPORT_RABBITMQ:
            return rtl433_transport_rabbitmq_send_to_queue(conn, message, queue_name);
#endif
        default:
            return -1;
    }
}

int rtl433_transport_send_raw_json_to_queue(rtl433_transport_connection_t *conn, const char *json_data, const char *queue_name)
{
    if (!conn || !json_data || !conn->connected || !queue_name) return -1;
    
    switch (conn->config->type) {
#ifdef ENABLE_RABBITMQ
        case RTL433_TRANSPORT_RABBITMQ:
            return rtl433_transport_rabbitmq_send_raw_to_queue(conn, json_data, queue_name);
#endif
        default:
            return -1;
    }
}

// === MESSAGE RECEPTION ===

#ifdef ENABLE_RABBITMQ
static int rtl433_transport_rabbitmq_receive(rtl433_transport_connection_t *conn, 
                                             rtl433_message_handler_t handler, 
                                             void *user_data, 
                                             int timeout_ms)
{
    if (!conn->connection_data) return -1;
    
    rabbitmq_connection_data_t *rabbitmq = (rabbitmq_connection_data_t*)conn->connection_data;
    
    // Установка таймаута
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    // Получение сообщения
    amqp_envelope_t envelope;
    amqp_rpc_reply_t reply = amqp_consume_message(rabbitmq->conn, &envelope, &timeout, 0);
    
    if (reply.reply_type == AMQP_RESPONSE_NORMAL) {
        // Парсинг сообщения
        char *message_body = malloc(envelope.message.body.len + 1);
        if (message_body) {
            memcpy(message_body, envelope.message.body.bytes, envelope.message.body.len);
            message_body[envelope.message.body.len] = '\0';
            
            // Создание rtl433_message_t из JSON
            rtl433_message_t *message = rtl433_message_create_from_json(message_body);
            if (message) {
                // Вызов обработчика
                handler(message, user_data);
                rtl433_message_free(message);
                g_transport_stats.messages_received++;
            }
            
            free(message_body);
        }
        
        // Подтверждение сообщения
        amqp_basic_ack(rabbitmq->conn, rabbitmq->channel, envelope.delivery_tag, 0);
        amqp_destroy_envelope(&envelope);
        
        return 1; // Получено 1 сообщение
    } else if (reply.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION && 
               reply.library_error == AMQP_STATUS_TIMEOUT) {
        return 0; // Таймаут
    } else {
        g_transport_stats.receive_errors++;
        return -1; // Ошибка
    }
}
#endif

int rtl433_transport_receive_messages(rtl433_transport_connection_t *conn, 
                                     rtl433_message_handler_t handler, 
                                     void *user_data, 
                                     int timeout_ms)
{
    if (!conn || !handler || !conn->connected) return -1;
    
    switch (conn->config->type) {
#ifdef ENABLE_RABBITMQ
        case RTL433_TRANSPORT_RABBITMQ:
            return rtl433_transport_rabbitmq_receive(conn, handler, user_data, timeout_ms);
#endif
        default:
            return -1;
    }
}

rtl433_message_t* rtl433_message_create_from_json(const char *json_str)
{
    if (!json_str) return NULL;
    
    // TODO: Полная реализация парсинга JSON -> rtl433_message_t
    // Пока создаем заглушку
    rtl433_message_t *msg = calloc(1, sizeof(rtl433_message_t));
    if (!msg) return NULL;
    
    msg->package_id = rtl433_generate_package_id();
    msg->modulation = strdup("FSK");
    msg->timestamp = (uint32_t)time(NULL);
    
    // Создаем пустой pulse_data для тестирования
    msg->pulse_data = calloc(1, sizeof(pulse_data_t));
    if (!msg->pulse_data) {
        rtl433_message_free(msg);
        return NULL;
    }
    
    // TODO: Использовать rtl433_signal_reconstruct_from_json для заполнения pulse_data
    
    return msg;
}

// === UTILITIES ===

uint64_t rtl433_generate_package_id(void)
{
    return __sync_fetch_and_add(&g_next_package_id, 1);
}

const char* rtl433_transport_type_to_string(rtl433_transport_type_t type)
{
    switch (type) {
        case RTL433_TRANSPORT_RABBITMQ: return "RabbitMQ";
        case RTL433_TRANSPORT_MQTT: return "MQTT";
        case RTL433_TRANSPORT_TCP: return "TCP";
        case RTL433_TRANSPORT_UDP: return "UDP";
        default: return "Unknown";
    }
}

rtl433_transport_type_t rtl433_transport_type_from_string(const char *type_str)
{
    if (!type_str) return RTL433_TRANSPORT_RABBITMQ;
    
    if (strcasecmp(type_str, "rabbitmq") == 0 || strcasecmp(type_str, "amqp") == 0) {
        return RTL433_TRANSPORT_RABBITMQ;
    } else if (strcasecmp(type_str, "mqtt") == 0) {
        return RTL433_TRANSPORT_MQTT;
    } else if (strcasecmp(type_str, "tcp") == 0) {
        return RTL433_TRANSPORT_TCP;
    } else if (strcasecmp(type_str, "udp") == 0) {
        return RTL433_TRANSPORT_UDP;
    }
    
    return RTL433_TRANSPORT_RABBITMQ;
}

void rtl433_transport_get_stats(rtl433_transport_connection_t *conn, rtl433_transport_stats_t *stats)
{
    if (!stats) return;
    *stats = g_transport_stats;
}

void rtl433_transport_reset_stats(rtl433_transport_connection_t *conn)
{
    memset(&g_transport_stats, 0, sizeof(g_transport_stats));
}
