#!/bin/bash

# ASN.1 Hex Data Decoder Script
# Usage: ./decode_asn1.sh <PDU_TYPE> <HEX_DATA>
# Example: ./decode_asn1.sh SignalMessage 60000A8290000003D08F00001B80...

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to show usage
show_usage() {
    echo -e "${BLUE}ASN.1 Hex Data Decoder${NC}"
    echo -e "${BLUE}======================${NC}"
    echo ""
    echo -e "${YELLOW}Usage:${NC}"
    echo "  $0 <PDU_TYPE> <HEX_DATA>"
    echo ""
    echo -e "${YELLOW}Available PDU Types:${NC}"
    echo "  RTL433Message     - Main message container (recommended)"
    echo "  SignalMessage     - Raw signal data"
    echo "  DetectedMessage   - Decoded device data"
    echo "  StatusMessage     - System status"
    echo "  ConfigMessage     - Configuration data"
    echo "  PulsesData        - Pulse array data"
    echo "  SignalData        - Signal data wrapper"
    echo "  TimingInfo        - Timing information"
    echo "  SignalQuality     - Signal quality metrics"
    echo ""
    echo -e "${YELLOW}Examples:${NC}"
    echo "  $0 SignalMessage 60000A8290000003D08F00001B80120006800B84E2186A00258032003E804B005780640070807D00898096020CEE8BFF8CEEB18FF01B03A403E8F5C41A03B400ADE3541703BC3B1CAC705D69466C9D78"
    echo ""
    echo "  $0 RTL433Message 180002A0A4000000F423C00006E0048001A002E138861A8009600C800FA012C015E019001C201F4022602580833BA2FFE33BAC63FC06C0E900FA3D710680ED002B78D505C0EF0EC72B1C175A519B275E"
    echo ""
    echo -e "${YELLOW}Options:${NC}"
    echo "  -h, --help        Show this help message"
    echo "  -x, --xml         Output in XML format instead of text"
    echo "  -v, --verbose     Show hex data and size info"
    echo "  -c, --check       Check ASN.1 constraints after decoding"
    echo ""
}

# Check if converter-example exists
check_converter() {
    if [ ! -f "./converter-example" ]; then
        echo -e "${RED}❌ Error: converter-example not found in current directory${NC}"
        echo -e "${YELLOW}💡 Make sure you're in the build directory where converter-example is located${NC}"
        exit 1
    fi
}

# Validate hex data
validate_hex() {
    local hex_data="$1"
    
    # Remove spaces and convert to uppercase
    hex_data=$(echo "$hex_data" | tr -d ' ' | tr '[:lower:]' '[:upper:]')
    
    # Check if it's valid hex (only 0-9, A-F)
    if ! [[ "$hex_data" =~ ^[0-9A-F]+$ ]]; then
        echo -e "${RED}❌ Error: Invalid hex data. Only 0-9 and A-F characters allowed${NC}"
        exit 1
    fi
    
    # Check if length is even (hex pairs)
    if [ $((${#hex_data} % 2)) -ne 0 ]; then
        echo -e "${RED}❌ Error: Hex data length must be even (hex pairs)${NC}"
        exit 1
    fi
    
    echo "$hex_data"
}

# Get available PDU types
get_pdu_types() {
    if [ -f "./converter-example" ]; then
        ./converter-example -p list 2>/dev/null | grep -E "^[A-Z]" | head -20
    fi
}

# Main decode function
decode_asn1() {
    local pdu_type="$1"
    local hex_data="$2"
    local output_format="$3"
    local verbose="$4"
    local check_constraints="$5"
    
    # Validate and clean hex data
    hex_data=$(validate_hex "$hex_data")
    
    # Calculate size
    local byte_count=$((${#hex_data} / 2))
    
    if [ "$verbose" = "true" ]; then
        echo -e "${BLUE}🔍 Decoding Information:${NC}"
        echo -e "  PDU Type: ${YELLOW}$pdu_type${NC}"
        echo -e "  Hex Data: ${YELLOW}$hex_data${NC}"
        echo -e "  Size: ${YELLOW}$byte_count bytes${NC}"
        echo ""
    fi
    
    # Build converter-example command
    local cmd_args="-iper"
    
    if [ "$output_format" = "xml" ]; then
        cmd_args="$cmd_args -oxer"
    else
        cmd_args="$cmd_args -otext"
    fi
    
    if [ "$check_constraints" = "true" ]; then
        cmd_args="$cmd_args -c"
    fi
    
    cmd_args="$cmd_args -p $pdu_type"
    
    # Decode the data
    echo -e "${GREEN}📤 Decoding ASN.1 data...${NC}"
    echo ""
    
    if echo "$hex_data" | xxd -r -p | ./converter-example $cmd_args - 2>/dev/null; then
        echo ""
        echo -e "${GREEN}✅ Decoding successful!${NC}"
    else
        echo -e "${RED}❌ Decoding failed!${NC}"
        echo -e "${YELLOW}💡 Try a different PDU type or check your hex data${NC}"
        echo ""
        echo -e "${YELLOW}Available PDU types:${NC}"
        get_pdu_types
        exit 1
    fi
}

# Parse command line arguments
PDU_TYPE=""
HEX_DATA=""
OUTPUT_FORMAT="text"
VERBOSE="false"
CHECK_CONSTRAINTS="false"

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_usage
            exit 0
            ;;
        -x|--xml)
            OUTPUT_FORMAT="xml"
            shift
            ;;
        -v|--verbose)
            VERBOSE="true"
            shift
            ;;
        -c|--check)
            CHECK_CONSTRAINTS="true"
            shift
            ;;
        *)
            if [ -z "$PDU_TYPE" ]; then
                PDU_TYPE="$1"
            elif [ -z "$HEX_DATA" ]; then
                HEX_DATA="$1"
            else
                echo -e "${RED}❌ Error: Too many arguments${NC}"
                show_usage
                exit 1
            fi
            shift
            ;;
    esac
done

# Check if required arguments are provided
if [ -z "$PDU_TYPE" ] || [ -z "$HEX_DATA" ]; then
    echo -e "${RED}❌ Error: Missing required arguments${NC}"
    echo ""
    show_usage
    exit 1
fi

# Check if converter-example exists
check_converter

# Decode the ASN.1 data
decode_asn1 "$PDU_TYPE" "$HEX_DATA" "$OUTPUT_FORMAT" "$VERBOSE" "$CHECK_CONSTRAINTS"



