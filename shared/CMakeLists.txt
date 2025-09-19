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

# Заголовочные файлы
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include  # Заголовки оригинального rtl_433
    ${JSON_INCLUDE_DIRS}
)

if(ENABLE_RABBITMQ)
    include_directories(${AMQP_INCLUDE_DIR})
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
)

# Создание статической библиотеки
add_library(rtl433_shared STATIC ${SHARED_SOURCES})

# Линковка с зависимостями
target_link_libraries(rtl433_shared 
    ${JSON_LIBRARIES}
)

if(ENABLE_RABBITMQ)
    target_link_libraries(rtl433_shared ${AMQP_LIBRARIES})
endif()

# Установка флагов компиляции
target_compile_options(rtl433_shared PRIVATE ${JSON_CFLAGS_OTHER})

# Установка публичных заголовков
set_target_properties(rtl433_shared PROPERTIES
    PUBLIC_HEADER "include/rtl433_transport.h;include/rtl433_signal.h;include/rtl433_config.h;include/rtl433_input.h;include/rtl433_rfraw.h;include/rtl433_pulse_enhanced.h;include/rtl433_override.h"
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
