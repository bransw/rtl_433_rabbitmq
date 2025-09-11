# CMakeLists.txt for rtl_433_client
cmake_minimum_required(VERSION 3.10)
project(rtl_433_client)

set(CMAKE_C_STANDARD 99)

# Find required libraries
find_package(PkgConfig REQUIRED)

# Find libcurl for HTTP transport
find_package(CURL REQUIRED)

# Find RTL-SDR
pkg_check_modules(LIBRTLSDR librtlsdr)
if(NOT LIBRTLSDR_FOUND)
    message(FATAL_ERROR "librtlsdr not found")
endif()

# Find paho-mqtt for MQTT transport
find_path(PAHO_MQTT_INCLUDE_DIR MQTTClient.h)
find_library(PAHO_MQTT_LIBRARY paho-mqtt3c)

# Add RabbitMQ option
option(ENABLE_RABBITMQ "Enable RabbitMQ transport" OFF)
if(ENABLE_RABBITMQ)
    find_path(RABBITMQ_INCLUDE_DIR amqp.h)
    find_library(RABBITMQ_LIBRARY rabbitmq)
    if(RABBITMQ_INCLUDE_DIR AND RABBITMQ_LIBRARY)
        add_definitions(-DENABLE_RABBITMQ)
    endif()
endif()

# Find JSON-C for serialization
pkg_check_modules(JSON_C json-c)
if(NOT JSON_C_FOUND)
    message(FATAL_ERROR "json-c not found")
endif()

# Include header files from main project
include_directories(../include)
include_directories(.)

# Source files for client
set(CLIENT_SOURCES
    src/rtl_433_client.c
    src/client_transport.c
    src/client_config.c
    
    # Copy necessary files from main project
    ../src/sdr.c
    ../src/baseband.c
    ../src/pulse_detect.c
    ../src/pulse_detect_fsk.c
    ../src/pulse_data.c
    ../src/pulse_slicer.c
    ../src/abuf.c
    ../src/bitbuffer.c
    ../src/bit_util.c
    ../src/data.c
    ../src/decoder_util.c
    ../src/r_util.c
    ../src/compat_time.c
    ../src/compat_paths.c
    ../src/logger.c
    ../src/fileformat.c
    ../src/am_analyze.c
    ../src/confparse.c
    ../src/optparse.c
    ../src/list.c
    ../src/jsmn.c
)

# Add getopt for Windows
if(WIN32)
    list(APPEND CLIENT_SOURCES ../src/getopt/getopt.c)
endif()

# Create client executable
add_executable(rtl_433_client ${CLIENT_SOURCES})

# Link libraries
target_link_libraries(rtl_433_client 
    ${LIBRTLSDR_LIBRARIES}
    ${JSON_C_LIBRARIES}
    ${CURL_LIBRARIES}
    m
)

if(PAHO_MQTT_INCLUDE_DIR AND PAHO_MQTT_LIBRARY)
    target_include_directories(rtl_433_client PRIVATE ${PAHO_MQTT_INCLUDE_DIR})
    target_link_libraries(rtl_433_client ${PAHO_MQTT_LIBRARY})
    add_definitions(-DENABLE_MQTT)
endif()

if(ENABLE_RABBITMQ AND RABBITMQ_INCLUDE_DIR AND RABBITMQ_LIBRARY)
    target_include_directories(rtl_433_client PRIVATE ${RABBITMQ_INCLUDE_DIR})
    target_link_libraries(rtl_433_client ${RABBITMQ_LIBRARY})
endif()

# Compilation flags
target_compile_options(rtl_433_client PRIVATE 
    ${LIBRTLSDR_CFLAGS_OTHER}
    ${JSON_C_CFLAGS_OTHER}
)

# Include header directories
target_include_directories(rtl_433_client PRIVATE 
    ${LIBRTLSDR_INCLUDE_DIRS}
    ${JSON_C_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

# Install executable
install(TARGETS rtl_433_client DESTINATION bin)
