# CMake файл для общей библиотеки rtl_433
cmake_minimum_required(VERSION 3.10)

project(rtl433_shared)

# Установка стандарта C
set(CMAKE_C_STANDARD 99)

# Поиск зависимостей
find_package(PkgConfig REQUIRED)

# JSON-C
pkg_check_modules(JSON REQUIRED json-c)

# RabbitMQ (опционально)
find_path(AMQP_INCLUDE_DIR amqp.h)
find_library(AMQP_LIBRARIES rabbitmq)

if(AMQP_INCLUDE_DIR AND AMQP_LIBRARIES)
    set(ENABLE_RABBITMQ TRUE)
    add_definitions(-DENABLE_RABBITMQ)
    message(STATUS "RabbitMQ support enabled")
else()
    set(ENABLE_RABBITMQ FALSE)
    message(STATUS "RabbitMQ support disabled")
endif()

# ASN1 support (optional)
find_program(ASN1C_EXECUTABLE asn1c)

if(ASN1C_EXECUTABLE)
    set(ENABLE_ASN1 TRUE)
    add_definitions(-DENABLE_ASN1)
    message(STATUS "ASN1 support enabled")
    
    # ASN1 generation directory
    set(ASN1_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/asn1-generated)
    set(ASN1_SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/asn1/rtl433-rabbitmq.asn1)
else()
    set(ENABLE_ASN1 FALSE)
    message(STATUS "ASN1 support disabled - asn1c compiler not found")
endif()

# Заголовочные файлы
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include  # Заголовки оригинального rtl_433
    ${CMAKE_CURRENT_SOURCE_DIR}/../src      # Внутренние заголовки rtl_433
    ${JSON_INCLUDE_DIRS}
)

if(ENABLE_RABBITMQ)
    include_directories(${AMQP_INCLUDE_DIR})
endif()

if(ENABLE_ASN1)
    include_directories(${ASN1_OUTPUT_DIR})
endif()

# Исходные файлы
set(SHARED_SOURCES
    src/rtl433_transport.c
    src/rtl433_signal.c
    src/rtl433_config.c
    src/rtl433_input.c
    src/rtl433_rfraw.c
    src/rtl433_pulse_enhanced.c
    src/rtl433_override.c
    src/output_rabbitmq.c
    src/rtl433_signal_format.c
    src/rtl433_asn1.c
)

# ASN1 code generation and sources
if(ENABLE_ASN1)
    # List of known ASN1 C files that will be generated (based on actual asn1c output)
    set(ASN1_GENERATED_SOURCES
        ${ASN1_OUTPUT_DIR}/BIT_STRING.c
        ${ASN1_OUTPUT_DIR}/BOOLEAN.c
        ${ASN1_OUTPUT_DIR}/ConfigAction.c
        ${ASN1_OUTPUT_DIR}/ConfigData.c
        ${ASN1_OUTPUT_DIR}/ConfigMessage.c
        ${ASN1_OUTPUT_DIR}/ConfigParameter.c
        ${ASN1_OUTPUT_DIR}/DetectedMessage.c
        ${ASN1_OUTPUT_DIR}/DetectionMetadata.c
        ${ASN1_OUTPUT_DIR}/DeviceConfig.c
        ${ASN1_OUTPUT_DIR}/DeviceData.c
        ${ASN1_OUTPUT_DIR}/DeviceDataField.c
        ${ASN1_OUTPUT_DIR}/DeviceFieldValue.c
        ${ASN1_OUTPUT_DIR}/DeviceInfo.c
        ${ASN1_OUTPUT_DIR}/ErrorInfo.c
        ${ASN1_OUTPUT_DIR}/ErrorSeverity.c
        ${ASN1_OUTPUT_DIR}/FilterSettings.c
        ${ASN1_OUTPUT_DIR}/FrequencyRange.c
        ${ASN1_OUTPUT_DIR}/GeneralizedTime.c
        ${ASN1_OUTPUT_DIR}/INTEGER.c
        ${ASN1_OUTPUT_DIR}/INTEGER_oer.c
        ${ASN1_OUTPUT_DIR}/ModulationType.c
        ${ASN1_OUTPUT_DIR}/NativeEnumerated.c
        ${ASN1_OUTPUT_DIR}/NativeEnumerated_oer.c
        ${ASN1_OUTPUT_DIR}/NativeInteger.c
        ${ASN1_OUTPUT_DIR}/NativeInteger_oer.c
        ${ASN1_OUTPUT_DIR}/NativeReal.c
        ${ASN1_OUTPUT_DIR}/OCTET_STRING.c
        ${ASN1_OUTPUT_DIR}/OCTET_STRING_oer.c
        ${ASN1_OUTPUT_DIR}/OPEN_TYPE.c
        ${ASN1_OUTPUT_DIR}/OPEN_TYPE_oer.c
        ${ASN1_OUTPUT_DIR}/PulsesData.c
        ${ASN1_OUTPUT_DIR}/REAL.c
        ${ASN1_OUTPUT_DIR}/RFParameters.c
        ${ASN1_OUTPUT_DIR}/RTL433Message.c
        ${ASN1_OUTPUT_DIR}/SignalData.c
        ${ASN1_OUTPUT_DIR}/SignalMessage.c
        ${ASN1_OUTPUT_DIR}/SignalQuality.c
        ${ASN1_OUTPUT_DIR}/StatusMessage.c
        ${ASN1_OUTPUT_DIR}/SystemSettings.c
        ${ASN1_OUTPUT_DIR}/SystemStatistics.c
        ${ASN1_OUTPUT_DIR}/SystemStatus.c
        ${ASN1_OUTPUT_DIR}/TimingInfo.c
        ${ASN1_OUTPUT_DIR}/UTF8String.c
        ${ASN1_OUTPUT_DIR}/asn_SEQUENCE_OF.c
        ${ASN1_OUTPUT_DIR}/asn_SET_OF.c
        ${ASN1_OUTPUT_DIR}/asn_application.c
        ${ASN1_OUTPUT_DIR}/asn_bit_data.c
        ${ASN1_OUTPUT_DIR}/asn_codecs_prim.c
        ${ASN1_OUTPUT_DIR}/asn_internal.c
        ${ASN1_OUTPUT_DIR}/asn_random_fill.c
        ${ASN1_OUTPUT_DIR}/ber_decoder.c
        ${ASN1_OUTPUT_DIR}/ber_tlv_length.c
        ${ASN1_OUTPUT_DIR}/ber_tlv_tag.c
        ${ASN1_OUTPUT_DIR}/constr_CHOICE.c
        ${ASN1_OUTPUT_DIR}/constr_CHOICE_oer.c
        ${ASN1_OUTPUT_DIR}/constr_SEQUENCE.c
        ${ASN1_OUTPUT_DIR}/constr_SEQUENCE_OF.c
        ${ASN1_OUTPUT_DIR}/constr_SEQUENCE_oer.c
        ${ASN1_OUTPUT_DIR}/constr_SET_OF.c
        ${ASN1_OUTPUT_DIR}/constr_SET_OF_oer.c
        ${ASN1_OUTPUT_DIR}/constr_TYPE.c
        ${ASN1_OUTPUT_DIR}/constraints.c
        ${ASN1_OUTPUT_DIR}/der_encoder.c
        ${ASN1_OUTPUT_DIR}/oer_decoder.c
        ${ASN1_OUTPUT_DIR}/oer_encoder.c
        ${ASN1_OUTPUT_DIR}/oer_support.c
        ${ASN1_OUTPUT_DIR}/per_decoder.c
        ${ASN1_OUTPUT_DIR}/per_encoder.c
        ${ASN1_OUTPUT_DIR}/per_opentype.c
        ${ASN1_OUTPUT_DIR}/per_support.c
        ${ASN1_OUTPUT_DIR}/xer_decoder.c
        ${ASN1_OUTPUT_DIR}/xer_encoder.c
        ${ASN1_OUTPUT_DIR}/xer_support.c
    )
    
    # Custom target to generate ASN1 C code
    add_custom_command(
        OUTPUT ${ASN1_GENERATED_SOURCES}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ASN1_OUTPUT_DIR}
        COMMAND ${ASN1C_EXECUTABLE} -fcompound-names -fincludes-quoted -fskeletons-copy -D ${ASN1_OUTPUT_DIR} ${ASN1_SCHEMA}
        DEPENDS ${ASN1_SCHEMA}
        COMMENT "Generating ASN1 C code from schema"
        VERBATIM
    )
    
    # Add ASN1 sources to shared sources
    list(APPEND SHARED_SOURCES ${ASN1_GENERATED_SOURCES})
endif()

# Создание статической библиотеки
add_library(rtl433_shared STATIC ${SHARED_SOURCES})

# Линковка с зависимостями
target_link_libraries(rtl433_shared 
    ${JSON_LIBRARIES}
)

if(ENABLE_RABBITMQ)
    target_link_libraries(rtl433_shared ${AMQP_LIBRARIES})
endif()

if(ENABLE_ASN1)
    target_link_libraries(rtl433_shared m)  # Math library for ASN1
endif()

# Установка флагов компиляции
target_compile_options(rtl433_shared PRIVATE ${JSON_CFLAGS_OTHER})

# Установка публичных заголовков
set_target_properties(rtl433_shared PROPERTIES
    PUBLIC_HEADER "include/rtl433_transport.h;include/rtl433_signal.h;include/rtl433_config.h;include/rtl433_input.h;include/rtl433_rfraw.h;include/rtl433_pulse_enhanced.h;include/rtl433_override.h;include/rtl433_asn1.h"
)

# Экспорт переменных для использования в родительском CMake
set(RTL433_SHARED_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)
set(RTL433_SHARED_LIBRARIES rtl433_shared PARENT_SCOPE)

# Опции установки
install(TARGETS rtl433_shared
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/rtl433
)

# Установка конфигурационного файла для pkg-config
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/rtl433_shared.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/rtl433_shared.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rtl433_shared.pc
    DESTINATION lib/pkgconfig
)
