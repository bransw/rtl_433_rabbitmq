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

# Find libusb
pkg_check_modules(LIBUSB libusb-1.0)
if(NOT LIBUSB_FOUND)
    message(FATAL_ERROR "libusb-1.0 not found")
endif()

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Find threads
find_package(Threads REQUIRED)

# Find SoapySDR
pkg_check_modules(SOAPYSDR SoapySDR)
if(NOT SOAPYSDR_FOUND)
    message(WARNING "SoapySDR not found - some SDR features may not work")
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

# Add shared library support
add_subdirectory(../shared shared_build)
include_directories(${RTL433_SHARED_INCLUDE_DIR})

# Source files for client
set(CLIENT_SOURCES
    src/rtl_433_client_new.c
)

# Add getopt for Windows
if(WIN32)
    list(APPEND CLIENT_SOURCES ../src/getopt/getopt.c)
endif()

# Create client executable
add_executable(rtl_433_client ${CLIENT_SOURCES})

# Link libraries
target_link_libraries(rtl_433_client 
    ${RTL433_SHARED_LIBRARIES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../build/src/libr_433.a  # Main rtl_433 library
    ${CMAKE_CURRENT_SOURCE_DIR}/../build/src/libdata.a   # Data handling library
    ${LIBRTLSDR_LIBRARIES}
    ${LIBUSB_LIBRARIES}
    ${JSON_C_LIBRARIES}
    ${CURL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    m
)

# Add SoapySDR if found
if(SOAPYSDR_FOUND)
    target_link_libraries(rtl_433_client ${SOAPYSDR_LIBRARIES})
    target_include_directories(rtl_433_client PRIVATE ${SOAPYSDR_INCLUDE_DIRS})
endif()

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
    ${LIBUSB_INCLUDE_DIRS}
    ${JSON_C_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

# Install executable
install(TARGETS rtl_433_client DESTINATION bin)
