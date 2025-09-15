########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.6...3.10)
# Fix behavior of CMAKE_C_STANDARD when targeting macOS.
if(POLICY CMP0025)
    cmake_policy(SET CMP0025 NEW)
endif()
# Only interpret if() arguments as variables or keywords when unquoted.
if(POLICY CMP0054)
    cmake_policy(SET CMP0054 NEW)
endif()

project(rtl433 C)

########################################################################
# Split Architecture Options
########################################################################
option(BUILD_CLIENT "Build rtl_433_client for signal demodulation" OFF)
option(BUILD_SERVER "Build rtl_433_server for device decoding" OFF)
option(BUILD_SHARED "Build shared rtl_433 library" ON)
option(CLIENT_MINIMAL "Build minimal client (demo) instead of full client" ON)
option(SERVER_MINIMAL "Build minimal server (demo) instead of full server" ON)
option(ENABLE_MQTT "Enable MQTT transport for split architecture" OFF)
option(ENABLE_RABBITMQ "Enable RabbitMQ transport for split architecture" OFF)

#select the release build type by default to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)

########################################################################
# Get version info from Git
########################################################################
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_COMMIT)
if(GIT_COMMIT) # is a git repo
    # shorten branch spec
    string(REGEX REPLACE ".*/" "" GIT_BRANCH "${GIT_REFSPEC}")
    # use lightweight (non-annotated) tags
    git_describe(GIT_VERSION "--tags" "--exclude=nightly")
    git_timestamp(GIT_TIMESTAMP)
    message(STATUS "Found Git version: ${GIT_REFSPEC} commit ${GIT_COMMIT} from ${GIT_TIMESTAMP_ISO}")
    message(STATUS "Using Git version tag: ${GIT_VERSION} on ${GIT_BRANCH} at ${GIT_TIMESTAMP}")
    ADD_DEFINITIONS(-DGIT_VERSION=${GIT_VERSION})
    ADD_DEFINITIONS(-DGIT_BRANCH=${GIT_BRANCH})
    ADD_DEFINITIONS(-DGIT_TIMESTAMP=${GIT_TIMESTAMP})
endif()

########################################################################
# Fallback to get release version info from Changelog
########################################################################
if(NOT GIT_COMMIT AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md")
    # parse the CHANGELOG.md, this is clever, i.e. might go wrong ;)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md" CHANGELOG LIMIT_COUNT 42)
    # list(FILTER ...) needs CMake 3.6+
    foreach(item ${CHANGELOG})
        string(REGEX MATCH "^## Release (.+)" item ${item})
        if(CMAKE_MATCH_1)
            list(APPEND RELEASE_VERSIONS ${CMAKE_MATCH_1})
        endif()
    endforeach()
    if(RELEASE_VERSIONS)
        list(GET RELEASE_VERSIONS 0 RELEASE_VERSION)
        message(STATUS "Found Release version: ${RELEASE_VERSION}")
        ADD_DEFINITIONS(-DGIT_VERSION=${RELEASE_VERSION})
    endif()
endif()

########################################################################
# Compiler specific setup
########################################################################
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
if(("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_C_COMPILER_ID}" MATCHES "Clang") AND NOT WIN32)
    ADD_DEFINITIONS(-Wall)
    ADD_DEFINITIONS(-Wextra)
    ADD_DEFINITIONS(-Wvla)
    ADD_DEFINITIONS(-Wsign-compare)
    ADD_DEFINITIONS(-std=c99)
    ADD_DEFINITIONS(-pedantic)
    ADD_DEFINITIONS(-Wshadow)
    ADD_DEFINITIONS(-Wmissing-prototypes)
    if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang" OR NOT "7.0.0" VERSION_GREATER CMAKE_C_COMPILER_VERSION)
        ADD_DEFINITIONS(-Wimplicit-fallthrough)
    endif()
    #ADD_DEFINITIONS(-Wfloat-equal)
    #ADD_DEFINITIONS(-Wbad-function-cast)
    #ADD_DEFINITIONS(-Wdocumentation)
    add_definitions(-Wno-deprecated-declarations)

    # for strdup, setenv, use either
    #ADD_DEFINITIONS(-D_POSIX_C_SOURCE=200809) # does not work with uClibc
    ADD_DEFINITIONS(-D_GNU_SOURCE)
    #http://gcc.gnu.org/wiki/Visibility
    add_definitions(-fvisibility=hidden)

    # CMake Release default for GCC/Clang is "-O3 -DNDEBUG"
    # set(CMAKE_C_FLAGS_RELEASE -O2)
    # CMake Debug default for GCC/Clang is "-g -DNDEBUG"
    # set(CMAKE_C_FLAGS_DEBUG -g3 -O0)
    # make use of ASAN
    set(CMAKE_C_FLAGS_DEBUG "-ggdb -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer")
endif()
if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
    # make sure we don't accidentally copy more than an int
    ADD_DEFINITIONS(-Wlarge-by-value-copy=8)
endif()

# Enable Static analysis on GCC13.2.0+
if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" AND NOT "13.2.0" VERSION_GREATER CMAKE_C_COMPILER_VERSION)
    message(STATUS "Using GCC Static analysis")
    add_definitions(-fanalyzer)
endif()

# Shut MSVC up about strdup and strtok
if(MSVC)
    ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_DEPRECATE)
    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
    ADD_DEFINITIONS(-DNOMINMAX)
    # don't warn on type truncation
    add_compile_options("/wd4244")
    add_compile_options("/wd4267")
    add_compile_options("/wd4305")
endif()

# Fix printf %zu
if(MINGW)
    add_definitions(-D__USE_MINGW_ANSI_STDIO)
endif()

# Make sure we get M_PI
if(WIN32)
    add_definitions(-D_USE_MATH_DEFINES)
endif()

# On Unix pass the SYSCONFDIR through to conf file loader
if(NOT WIN32)
    include(GNUInstallDirs)
    add_definitions(-DINSTALL_SYSCONFDIR=${CMAKE_INSTALL_FULL_SYSCONFDIR})
endif()

########################################################################
# Use pkg-config
########################################################################
find_package(PkgConfig)

########################################################################
# Option to force ANSI-colored build output (for Ninja)
########################################################################
option(FORCE_COLORED_BUILD "Always produce ANSI-colored build output (GNU/Clang only)." FALSE)
if(FORCE_COLORED_BUILD)
    if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options(-fdiagnostics-color=always)
    elseif("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
       add_compile_options(-fcolor-diagnostics)
    endif()
endif()

########################################################################
# Enable IPv6 support
########################################################################
option(ENABLE_IPV6 "Enable IPv6 support" TRUE)
if(ENABLE_IPV6)
    message(STATUS "IPv6 support enabled.")
    ADD_DEFINITIONS(-DMG_ENABLE_IPV6=1)
    if(MINGW)
        # IPv6 requires at least Vista for inet_pton, inet_ntop
        add_definitions(-D_WIN32_WINNT=0x0600)
    endif()
else()
    message(STATUS "IPv6 support disabled.")
endif()

########################################################################
# Find Threads support build dependencies
########################################################################
set(ENABLE_THREADS AUTO CACHE STRING "Enable Threads support")
set_property(CACHE ENABLE_THREADS PROPERTY STRINGS AUTO ON OFF)
if(ENABLE_THREADS) # AUTO / ON

find_package(Threads)
if(Threads_FOUND)
    message(STATUS "Threads support will be compiled.")
    ADD_DEFINITIONS(-DTHREADS)
elseif(ENABLE_THREADS STREQUAL "AUTO")
    message(STATUS "Threads support not found, some features will be disabled.")
else()
    message(FATAL_ERROR "Threads support not found.")
endif()

else()
    message(STATUS "Threads support disabled.")
endif()

########################################################################
# Find OpenSSL build dependencies
########################################################################
set(ENABLE_OPENSSL AUTO CACHE STRING "Enable OpenSSL TLS support")
set_property(CACHE ENABLE_OPENSSL PROPERTY STRINGS AUTO ON OFF)
if(ENABLE_OPENSSL) # AUTO / ON

find_package(OpenSSL)
# Get actual libs from pkg-config to support edge cases (static on Sparc)
# This will break Win32 builds, exclude that platform
pkg_check_modules(PC_OPENSSL QUIET openssl)
if(PC_OPENSSL_FOUND AND NOT WIN32)
    set(OPENSSL_LIBRARIES ${PC_OPENSSL_LINK_LIBRARIES})
    message(STATUS "Using OpenSSL: ${OPENSSL_LIBRARIES}")
endif()
if(OPENSSL_FOUND)
    message(STATUS "OpenSSL TLS support will be compiled. Found version ${OPENSSL_VERSION}")
    include_directories(${OPENSSL_INCLUDE_DIR})
    list(APPEND SDR_LIBRARIES ${OPENSSL_LIBRARIES})
    ADD_DEFINITIONS(-DOPENSSL)
    ADD_DEFINITIONS(-DMG_ENABLE_SSL)
elseif(ENABLE_OPENSSL STREQUAL "AUTO")
    message(STATUS "OpenSSL development files not found, TLS won't be possible.")
else()
    message(FATAL_ERROR "OpenSSL development files not found.")
endif()

else()
    message(STATUS "OpenSSL TLS disabled.")
endif()

########################################################################
# Find LibRTLSDR build dependencies
########################################################################
set(ENABLE_RTLSDR ON CACHE STRING "Enable RTL-SDR (lbrtlsdr) driver support")
set_property(CACHE ENABLE_RTLSDR PROPERTY STRINGS AUTO ON OFF)
if(ENABLE_RTLSDR) # AUTO / ON

find_package(LibRTLSDR)
find_package(LibUSB)
if(LibRTLSDR_FOUND)
    message(STATUS "RTL-SDR device input will be compiled. Found version ${LibRTLSDR_VERSION}")
    include_directories(${LibRTLSDR_INCLUDE_DIRS})
    list(APPEND SDR_LIBRARIES ${LibRTLSDR_LIBRARIES})
    ADD_DEFINITIONS(-DRTLSDR)

if(LibUSB_FOUND)
    message(STATUS "libusb-1.0 error messages are available. Found version ${LibUSB_VERSION}")
    include_directories(${LibUSB_INCLUDE_DIRS})
    list(APPEND SDR_LIBRARIES ${LibUSB_LIBRARIES})
    ADD_DEFINITIONS(-DLIBUSB1)
else()
    message(STATUS "libusb-1.0 error messages are not available.")
endif()

elseif(ENABLE_RTLSDR STREQUAL "AUTO")
    message(STATUS "RTL-SDR development files not found, RTL-SDR device input won't be possible.")
else()
    message(FATAL_ERROR "RTL-SDR development files not found.")
endif()

else()
    message(STATUS "RTL-SDR device input disabled.")
endif()

########################################################################
# Find SoapySDR build dependencies
########################################################################
set(ENABLE_SOAPYSDR AUTO CACHE STRING "Enable SoapySDR driver support")
set_property(CACHE ENABLE_SOAPYSDR PROPERTY STRINGS AUTO ON OFF)
if(ENABLE_SOAPYSDR) # AUTO / ON

find_package(SoapySDR "0.6" NO_MODULE)
if(SoapySDR_FOUND)
    message(STATUS "SoapySDR device input will be compiled. Found version ${SoapySDR_VERSION}")
    include_directories(${SoapySDR_INCLUDE_DIRS})
    list(APPEND SDR_LIBRARIES ${SoapySDR_LIBRARIES})
    ADD_DEFINITIONS(-DSOAPYSDR)
elseif(ENABLE_SOAPYSDR STREQUAL "AUTO")
    message(STATUS "SoapySDR development files not found, SoapySDR device input won't be possible.")
else()
    message(FATAL_ERROR "SoapySDR development files not found.")
endif()

else()
    message(STATUS "SoapySDR device input disabled.")
endif()

########################################################################
# Setup optional Profiling with GPerfTools
########################################################################
# cmake -DCMAKE_BUILD_TYPE=Profile ..
# CPUPROFILE=prof.out ./src/rtl_433 ...
# pprof -text ./src/rtl_433 prof.out
if("${CMAKE_BUILD_TYPE}" STREQUAL "Profile")
    message(STATUS "Build type set to Profile. Linking GPerfTools.")
    find_package(Gperftools REQUIRED)
    include_directories(${GPERFTOOLS_INCLUDE_DIR})
    list(APPEND SDR_LIBRARIES ${GPERFTOOLS_LIBRARIES} -Wl,-no_pie)
    ADD_DEFINITIONS(-g)
    ADD_DEFINITIONS(-fno-builtin-malloc)
    ADD_DEFINITIONS(-fno-builtin-calloc)
    ADD_DEFINITIONS(-fno-builtin-realloc)
    ADD_DEFINITIONS(-fno-builtin-free)
endif()

########################################################################
# Setup the include and linker paths
########################################################################
if(MINGW OR MSVC)
list(APPEND NET_LIBRARIES ws2_32 mswsock netapi32)
endif()

include_directories(
    BEFORE
    ${PROJECT_SOURCE_DIR}/include
)

########################################################################
# Create uninstall target
########################################################################
configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
@ONLY)

add_custom_target(uninstall
    ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)

########################################################################
# Build documentation with Doxygen
########################################################################
option(BUILD_DOCUMENTATION "Create and install the HTML based API
documentation (requires Doxygen)" OFF)

find_package(Doxygen)
if(BUILD_DOCUMENTATION)
    if(NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "Doxygen is needed to build the documentation.")
    endif()

    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    if(DOXYGEN_DOT_FOUND)
        set(HAVE_DOT "YES")
    else()
        set(HAVE_DOT "NO")
    endif()

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message(STATUS "Doxygen build started")

    # note the option ALL which allows to build the docs together with the application
    add_custom_target(doc_doxygen ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
    #    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/html DESTINATION share/doc)
endif()

########################################################################
# Build tests with analyzer
########################################################################
option(BUILD_TESTING_ANALYZER "Build the testing tree with static
analyzer (requires Clang)" OFF)

########################################################################
# Build tests
########################################################################
include(CTest) # note: this adds a BUILD_TESTING which defaults to ON

########################################################################
# Add subdirectories
########################################################################
add_subdirectory(include)
add_subdirectory(src)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif(BUILD_TESTING)
add_subdirectory(conf)

# use space-separation format for the pc file
STRING(REPLACE ";" " " RTL433_PC_CFLAGS "${RTL433_PC_CFLAGS}")
STRING(REPLACE ";" " " RTL433_PC_LIBS "${RTL433_PC_LIBS}")

# unset these vars to avoid hard-coded paths to cross environment
IF(CMAKE_CROSSCOMPILING)
    UNSET(RTL433_PC_CFLAGS)
    UNSET(RTL433_PC_LIBS)
ENDIF(CMAKE_CROSSCOMPILING)

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix \${prefix})
set(libdir \${exec_prefix}/lib)
set(includedir \${prefix}/include)

INSTALL(
    FILES
    DESTINATION lib/pkgconfig
)

install(DIRECTORY man
    DESTINATION share
    PATTERN ".md" EXCLUDE)

########################################################################
# Split Architecture Dependencies
########################################################################
if(BUILD_CLIENT OR BUILD_SERVER)
    # Common dependencies for split architecture
    pkg_check_modules(JSON_C json-c)
    if(NOT JSON_C_FOUND)
        message(FATAL_ERROR "json-c library is required for split architecture")
    endif()
    
    # Client dependencies
    if(BUILD_CLIENT)
        find_package(CURL REQUIRED)
        
        # MQTT support for client
        if(ENABLE_MQTT)
            find_path(PAHO_MQTT_INCLUDE_DIR MQTTClient.h)
            find_library(PAHO_MQTT_LIBRARY paho-mqtt3c)
            if(PAHO_MQTT_INCLUDE_DIR AND PAHO_MQTT_LIBRARY)
                add_definitions(-DENABLE_MQTT)
            else()
                message(WARNING "paho-mqtt library not found - MQTT transport disabled")
                set(ENABLE_MQTT OFF)
            endif()
        endif()
        
        # RabbitMQ support for client
        if(ENABLE_RABBITMQ)
            find_path(RABBITMQ_INCLUDE_DIR amqp.h)
            find_library(RABBITMQ_LIBRARY NAMES rabbitmq librabbitmq)
            if(RABBITMQ_INCLUDE_DIR AND RABBITMQ_LIBRARY)
                add_definitions(-DENABLE_RABBITMQ)
            else()
                message(WARNING "rabbitmq library not found - RabbitMQ transport disabled")
                set(ENABLE_RABBITMQ OFF)
            endif()
        endif()
    endif()
    
    # Server dependencies
    if(BUILD_SERVER)
        pkg_check_modules(SQLITE3 sqlite3)
        if(NOT SQLITE3_FOUND)
            message(FATAL_ERROR "sqlite3 library is required for server")
        endif()
        
        find_package(Threads REQUIRED)
    endif()
endif()

########################################################################
# Common source files for split architecture
########################################################################
if(BUILD_CLIENT OR BUILD_SERVER)
    # Base sources for both client and server (no device decoders)
    set(BASE_SOURCES
        src/abuf.c
        src/baseband.c
        src/bit_util.c
        src/bitbuffer.c
        src/data.c
        src/decoder_util.c
        src/fileformat.c
        src/jsmn.c
        src/list.c
        src/logger.c
        src/pulse_analyzer.c
        src/pulse_data.c
        src/pulse_detect_fsk.c
        src/pulse_detect.c
        src/pulse_slicer.c
        src/r_util.c
        src/compat_paths.c
        src/compat_time.c
        src/confparse.c
        src/optparse.c
    )
    
    # Client uses the main rtl_433 library
    set(CLIENT_CORE_SOURCES
        # Client will link with libr_433.a - no need to rebuild sources
    )
    
    # Server-specific sources (includes device decoders)
    set(SERVER_CORE_SOURCES
        ${BASE_SOURCES}
        src/r_api.c
        src/data_tag.c
        src/am_analyze.c
    )
    
    # Add getopt for Windows
    if(WIN32)
        list(APPEND CLIENT_CORE_SOURCES src/getopt/getopt.c)
        list(APPEND SERVER_CORE_SOURCES src/getopt/getopt.c)
    endif()
endif()

########################################################################
# rtl_433_client
########################################################################
if(BUILD_CLIENT)
    message(STATUS "Building rtl_433_client")
    
    # Choose client source based on shared library usage
    # Choose client version
    if(CLIENT_MINIMAL)
        set(CLIENT_SOURCES
            client/src/rtl_433_client_shared.c
        )
        message(STATUS "Building minimal client (demo)")
    else()
        set(CLIENT_SOURCES
            client/src/rtl_433_client_new.c
        )
        message(STATUS "Building full client (from original rtl_433.c)")
        # Full client needs main rtl_433 library
        set(CLIENT_NEEDS_RTL433_LIB TRUE)
    endif()
    
    add_executable(rtl_433_client ${CLIENT_SOURCES})
    
    # Client depends on the main rtl_433 library
    add_dependencies(rtl_433_client r_433)
    
    target_include_directories(rtl_433_client PRIVATE 
        include
        client/src
        ${JSON_C_INCLUDE_DIRS}
        ${CURL_INCLUDE_DIRS}
    )
    
    target_link_libraries(rtl_433_client 
        r_433                    # Link with the main rtl_433 library
        ${SDR_LIBRARIES}         # Includes OpenSSL and other dependencies
        ${NET_LIBRARIES}         # Network libraries (ws2_32, etc.)
        ${CMAKE_THREAD_LIBS_INIT} # Threading support
        ${JSON_C_LIBRARIES}
        ${CURL_LIBRARIES}
        m
    )
    
    # Link with shared library (required)
    target_include_directories(rtl_433_client PRIVATE ${CMAKE_SOURCE_DIR}/shared/include)
    target_link_libraries(rtl_433_client rtl433_shared)
    target_compile_definitions(rtl_433_client PRIVATE USE_SHARED_LIBRARY)
    add_dependencies(rtl_433_client rtl433_shared)
    message(STATUS "rtl_433_client: Using shared rtl433 library")
    
    # Full client already linked with main library above, just add dependency
    if(CLIENT_NEEDS_RTL433_LIB)
        message(STATUS "rtl_433_client: Using main rtl_433 library for full functionality")
    endif()
    
    # Add pthread flags like main rtl_433
    if(THREADS_HAVE_PTHREAD_ARG)
        set_target_properties(rtl_433_client PROPERTIES COMPILE_OPTIONS "-pthread")
        set_target_properties(rtl_433_client PROPERTIES INTERFACE_COMPILE_OPTIONS "-pthread")
    endif()
    
    # Client uses real SDR libraries
    # target_compile_definitions(rtl_433_client PRIVATE RTL433_CLIENT_STUBS)
    
    # Use real SDR libraries:
    if(LIBRTLSDR_FOUND)
        target_include_directories(rtl_433_client PRIVATE ${LIBRTLSDR_INCLUDE_DIRS})
        target_link_libraries(rtl_433_client ${LIBRTLSDR_LIBRARIES})
        target_compile_options(rtl_433_client PRIVATE ${LIBRTLSDR_CFLAGS_OTHER})
        target_compile_definitions(rtl_433_client PRIVATE RTLSDR)
        message(STATUS "rtl_433_client: RTL-SDR support enabled")
    endif()
    
    if(SOAPYSDR_FOUND)
        target_include_directories(rtl_433_client PRIVATE ${SOAPYSDR_INCLUDE_DIRS})
        target_link_libraries(rtl_433_client PRIVATE ${SOAPYSDR_LIBRARIES})
        target_compile_definitions(rtl_433_client PRIVATE SOAPYSDR)
        message(STATUS "rtl_433_client: SoapySDR support enabled")
    endif()
    
    if(ENABLE_MQTT AND PAHO_MQTT_INCLUDE_DIR AND PAHO_MQTT_LIBRARY)
        target_include_directories(rtl_433_client PRIVATE ${PAHO_MQTT_INCLUDE_DIR})
        target_link_libraries(rtl_433_client ${PAHO_MQTT_LIBRARY})
    endif()
    
    if(ENABLE_RABBITMQ AND RABBITMQ_INCLUDE_DIR AND RABBITMQ_LIBRARY)
        target_include_directories(rtl_433_client PRIVATE ${RABBITMQ_INCLUDE_DIR})
        target_link_libraries(rtl_433_client ${RABBITMQ_LIBRARY})
    endif()
    
    install(TARGETS rtl_433_client DESTINATION bin)
endif()

########################################################################
# rtl_433_server
########################################################################
if(BUILD_SERVER)
    message(STATUS "Building rtl_433_server")
    
    # Get all device decoders
    file(GLOB DEVICE_SOURCES "src/devices/*.c")
    
    # Choose server version
    if(SERVER_MINIMAL)
        set(SERVER_SOURCES
            server/src/rtl_433_server_minimal.c
        )
        message(STATUS "Building minimal server (demo)")
    else()
        set(SERVER_SOURCES
            server/src/rtl_433_server_full.c
        )
        message(STATUS "Building full server (real device decoding)")
        # Full server needs main rtl_433 library
        set(SERVER_NEEDS_RTL433_LIB TRUE)
    endif()
    
    add_executable(rtl_433_server ${SERVER_SOURCES})
    
    target_include_directories(rtl_433_server PRIVATE 
        include
        server/src
        ${JSON_C_INCLUDE_DIRS}
        ${SQLITE3_INCLUDE_DIRS}
    )
    
    target_link_libraries(rtl_433_server 
        ${JSON_C_LIBRARIES}
        ${SQLITE3_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${OPENSSL_LIBRARIES}
        ${SDR_LIBRARIES}
        ${NET_LIBRARIES}
        m
    )
    
    # Link with shared library (required)
    target_include_directories(rtl_433_server PRIVATE ${CMAKE_SOURCE_DIR}/shared/include)
    target_link_libraries(rtl_433_server rtl433_shared)
    target_compile_definitions(rtl_433_server PRIVATE USE_SHARED_LIBRARY)
    add_dependencies(rtl_433_server rtl433_shared)
    message(STATUS "rtl_433_server: Using shared rtl433 library")
    
    # Full server needs main rtl_433 library
    if(SERVER_NEEDS_RTL433_LIB)
        target_link_libraries(rtl_433_server r_433)
        add_dependencies(rtl_433_server r_433)
        message(STATUS "rtl_433_server: Linking with main rtl_433 library for full functionality")
    endif()
    
    # Add SDR libraries for full server (same as client)
    if(LibRTLSDR_FOUND)
        target_include_directories(rtl_433_server PRIVATE ${LibRTLSDR_INCLUDE_DIRS})
        target_link_libraries(rtl_433_server ${LibRTLSDR_LIBRARIES})
        target_compile_definitions(rtl_433_server PRIVATE RTLSDR)
        message(STATUS "rtl_433_server: RTL-SDR support enabled")
    endif()
    
    if(SoapySDR_FOUND)
        target_include_directories(rtl_433_server PRIVATE ${SoapySDR_INCLUDE_DIRS})
        target_link_libraries(rtl_433_server ${SoapySDR_LIBRARIES})
        target_compile_definitions(rtl_433_server PRIVATE SOAPYSDR)
        message(STATUS "rtl_433_server: SoapySDR support enabled")
    endif()
    
    if(LibUSB_FOUND)
        target_include_directories(rtl_433_server PRIVATE ${LibUSB_INCLUDE_DIRS})
        target_link_libraries(rtl_433_server ${LibUSB_LIBRARIES})
        target_compile_definitions(rtl_433_server PRIVATE LIBUSB1)
        message(STATUS "rtl_433_server: LibUSB support enabled")
    endif()
    
    if(ENABLE_MQTT AND PAHO_MQTT_INCLUDE_DIR AND PAHO_MQTT_LIBRARY)
        target_include_directories(rtl_433_server PRIVATE ${PAHO_MQTT_INCLUDE_DIR})
        target_link_libraries(rtl_433_server ${PAHO_MQTT_LIBRARY})
    endif()
    
    if(ENABLE_RABBITMQ AND RABBITMQ_INCLUDE_DIR AND RABBITMQ_LIBRARY)
        target_include_directories(rtl_433_server PRIVATE ${RABBITMQ_INCLUDE_DIR})
        target_link_libraries(rtl_433_server ${RABBITMQ_LIBRARY})
    endif()
    
    install(TARGETS rtl_433_server DESTINATION bin)
    
    # Install configuration files
    install(DIRECTORY server/config/ 
            DESTINATION etc/rtl_433_server
            FILES_MATCHING PATTERN "*.conf*")
endif()

########################################################################
# Shared Library
########################################################################
# Always build shared library (required)
message(STATUS "Building shared rtl_433 library")
add_subdirectory(shared)

# Export variables for client and server
set(RTL433_SHARED_LIBRARIES ${RTL433_SHARED_LIBRARIES})
set(RTL433_SHARED_INCLUDE_DIR ${RTL433_SHARED_INCLUDE_DIR})

########################################################################
# Environment detection and status
########################################################################
if(BUILD_CLIENT OR BUILD_SERVER)
    # Detect WSL environment
    if(EXISTS "/proc/version")
        file(READ "/proc/version" PROC_VERSION)
        if(PROC_VERSION MATCHES "Microsoft")
            message(STATUS "WSL environment detected")
            set(WSL_DETECTED TRUE)
        else()
            set(WSL_DETECTED FALSE)
        endif()
    else()
        set(WSL_DETECTED FALSE)
    endif()
    
    message(STATUS "")
    message(STATUS "RTL433 Split Architecture Configuration:")
    message(STATUS "  Build client: ${BUILD_CLIENT}")
    message(STATUS "  Build server: ${BUILD_SERVER}")
    message(STATUS "  MQTT support: ${ENABLE_MQTT}")
    message(STATUS "  RabbitMQ support: ${ENABLE_RABBITMQ}")
    if(BUILD_CLIENT)
        message(STATUS "  RTL-SDR support: ${LIBRTLSDR_FOUND}")
        message(STATUS "  SoapySDR support: ${SOAPYSDR_FOUND}")
    endif()
    message(STATUS "  WSL detected: ${WSL_DETECTED}")
    
    if(WSL_DETECTED AND BUILD_CLIENT AND LIBRTLSDR_FOUND)
        message(STATUS "")
        message(STATUS "WSL USB Setup Required:")
        message(STATUS "  1. Install usbipd on Windows: winget install usbipd")
        message(STATUS "  2. Attach RTL-SDR: usbipd wsl attach --busid <busid>")
        message(STATUS "  3. Test device: rtl_test")
    endif()
    
    message(STATUS "")
    message(STATUS "For dependency installation instructions, see README_SPLIT.md")
    message(STATUS "")
    
    # Install split architecture documentation
    install(FILES 
        README_SPLIT.md
        USAGE_EXAMPLES.md
        DESTINATION share/doc/rtl433_split
    )
endif()
