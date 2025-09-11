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
    echo "RTL_433 Split Architecture Build Script for WSL Ubuntu 22.04"
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
    echo "  --setup-deps            Install dependencies"
    echo "  --setup-usb             Setup USB for RTL-SDR"
    echo ""
    echo "Examples:"
    echo "  $0 --setup-deps         Install all required dependencies"
    echo "  $0                      Basic build"
    echo "  $0 --clean --install    Rebuild and install"
    echo "  $0 --with-rabbitmq      Build with RabbitMQ support"
    echo "  $0 --no-server -j 8     Client only, 8 threads"
}

# Parse command line arguments
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
        --setup-deps)
            SETUP_DEPS=1
            shift
            ;;
        --setup-usb)
            SETUP_USB=1
            shift
            ;;
        *)
            print_error "Unknown parameter: $1"
            show_help
            exit 1
            ;;
    esac
done

# Setup dependencies for WSL Ubuntu 22.04
setup_dependencies() {
    print_info "Setting up dependencies for WSL Ubuntu 22.04..."
    
    # Update package list
    print_info "Updating package list..."
    sudo apt update
    
    # Install essential build tools
    print_info "Installing build tools..."
    sudo apt install -y build-essential cmake pkg-config git
    
    # Install JSON-C (required for both client and server)
    print_info "Installing json-c..."
    sudo apt install -y libjson-c-dev
    
    # Client dependencies
    if [[ "$BUILD_CLIENT" == "ON" ]]; then
        print_info "Installing client dependencies..."
        
        # RTL-SDR
        sudo apt install -y librtlsdr-dev rtl-sdr
        
        # libcurl for HTTP transport
        sudo apt install -y libcurl4-openssl-dev
        
        # Optional: SoapySDR
        sudo apt install -y libsoapysdr-dev soapysdr-tools
        
        # MQTT support
        if [[ "$ENABLE_MQTT" == "ON" ]]; then
            sudo apt install -y libpaho-mqtt-dev
        fi
        
        # RabbitMQ support
        if [[ "$ENABLE_RABBITMQ" == "ON" ]]; then
            sudo apt install -y librabbitmq-dev
        fi
    fi
    
    # Server dependencies
    if [[ "$BUILD_SERVER" == "ON" ]]; then
        print_info "Installing server dependencies..."
        
        # SQLite for unknown signals database
        sudo apt install -y libsqlite3-dev sqlite3
        
        # MQTT support
        if [[ "$ENABLE_MQTT" == "ON" ]]; then
            sudo apt install -y libpaho-mqtt-dev
        fi
        
        # RabbitMQ support
        if [[ "$ENABLE_RABBITMQ" == "ON" ]]; then
            sudo apt install -y librabbitmq-dev
        fi
    fi
    
    print_success "Dependencies installed successfully"
}

# Setup USB for RTL-SDR in WSL
setup_usb() {
    print_info "Setting up USB support for RTL-SDR in WSL..."
    
    # Install USB utilities
    sudo apt install -y usbutils
    
    # Add user to dialout group for USB device access
    print_info "Adding user to dialout group..."
    sudo usermod -a -G dialout $USER
    
    # Create udev rules for RTL-SDR
    print_info "Creating udev rules for RTL-SDR..."
    echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="0bda", ATTRS{idProduct}=="2832", GROUP="dialout", MODE="0664"' | sudo tee /etc/udev/rules.d/20-rtlsdr.rules
    echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="0bda", ATTRS{idProduct}=="2838", GROUP="dialout", MODE="0664"' | sudo tee -a /etc/udev/rules.d/20-rtlsdr.rules
    
    # Reload udev rules
    sudo udevadm control --reload-rules
    sudo udevadm trigger
    
    # Create blacklist for DVB driver
    echo 'blacklist dvb_usb_rtl28xxu' | sudo tee /etc/modprobe.d/blacklist-rtl.conf
    
    print_warning "You need to:"
    print_warning "1. Install usbipd on Windows: winget install usbipd"
    print_warning "2. Attach RTL-SDR device: usbipd wsl attach --busid <busid>"
    print_warning "3. Logout and login again to apply group changes"
    
    print_success "USB setup completed"
}

# Check dependencies
check_dependencies() {
    print_info "Checking dependencies..."
    
    # Essential tools
    local deps=("cmake" "make" "gcc" "pkg-config")
    
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            print_error "Required dependency not found: $dep"
            print_info "Run with --setup-deps to install dependencies"
            exit 1
        fi
    done
    
    # Check libraries
    if [[ "$BUILD_CLIENT" == "ON" ]]; then
        if ! pkg-config --exists libcurl; then
            print_error "libcurl not found"
            print_info "Run with --setup-deps to install dependencies"
            exit 1
        fi
        
        if ! pkg-config --exists librtlsdr; then
            print_warning "librtlsdr not found - client will be built without RTL-SDR support"
        fi
    fi
    
    if [[ "$BUILD_SERVER" == "ON" ]]; then
        if ! pkg-config --exists sqlite3; then
            print_error "sqlite3 not found"
            print_info "Run with --setup-deps to install dependencies"
            exit 1
        fi
    fi
    
    if ! pkg-config --exists json-c; then
        print_error "json-c not found"
        print_info "Run with --setup-deps to install dependencies"
        exit 1
    fi
    
    if [[ "$ENABLE_MQTT" == "ON" ]]; then
        if ! pkg-config --exists libpaho-mqtt3c; then
            print_warning "paho-mqtt not found - MQTT support will be disabled"
            ENABLE_MQTT=OFF
        fi
    fi
    
    print_success "All dependencies found"
}

# Clean build directory
clean_build() {
    if [[ -d "$BUILD_DIR" ]]; then
        print_info "Cleaning build directory: $BUILD_DIR"
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned"
    fi
}

# Configure project
configure_project() {
    print_info "Configuring project..."
    
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
    
    print_info "Executing: cmake ${cmake_args[*]}"
    
    if cmake "${cmake_args[@]}"; then
        print_success "Configuration completed successfully"
    else
        print_error "Configuration failed"
        exit 1
    fi
    
    cd ..
}

# Build project
build_project() {
    print_info "Building project..."
    
    cd "$BUILD_DIR"
    
    if make -j"$JOBS"; then
        print_success "Build completed successfully"
    else
        print_error "Build failed"
        exit 1
    fi
    
    cd ..
}

# Install project
install_project() {
    print_info "Installing project..."
    
    cd "$BUILD_DIR"
    
    if [[ "$INSTALL_PREFIX" == "/usr/local" ]] || [[ "$INSTALL_PREFIX" =~ ^/usr ]]; then
        if sudo make install; then
            print_success "Installation completed successfully"
        else
            print_error "Installation failed"
            exit 1
        fi
    else
        if make install; then
            print_success "Installation completed successfully"
        else
            print_error "Installation failed"
            exit 1
        fi
    fi
    
    cd ..
}

# Create package
create_package() {
    print_info "Creating package..."
    
    cd "$BUILD_DIR"
    
    if make package; then
        print_success "Package created successfully"
        ls -la *.deb *.tar.gz 2>/dev/null || true
    else
        print_error "Package creation failed"
        exit 1
    fi
    
    cd ..
}

# Print build information
print_build_info() {
    print_info "Build configuration:"
    echo "  Build directory: $BUILD_DIR"
    echo "  Build type: $BUILD_TYPE"
    echo "  Install prefix: $INSTALL_PREFIX"
    echo "  Parallel jobs: $JOBS"
    echo "  Build client: $BUILD_CLIENT"
    echo "  Build server: $BUILD_SERVER"
    echo "  Build original rtl_433: $BUILD_ORIGINAL"
    echo "  MQTT support: $ENABLE_MQTT"
    echo "  RabbitMQ support: $ENABLE_RABBITMQ"
    echo "  WSL Ubuntu version: $(lsb_release -d | cut -f2)"
    echo ""
}

# Check WSL environment
check_wsl() {
    if [[ ! -f /proc/version ]] || ! grep -q Microsoft /proc/version; then
        print_warning "This script is optimized for WSL. You may need to adjust settings for native Linux."
    else
        print_info "Running on WSL environment"
    fi
}

# Main function
main() {
    print_info "RTL_433 Split Architecture Build Script for WSL Ubuntu 22.04"
    
    check_wsl
    
    # Handle special options first
    if [[ -n "$SETUP_DEPS" ]]; then
        setup_dependencies
        exit 0
    fi
    
    if [[ -n "$SETUP_USB" ]]; then
        setup_usb
        exit 0
    fi
    
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
    
    # Show created files
    print_info "Created executable files:"
    if [[ "$BUILD_CLIENT" == "ON" ]] && [[ -f "$BUILD_DIR/rtl_433_client" ]]; then
        echo "  rtl_433_client: $(realpath $BUILD_DIR/rtl_433_client)"
    fi
    if [[ "$BUILD_SERVER" == "ON" ]] && [[ -f "$BUILD_DIR/rtl_433_server" ]]; then
        echo "  rtl_433_server: $(realpath $BUILD_DIR/rtl_433_server)"
    fi
    if [[ "$BUILD_ORIGINAL" == "ON" ]] && [[ -f "$BUILD_DIR/src/rtl_433" ]]; then
        echo "  rtl_433: $(realpath $BUILD_DIR/src/rtl_433)"
    fi
    
    print_info "To run:"
    if [[ "$BUILD_CLIENT" == "ON" ]]; then
        echo "  Client: $BUILD_DIR/rtl_433_client --help"
    fi
    if [[ "$BUILD_SERVER" == "ON" ]]; then
        echo "  Server: $BUILD_DIR/rtl_433_server --help"
    fi
    
    print_info "Next steps for WSL:"
    echo "  1. Setup USB: $0 --setup-usb"
    echo "  2. Install usbipd on Windows: winget install usbipd"
    echo "  3. Attach RTL-SDR: usbipd wsl attach --busid <busid>"
    echo "  4. Test RTL-SDR: rtl_test"
}

# Run main function
main "$@"



