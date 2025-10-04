# RTL433 ASN.1 Library - Automated Setup

This directory contains the ASN.1 library for RTL433 with **fully automated setup**. No manual generation of ASN.1 files is required!

## Quick Start

### Automatic Setup (Recommended)

The build system will automatically:
1. Detect and install `asn1c` compiler if missing
2. Generate C code from ASN.1 schema
3. Build the library

Just run:
```bash
# From project root
mkdir build && cd build
cmake .. -DENABLE_ASN1=ON
make
```

### Manual Setup Scripts

If you prefer manual control, use our setup scripts:

#### Universal Script (Cross-Platform)
```bash
# Full setup
python3 asn1/setup_asn1_universal.py

# Install asn1c only
python3 asn1/setup_asn1_universal.py --install-only

# Generate code only
python3 asn1/setup_asn1_universal.py --generate-only

# Build library only
python3 asn1/setup_asn1_universal.py --build-only
```

## Supported Platforms

### Linux
- **Ubuntu/Debian**: `apt-get install asn1c`
- **CentOS/RHEL**: `yum install asn1c`
- **Fedora**: `dnf install asn1c`
- **Arch Linux**: `pacman -S asn1c`
- **openSUSE**: `zypper install asn1c`

### macOS
- **Homebrew**: `brew install asn1c`

### Windows
- **vcpkg**: `vcpkg install asn1c`
- **Chocolatey**: `choco install asn1c`
- **MSYS2**: `pacman -S asn1c`
- **WSL**: Use Linux instructions

## What Gets Generated

The ASN.1 schema `rtl433-rabbitmq.asn1` generates:

### Core Message Types
- `RTL433Message` - Main message wrapper
- `SignalMessage` - Raw signal data
- `DetectedMessage` - Decoded device data
- `StatusMessage` - System status
- `ConfigMessage` - Configuration data

### Support Files
- ASN.1 runtime library (~50 C files)
- Basic ASN.1 types (INTEGER, BOOLEAN, etc.)
- Encoding/decoding functions (PER, XER, etc.)

### Generated Library
- **Static library**: `libr_asn1.a` (Unix) or `r_asn1.lib` (Windows)
- **Headers**: All `.h` files in `asn1-generated/`
- **Examples**: `converter-example.c`

## CMake Integration

### In Your Project
```cmake
# Enable ASN.1 support
set(ENABLE_ASN1 ON)
add_subdirectory(asn1)

# Link with your target
target_link_libraries(your_target r_asn1)
target_include_directories(your_target PRIVATE ${ASN1_INCLUDE_DIRS})
```

### Available CMake Targets
```bash
make asn1_generate    # Generate ASN.1 code only
make asn1_validate    # Validate schema only
make asn1_clean       # Clean generated files
make asn1_stats       # Show generation statistics
make r_asn1           # Build library
```

## Usage Example

```c
#include "RTL433Message.h"

// Create a signal message
SignalMessage_t *signal = calloc(1, sizeof(SignalMessage_t));

// Set frequency
signal->frequency.centerFreq = 433920000;  // 433.92 MHz

// Set signal data (hex string)
OCTET_STRING_fromBuf(&signal->signalData.choice.hexString, 
                     data_bytes, data_length);

// Encode to binary
asn_enc_rval_t er = per_encode_to_buffer(&asn_DEF_SignalMessage,
                                        NULL, signal, buffer, buffer_size);

// Decode from binary
SignalMessage_t *decoded = 0;
asn_dec_rval_t dr = per_decode(NULL, &asn_DEF_SignalMessage,
                              (void**)&decoded, buffer, buffer_size);
```

## Troubleshooting

### Common Issues

**Issue**: `asn1c not found`
**Solution**: Run setup script or install manually:
```bash
python3 asn1/setup_asn1_universal.py --install-only
```

**Issue**: Permission denied during installation
**Solution**: Run with sudo or use user-local installation:
```bash
# For Homebrew on macOS
brew install asn1c

# For vcpkg on Windows
vcpkg install asn1c
```

**Issue**: Generated files missing
**Solution**: Clean and regenerate:
```bash
make asn1_clean
make asn1_generate
```

**Issue**: Compilation errors
**Solution**: Check asn1c version and regenerate:
```bash
asn1c -h  # Check version
make asn1_clean
make asn1_generate
```

### Manual Installation

If automatic installation fails, install `asn1c` manually:

#### From Package Manager
```bash
# Ubuntu/Debian
sudo apt-get install asn1c build-essential cmake

# CentOS/RHEL
sudo yum install asn1c gcc cmake make

# macOS
brew install asn1c cmake

# Windows (vcpkg)
vcpkg install asn1c
```

#### From Source
```bash
git clone https://github.com/vlm/asn1c.git
cd asn1c
autoreconf -iv
./configure --prefix=/usr/local
make
sudo make install
```

## Directory Structure

```
asn1/
├── rtl433-rabbitmq.asn1          # ASN.1 schema definition
├── CMakeLists.txt                # CMake build configuration
├── Makefile.asn1                 # Alternative Make build
├── setup_asn1_universal.py       # Universal setup script
├── setup_asn1.sh                 # Unix setup script
├── setup_asn1.bat                # Windows setup script
├── cmake/
│   └── count_files.cmake         # File counting utility
├── asn1-generated/               # Generated C code (auto-created)
│   ├── RTL433Message.h
│   ├── RTL433Message.c
│   ├── SignalMessage.h
│   ├── SignalMessage.c
│   └── ... (50+ generated files)
└── build/                        # Build directory (auto-created)
    └── libr_asn1.a               # Generated library
```

## Performance

The ASN.1 binary encoding is highly efficient:

- **Size**: ~60-80% smaller than JSON
- **Speed**: ~3-5x faster encoding/decoding than JSON
- **Validation**: Built-in schema validation
- **Compatibility**: Cross-platform binary format

## Schema Information

The ASN.1 schema supports:

### Message Types
1. **SIGNALS Queue**: Raw signal data for reconstruction
2. **DETECTED Queue**: Decoded device information  
3. **STATUS Queue**: System status and health
4. **CONFIG Queue**: Configuration and control

### Signal Data Formats
- **Hex String**: Raw bytes from rtl_433 output
- **Hex Strings Array**: Multiple signal sequences
- **Pulses Array**: Timing data for complex signals

### RF Parameters
- Center frequency, bandwidth
- FSK frequencies (F1, F2)
- Modulation type (OOK, FSK, ASK, PSK, QAM)
- Signal quality metrics

## License

This ASN.1 implementation follows the same license as the main RTL433 project.

## Support

For issues with ASN.1 setup:
1. Check this README
2. Run diagnostic: `python3 asn1/setup_asn1_universal.py --help`
3. Check CMake output for detailed error messages
4. Open an issue with full error output
