#!/usr/bin/env python3
"""
Utility for creating triq.org links from RabbitMQ messages
Usage: python3 rabbitmq_to_triq.py '<json_message>'
"""

import json
import sys

def create_histogram(values, tolerance=0.2):
    """Creates histogram with tolerance"""
    if not values:
        return []
        
    sorted_values = sorted(values)
    bins = []
    
    for value in sorted_values:
        found_bin = False
        for i, (bin_mean, bin_count, bin_min, bin_max) in enumerate(bins):
            if abs(value - bin_mean) <= bin_mean * tolerance:
                new_count = bin_count + 1
                new_mean = (bin_mean * bin_count + value) / new_count
                new_min = min(bin_min, value)
                new_max = max(bin_max, value)
                bins[i] = (new_mean, new_count, new_min, new_max)
                found_bin = True
                break
        
        if not found_bin:
            bins.append((value, 1, value, value))
    
    bins.sort(key=lambda x: x[0])
    return bins

def find_timing_bin_index(value, timing_bins, tolerance=0.2):
    """Finds bin index for value"""
    for i, (bin_mean, _, _, _) in enumerate(timing_bins):
        if abs(value - bin_mean) <= bin_mean * tolerance:
            return i
    return -1

def generate_rfraw_from_rabbitmq(json_data):
    """Generates RfRaw string from RabbitMQ message"""
    
    # Extract data
    pulses = json_data.get('pulses', [])
    rate_hz = json_data.get('rate_Hz', 250000)
    mod_type = json_data.get('mod', 'Unknown')
    count = json_data.get('count', 0)
    
    if not pulses or len(pulses) < 2:
        print("❌ Insufficient pulse data")
        return None
    
    print(f"📡 Modulation: {mod_type}")
    print(f"📊 Pulses: {count}")
    print(f"🔄 Rate: {rate_hz} Hz")
    
    # Separate into pulses and gaps
    pulse_widths = []
    gap_widths = []
    
    to_us = 1e6 / rate_hz
    
    for i in range(0, len(pulses), 2):
        if i < len(pulses):
            pulse_widths.append(pulses[i])
        if i + 1 < len(pulses):
            gap_widths.append(pulses[i + 1])
    
    # Create histogram of all timings
    all_timings = pulse_widths + gap_widths
    timing_bins = create_histogram(all_timings)
    
    if len(timing_bins) > 8:
        print(f"❌ Too many timings ({len(timing_bins)} > 8) for RfRaw B1")
        return None
    
    print(f"📈 Timings: {len(timing_bins)}")
    
    # Generate RfRaw B1
    hex_parts = []
    
    # Header
    hex_parts.append("AA")
    hex_parts.append("B1")
    hex_parts.append(f"{len(timing_bins):02X}")
    
    # Timing table
    for timing, _, _, _ in timing_bins:
        timing_us = int(timing * to_us)
        timing_us = min(timing_us, 65535)  # USHRT_MAX
        hex_parts.append(f"{timing_us:04X}")
    
    # Encode pulses
    for i in range(len(pulse_widths)):
        pulse = pulse_widths[i]
        gap = gap_widths[i] if i < len(gap_widths) else 0
        
        pulse_idx = find_timing_bin_index(pulse, timing_bins)
        gap_idx = find_timing_bin_index(gap, timing_bins)
        
        if pulse_idx >= 0 and gap_idx >= 0:
            encoded = 0x80 | (pulse_idx << 4) | gap_idx
            hex_parts.append(f"{encoded:02X}")
    
    # End byte
    hex_parts.append("55")
    
    rfraw = "".join(hex_parts)
    
    print(f"✅ RfRaw: {rfraw}")
    print(f"🌐 Link: https://triq.org/pdv/#{rfraw}")
    
    return rfraw

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 rabbitmq_to_triq.py '<json_message>'")
        print()
        print("Examples:")
        print('python3 rabbitmq_to_triq.py \'{"mod":"OOK","count":1,"pulses":[2396,23964],"rate_Hz":250000}\'')
        print()
        print("Supported JSON fields:")
        print("- mod: modulation type (OOK/FSK)")
        print("- count: number of pulses")
        print("- pulses: array [pulse1, gap1, pulse2, gap2, ...]")
        print("- rate_Hz: sample rate (default 250000)")
        print("- freq_Hz: center frequency")
        print("- rssi_dB, snr_dB, noise_dB: signal parameters")
        sys.exit(1)
    
    try:
        json_data = json.loads(sys.argv[1])
        print("🔄 Processing RabbitMQ message...")
        print("=" * 50)
        
        rfraw = generate_rfraw_from_rabbitmq(json_data)
        
        if rfraw:
            print("=" * 50)
            print("🎯 DONE! Copy the link above for analysis on triq.org")
        else:
            print("❌ Failed to create RfRaw string")
            
    except json.JSONDecodeError as e:
        print(f"❌ JSON Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"❌ Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
