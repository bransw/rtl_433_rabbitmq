# CMakeLists.txt для rtl_433_server
cmake_minimum_required(VERSION 3.10)
project(rtl_433_server)

set(CMAKE_C_STANDARD 99)

# Поиск необходимых библиотек
find_package(PkgConfig REQUIRED)

# Поиск libcurl для HTTP сервера
find_package(CURL REQUIRED)

# Поиск paho-mqtt для MQTT транспорта
find_path(PAHO_MQTT_INCLUDE_DIR MQTTClient.h)
find_library(PAHO_MQTT_LIBRARY paho-mqtt3c)

# Добавляем опцию для RabbitMQ
option(ENABLE_RABBITMQ "Enable RabbitMQ transport" OFF)
if(ENABLE_RABBITMQ)
    find_path(RABBITMQ_INCLUDE_DIR amqp.h)
    find_library(RABBITMQ_LIBRARY rabbitmq)
    if(RABBITMQ_INCLUDE_DIR AND RABBITMQ_LIBRARY)
        add_definitions(-DENABLE_RABBITMQ)
    endif()
endif()

# Поиск JSON-C для парсинга
pkg_check_modules(JSON_C json-c)
if(NOT JSON_C_FOUND)
    message(FATAL_ERROR "json-c not found")
endif()

# Поиск SQLite для хранения неизвестных сигналов
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 sqlite3)
if(NOT SQLITE3_FOUND)
    message(FATAL_ERROR "sqlite3 not found")
endif()

# Включаем заголовочные файлы из основного проекта
include_directories(../include)
include_directories(.)

# Исходные файлы для сервера
set(SERVER_SOURCES
    src/rtl_433_server.c
    src/server_transport.c
    src/server_config.c
    src/device_decoder.c
    src/queue_manager.c
    src/signal_processor.c
    
    # Копируем необходимые файлы из основного проекта
    ../src/pulse_data.c
    ../src/pulse_slicer.c
    ../src/bitbuffer.c
    ../src/bit_util.c
    ../src/data.c
    ../src/decoder_util.c
    ../src/r_util.c
    ../src/compat_time.c
    ../src/compat_paths.c
    ../src/logger.c
    ../src/list.c
    ../src/jsmn.c
    ../src/r_api.c
    ../src/confparse.c
    ../src/optparse.c
)

# Добавляем все декодеры устройств
file(GLOB DEVICE_SOURCES "../src/devices/*.c")
list(APPEND SERVER_SOURCES ${DEVICE_SOURCES})

# Добавляем getopt для Windows
if(WIN32)
    list(APPEND SERVER_SOURCES ../src/getopt/getopt.c)
endif()

# Создаем исполняемый файл сервера
add_executable(rtl_433_server ${SERVER_SOURCES})

# Линкуем библиотеки
target_link_libraries(rtl_433_server 
    ${JSON_C_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${CURL_LIBRARIES}
    m
    pthread
)

if(PAHO_MQTT_INCLUDE_DIR AND PAHO_MQTT_LIBRARY)
    target_include_directories(rtl_433_server PRIVATE ${PAHO_MQTT_INCLUDE_DIR})
    target_link_libraries(rtl_433_server ${PAHO_MQTT_LIBRARY})
    add_definitions(-DENABLE_MQTT)
endif()

if(ENABLE_RABBITMQ AND RABBITMQ_INCLUDE_DIR AND RABBITMQ_LIBRARY)
    target_include_directories(rtl_433_server PRIVATE ${RABBITMQ_INCLUDE_DIR})
    target_link_libraries(rtl_433_server ${RABBITMQ_LIBRARY})
endif()

# Флаги компиляции
target_compile_options(rtl_433_server PRIVATE 
    ${JSON_C_CFLAGS_OTHER}
    ${SQLITE3_CFLAGS_OTHER}
)

# Включаем директории заголовков
target_include_directories(rtl_433_server PRIVATE 
    ${JSON_C_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

# Устанавливаем исполняемый файл
install(TARGETS rtl_433_server DESTINATION bin)

# Устанавливаем конфигурационные файлы
install(FILES 
    config/server.conf.example
    config/devices.conf
    DESTINATION etc/rtl_433_server
)

