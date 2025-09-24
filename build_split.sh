#!/bin/bash
# Build script for RTL_433 split architecture on WSL Ubuntu 22.04

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Functions for message output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Default parameters
BUILD_DIR="build_split"
BUILD_TYPE="Release"
BUILD_CLIENT=ON
BUILD_SERVER=ON
BUILD_ORIGINAL=OFF
ENABLE_MQTT=ON
ENABLE_RABBITMQ=OFF
INSTALL_PREFIX="/usr/local"
JOBS=$(nproc)

# Help function
show_help() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -h, --help              Show this help"
    echo "  -d, --build-dir DIR     Build directory (default: $BUILD_DIR)"
    echo "  -t, --build-type TYPE   Build type: Release, Debug, RelWithDebInfo (default: $BUILD_TYPE)"
    echo "  -p, --prefix PREFIX     Install prefix (default: $INSTALL_PREFIX)"
    echo "  -j, --jobs JOBS         Number of parallel jobs (default: $JOBS)"
    echo "  --no-client             Don't build client"
    echo "  --no-server             Don't build server"
    echo "  --with-original         Build original rtl_433"
    echo "  --no-mqtt               Disable MQTT support"
    echo "  --with-rabbitmq         Enable RabbitMQ support"
    echo "  --clean                 Clean build directory"
    echo "  --install               Install after build"
    echo "  --package               Create package"
    echo ""
    echo "Examples:"
    echo "  $0                      Basic build"
    echo "  $0 --clean --install    Rebuild and install"
    echo "  $0 --with-rabbitmq      Build with RabbitMQ support"
    echo "  $0 --no-server -j 8     Client only, 8 threads"
}

# Command line argument parsing
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -d|--build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -t|--build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -p|--prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        --no-client)
            BUILD_CLIENT=OFF
            shift
            ;;
        --no-server)
            BUILD_SERVER=OFF
            shift
            ;;
        --with-original)
            BUILD_ORIGINAL=ON
            shift
            ;;
        --no-mqtt)
            ENABLE_MQTT=OFF
            shift
            ;;
        --with-rabbitmq)
            ENABLE_RABBITMQ=ON
            shift
            ;;
        --clean)
            CLEAN=1
            shift
            ;;
        --install)
            INSTALL=1
            shift
            ;;
        --package)
            PACKAGE=1
            shift
            ;;
        *)
            print_error "Unknown parameter: $1"
            show_help
            exit 1
            ;;
    esac
done

# Dependency checking
check_dependencies() {
    print_info "Checking dependencies..."
    
    # Required dependencies
    local deps=("cmake" "make" "gcc" "pkg-config")
    
    if [[ "$BUILD_CLIENT" == "ON" ]]; then
        deps+=("curl-config")
    fi
    
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            print_error "Dependency not found: $dep"
            exit 1
        fi
    done
    
    # Проверка библиотек
    if [[ "$BUILD_CLIENT" == "ON" ]]; then
        if ! pkg-config --exists libcurl; then
            print_error "libcurl не найдена"
            exit 1
        fi
        
        if ! pkg-config --exists librtlsdr; then
            print_warning "librtlsdr не найдена - клиент будет собран без поддержки RTL-SDR"
        fi
    fi
    
    if [[ "$BUILD_SERVER" == "ON" ]]; then
        if ! pkg-config --exists sqlite3; then
            print_error "sqlite3 не найдена"
            exit 1
        fi
    fi
    
    if ! pkg-config --exists json-c; then
        print_error "json-c не найдена"
        exit 1
    fi
    
    if [[ "$ENABLE_MQTT" == "ON" ]]; then
        if ! pkg-config --exists libpaho-mqtt3c; then
            print_warning "paho-mqtt не найдена - MQTT поддержка будет отключена"
            ENABLE_MQTT=OFF
        fi
    fi
    
    print_success "All dependencies found"
}

# Очистка директории сборки
clean_build() {
    if [[ -d "$BUILD_DIR" ]]; then
        print_info "Очистка директории сборки: $BUILD_DIR"
        rm -rf "$BUILD_DIR"
        print_success "Директория сборки очищена"
    fi
}

# Конфигурация проекта
configure_project() {
    print_info "Конфигурация проекта..."
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
        "-DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX"
        "-DBUILD_CLIENT=$BUILD_CLIENT"
        "-DBUILD_SERVER=$BUILD_SERVER"
        "-DBUILD_ORIGINAL=$BUILD_ORIGINAL"
        "-DENABLE_MQTT=$ENABLE_MQTT"
        "-DENABLE_RABBITMQ=$ENABLE_RABBITMQ"
        "-f" "../CMakeLists_split.txt"
        ".."
    )
    
    print_info "Выполнение: cmake ${cmake_args[*]}"
    
    if cmake "${cmake_args[@]}"; then
        print_success "Конфигурация завершена успешно"
    else
        print_error "Ошибка конфигурации"
        exit 1
    fi
    
    cd ..
}

# Сборка проекта
build_project() {
    print_info "Сборка проекта..."
    
    cd "$BUILD_DIR"
    
    if make -j"$JOBS"; then
        print_success "Сборка завершена успешно"
    else
        print_error "Ошибка сборки"
        exit 1
    fi
    
    cd ..
}

# Установка
install_project() {
    print_info "Установка проекта..."
    
    cd "$BUILD_DIR"
    
    if [[ "$INSTALL_PREFIX" == "/usr/local" ]] || [[ "$INSTALL_PREFIX" =~ ^/usr ]]; then
        if sudo make install; then
            print_success "Установка завершена успешно"
        else
            print_error "Ошибка установки"
            exit 1
        fi
    else
        if make install; then
            print_success "Установка завершена успешно"
        else
            print_error "Ошибка установки"
            exit 1
        fi
    fi
    
    cd ..
}

# Создание пакета
create_package() {
    print_info "Создание пакета..."
    
    cd "$BUILD_DIR"
    
    if make package; then
        print_success "Пакет создан успешно"
        ls -la *.deb *.rpm *.tar.gz 2>/dev/null || true
    else
        print_error "Ошибка создания пакета"
        exit 1
    fi
    
    cd ..
}

# Вывод информации о сборке
print_build_info() {
    print_info "Конфигурация сборки:"
    echo "  Директория сборки: $BUILD_DIR"
    echo "  Тип сборки: $BUILD_TYPE"
    echo "  Префикс установки: $INSTALL_PREFIX"
    echo "  Параллельные задачи: $JOBS"
    echo "  Собирать клиент: $BUILD_CLIENT"
    echo "  Собирать сервер: $BUILD_SERVER"
    echo "  Собирать оригинальный rtl_433: $BUILD_ORIGINAL"
    echo "  Поддержка MQTT: $ENABLE_MQTT"
    echo "  Поддержка RabbitMQ: $ENABLE_RABBITMQ"
    echo ""
}

# Основная логика
main() {
    print_info "Начало сборки разделенной архитектуры rtl_433"
    
    print_build_info
    
    if [[ -n "$CLEAN" ]]; then
        clean_build
    fi
    
    check_dependencies
    configure_project
    build_project
    
    if [[ -n "$INSTALL" ]]; then
        install_project
    fi
    
    if [[ -n "$PACKAGE" ]]; then
        create_package
    fi
    
    print_success "Build completed successfully!"
    
    # Вывод информации о созданных файлах
    print_info "Созданные исполняемые файлы:"
    if [[ "$BUILD_CLIENT" == "ON" ]] && [[ -f "$BUILD_DIR/rtl_433_client" ]]; then
        echo "  rtl_433_client: $(realpath $BUILD_DIR/rtl_433_client)"
    fi
    if [[ "$BUILD_SERVER" == "ON" ]] && [[ -f "$BUILD_DIR/rtl_433_server" ]]; then
        echo "  rtl_433_server: $(realpath $BUILD_DIR/rtl_433_server)"
    fi
    if [[ "$BUILD_ORIGINAL" == "ON" ]] && [[ -f "$BUILD_DIR/src/rtl_433" ]]; then
        echo "  rtl_433: $(realpath $BUILD_DIR/src/rtl_433)"
    fi
    
    print_info "Для запуска:"
    if [[ "$BUILD_CLIENT" == "ON" ]]; then
        echo "  Клиент: $BUILD_DIR/rtl_433_client --help"
    fi
    if [[ "$BUILD_SERVER" == "ON" ]]; then
        echo "  Сервер: $BUILD_DIR/rtl_433_server --help"
    fi
}

# Запуск основной функции
main "$@"
