#!/usr/bin/env python3
"""
Pulse data analyzer in rtl_433 pulse_analyzer style
Reproduces logic from src/pulse_analyzer.c
"""

import json
import sys
from collections import defaultdict

class PulseDataAnalyzer:
    def __init__(self, pulse_data):
        self.data = pulse_data
        self.sample_rate = pulse_data.get('rate_Hz', 250000)
        self.to_us = 1e6 / self.sample_rate
        self.to_ms = 1e3 / self.sample_rate
        self.TOLERANCE = 0.2  # 20% tolerance
        
    def analyze(self):
        """Main analysis function like in pulse_analyzer.c"""
        pulses = self.data.get('pulses', [])
        if len(pulses) < 2:
            print("Error: Need at least pulse and gap data")
            return
            
        # Separate into pulses and gaps (as in original)
        pulse_widths = pulses[::2]  # even indices - pulses
        gap_widths = pulses[1::2]   # odd indices - gaps
        
        num_pulses = len(pulse_widths)
        
        if num_pulses == 0:
            print("No pulses detected.")
            return
            
        print("Analyzing pulses...")
        
        # General statistics
        total_period = sum(pulses[:-1])  # exclude last gap
        print(f"Total count: {num_pulses:4d},  width: {total_period * self.to_ms:4.2f} ms\t\t({total_period:5d} S)")
        
        # Distribution analysis
        self._analyze_distribution("Pulse width", pulse_widths)
        self._analyze_distribution("Gap width", gap_widths)
        
        # Period = pulse + gap
        periods_pg = []
        for i in range(min(len(pulse_widths), len(gap_widths))):
            periods_pg.append(pulse_widths[i] + gap_widths[i])
        self._analyze_distribution("Pulse+gap period", periods_pg)
        
        # Level estimates (if available)
        if 'ook_high' in self.data and 'ook_low' in self.data:
            print(f"Level estimates [high, low]: {self.data['ook_high']:6d}, {self.data['ook_low']:6d}")
        
        # RSSI/SNR information
        rssi = self.data.get('rssi_dB', 0)
        snr = self.data.get('snr_dB', 0) 
        noise = self.data.get('noise_dB', 0)
        print(f"RSSI: {rssi:.1f} dB SNR: {snr:.1f} dB Noise: {noise:.1f} dB")
        
        # Frequency offsets
        freq1 = self.data.get('freq1_Hz', 0)
        freq = self.data.get('freq_Hz', 433920000)
        f1_offset = freq1 - freq if freq1 else 0
        print(f"Frequency offsets [F1, F2]: {f1_offset:6d}, 0\t({f1_offset/1000:+.1f} kHz, +0.0 kHz)")
        
        # Modulation type detection
        self._guess_modulation(pulse_widths, gap_widths)
        
        # RfRaw data generation
        self._generate_rfraw(pulses)
        
    def _analyze_distribution(self, name, values):
        """Analysis of value distribution"""
        if not values:
            print(f"{name} distribution: No data")
            return
            
        print(f"{name} distribution:")
        
        # Value grouping with tolerance
        bins = self._create_histogram(values)
        
        for i, (value, count, min_val, max_val) in enumerate(bins):
            us_value = value * self.to_us
            us_min = min_val * self.to_us  
            us_max = max_val * self.to_us
            samples = value
            
            if min_val == max_val:
                print(f" [{i:2d}] count: {count:4d},  width: {us_value:4.0f} us [{us_min:.0f};{us_max:.0f}]\t({int(samples):4d} S)")
            else:
                print(f" [{i:2d}] count: {count:4d},  width: {us_value:4.0f} us [{us_min:.0f};{us_max:.0f}]\t({int(samples):4d} S)")
    
    def _create_histogram(self, values):
        """Create histogram with tolerance like in rtl_433"""
        if not values:
            return []
            
        # Sort values
        sorted_values = sorted(values)
        bins = []
        
        for value in sorted_values:
            # Find suitable bin
            found_bin = False
            for i, (bin_mean, bin_count, bin_min, bin_max) in enumerate(bins):
                if abs(value - bin_mean) <= bin_mean * self.TOLERANCE:
                    # Update bin
                    new_count = bin_count + 1
                    new_mean = (bin_mean * bin_count + value) / new_count
                    new_min = min(bin_min, value)
                    new_max = max(bin_max, value)
                    bins[i] = (new_mean, new_count, new_min, new_max)
                    found_bin = True
                    break
            
            if not found_bin:
                # Create new bin
                bins.append((value, 1, value, value))
        
        return bins
    
    def _guess_modulation(self, pulse_widths, gap_widths):
        """Modulation type detection like in pulse_analyzer.c"""
        print("Guessing modulation: ", end="")
        
        pulse_bins = self._create_histogram(pulse_widths)
        gap_bins = self._create_histogram(gap_widths)
        
        pulse_count = len(pulse_bins)
        gap_count = len(gap_bins)
        num_pulses = len(pulse_widths)
        
        if num_pulses == 1:
            print("Single pulse detected. Probably Frequency Shift Keying or just noise...")
            return
        elif pulse_count == 1 and gap_count == 1:
            print("Un-modulated signal. Maybe a preamble...")
            return
        elif pulse_count == 1 and gap_count > 1:
            print("Pulse Position Modulation with fixed pulse width")
            self._suggest_flex_decoder("OOK_PPM", pulse_bins, gap_bins)
            return
        elif pulse_count == 2 and gap_count == 1:
            print("Pulse Width Modulation with fixed gap")
            self._suggest_flex_decoder("OOK_PWM", pulse_bins, gap_bins)
            return
        elif pulse_count == 2 and gap_count == 2:
            print("Pulse Width Modulation with fixed period")
            self._suggest_flex_decoder("OOK_PWM", pulse_bins, gap_bins)
            return
        elif pulse_count == 3:
            print("Pulse Width Modulation with sync/delimiter")
            self._suggest_flex_decoder("OOK_PWM", pulse_bins, gap_bins, has_sync=True)
            return
        else:
            print("No clue...")
    
    def _suggest_flex_decoder(self, modulation, pulse_bins, gap_bins, has_sync=False):
        """Flex decoder command generation"""
        if not pulse_bins:
            return
            
        # Sort by average values
        pulse_bins.sort(key=lambda x: x[0])
        gap_bins.sort(key=lambda x: x[0])
        
        if modulation == "OOK_PPM":
            short_width = gap_bins[0][0] * self.to_us
            long_width = gap_bins[1][0] * self.to_us if len(gap_bins) > 1 else short_width * 2
            gap_limit = long_width + 100
            reset_limit = gap_bins[-1][2] * self.to_us + 100  # max + margin
            
            print(f"Attempting demodulation... short_width: {short_width:.0f}, long_width: {long_width:.0f}, reset_limit: {reset_limit:.0f}")
            print(f"Use a flex decoder with -X 'n=name,m={modulation},s={short_width:.0f},l={long_width:.0f},g={gap_limit:.0f},r={reset_limit:.0f}'")
            
        elif modulation == "OOK_PWM":
            if has_sync and len(pulse_bins) >= 3:
                # Sort by count (sync has least count)
                pulse_bins.sort(key=lambda x: x[1])
                sync_width = pulse_bins[0][0] * self.to_us
                
                # Remaining two - short and long
                p1 = pulse_bins[1][0] * self.to_us
                p2 = pulse_bins[2][0] * self.to_us
                short_width = min(p1, p2)
                long_width = max(p1, p2)
            else:
                short_width = pulse_bins[0][0] * self.to_us
                long_width = pulse_bins[1][0] * self.to_us if len(pulse_bins) > 1 else short_width * 2
                sync_width = 0
            
            reset_limit = gap_bins[-1][2] * self.to_us + 100 if gap_bins else 10000
            tolerance = abs(long_width - short_width) * 0.4
            
            print(f"Attempting demodulation... short_width: {short_width:.0f}, long_width: {long_width:.0f}, reset_limit: {reset_limit:.0f}", end="")
            if has_sync:
                print(f", sync_width: {sync_width:.0f}")
                print(f"Use a flex decoder with -X 'n=name,m={modulation},s={short_width:.0f},l={long_width:.0f},r={reset_limit:.0f},g=0,t={tolerance:.0f},y={sync_width:.0f}'")
            else:
                print()
                print(f"Use a flex decoder with -X 'n=name,m={modulation},s={short_width:.0f},l={long_width:.0f},r={reset_limit:.0f}'")
    
    def _generate_rfraw(self, pulses):
        """RfRaw data generation for triq.org (simplified version)"""
        if len(pulses) > 16:  # Too many for simple RfRaw
            return
            
        # Simple version - all timings in microseconds
        print("Simplified RfRaw data:")
        rfraw_data = []
        for i, pulse in enumerate(pulses):
            us_value = int(pulse * self.to_us)
            rfraw_data.append(us_value)
            
        print(f"Timings (μs): {rfraw_data}")

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 pulse_data_analyzer.py '<json_data>'")
        print("Example: python3 pulse_data_analyzer.py '{\"mod\":\"OOK\",\"count\":1,\"pulses\":[2396,23964],...}'")
        sys.exit(1)
    
    try:
        pulse_data = json.loads(sys.argv[1])
        analyzer = PulseDataAnalyzer(pulse_data)
        analyzer.analyze()
    except json.JSONDecodeError as e:
        print(f"JSON decode error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Analysis error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
