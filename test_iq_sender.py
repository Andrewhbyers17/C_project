#!/usr/bin/env python3
"""
Test IQ Data Sender
Generates test IQ signals and sends them over TCP to the FFT analyzer.

Usage:
    python test_iq_sender.py --port 5000 --rate 8000 --signal sine
    python test_iq_sender.py --port 5000 --rate 100000 --signal sweep
    python test_iq_sender.py --port 5000 --rate 2000000 --signal noise
"""

import socket
import struct
import time
import math
import argparse
import sys

def generate_sine_wave(sample_rate, frequency, num_samples):
    """Generate complex sine wave (I/Q samples)"""
    samples = []
    for i in range(num_samples):
        t = i / float(sample_rate)
        # I/Q representation of sine wave
        i_val = math.cos(2 * math.pi * frequency * t)
        q_val = math.sin(2 * math.pi * frequency * t)
        samples.extend([i_val, q_val])
    return samples

def generate_chirp(sample_rate, start_freq, end_freq, num_samples):
    """Generate frequency sweep (chirp)"""
    samples = []
    for i in range(num_samples):
        t = i / float(sample_rate)
        # Linear frequency sweep
        freq = start_freq + (end_freq - start_freq) * t / (num_samples / sample_rate)
        i_val = math.cos(2 * math.pi * freq * t)
        q_val = math.sin(2 * math.pi * freq * t)
        samples.extend([i_val, q_val])
    return samples

def generate_noise(num_samples):
    """Generate white noise"""
    import random
    samples = []
    for _ in range(num_samples):
        i_val = random.gauss(0, 0.3)
        q_val = random.gauss(0, 0.3)
        samples.extend([i_val, q_val])
    return samples

def generate_signal_noise(sample_rate, frequency, num_samples, snr_db=10):
    """Generate sine wave with additive noise"""
    import random
    signal_power = 1.0
    noise_power = signal_power / (10 ** (snr_db / 10.0))
    noise_amplitude = math.sqrt(noise_power)

    samples = []
    for i in range(num_samples):
        t = i / float(sample_rate)
        # Signal
        i_signal = math.cos(2 * math.pi * frequency * t)
        q_signal = math.sin(2 * math.pi * frequency * t)
        # Add noise
        i_val = i_signal + random.gauss(0, noise_amplitude)
        q_val = q_signal + random.gauss(0, noise_amplitude)
        samples.extend([i_val, q_val])
    return samples

def generate_multi_tone(sample_rate, frequencies, num_samples):
    """Generate multiple sine waves combined"""
    samples = []
    for i in range(num_samples):
        t = i / float(sample_rate)
        i_val = 0.0
        q_val = 0.0
        for freq in frequencies:
            i_val += math.cos(2 * math.pi * freq * t) / len(frequencies)
            q_val += math.sin(2 * math.pi * freq * t) / len(frequencies)
        samples.extend([i_val, q_val])
    return samples

def send_iq_data(host, port, sample_rate, signal_type, duration=None):
    """Send IQ data to FFT analyzer"""

    # Create socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    print(f"[*] Connecting to {host}:{port}...")
    try:
        sock.connect((host, port))
        print(f"[OK] Connected!")
    except Exception as e:
        print(f"[ERROR] Connection failed: {e}")
        return

    print(f"[*] Sample rate: {sample_rate} Hz")
    print(f"[*] Signal type: {signal_type}")
    if duration:
        print(f"[*] Duration: {duration} seconds")
    else:
        print(f"[*] Duration: Continuous (Ctrl+C to stop)")

    # Calculate chunk size (send 512 I/Q pairs = 1024 floats at a time)
    chunk_samples = 512
    chunk_rate = sample_rate / chunk_samples  # Chunks per second
    chunk_delay = 1.0 / chunk_rate  # Delay between chunks

    print(f"[*] Sending {chunk_samples} I/Q pairs per chunk")
    print(f"[*] Chunk rate: {chunk_rate:.2f} Hz (delay: {chunk_delay*1000:.2f}ms)")
    print(f"[*] Data rate: {sample_rate * 8 / 1024 / 1024:.2f} MB/s")
    print("")

    start_time = time.time()
    total_samples = 0
    chunk_count = 0

    try:
        while True:
            # Generate signal chunk
            if signal_type == 'sine':
                # 1 kHz sine wave
                samples = generate_sine_wave(sample_rate, 1000, chunk_samples)
            elif signal_type == 'sweep':
                # Frequency sweep from 100 Hz to 4 kHz
                samples = generate_chirp(sample_rate, 100, 4000, chunk_samples)
            elif signal_type == 'noise':
                samples = generate_noise(chunk_samples)
            elif signal_type == 'signal_noise':
                # 1 kHz signal with 10 dB SNR
                samples = generate_signal_noise(sample_rate, 1000, chunk_samples, 10)
            elif signal_type == 'multi':
                # Three tones: 500 Hz, 1 kHz, 2 kHz
                samples = generate_multi_tone(sample_rate, [500, 1000, 2000], chunk_samples)
            else:
                print(f"[ERROR] Unknown signal type: {signal_type}")
                break

            # Pack as floats (little endian)
            data = struct.pack('<%df' % len(samples), *samples)

            # Send chunk
            sock.sendall(data)

            total_samples += chunk_samples
            chunk_count += 1

            # Print status every second
            if chunk_count % int(chunk_rate) == 0:
                elapsed = time.time() - start_time
                actual_rate = total_samples / elapsed
                print(f"[*] Sent {total_samples} I/Q pairs ({elapsed:.1f}s) - {actual_rate:.0f} samples/sec")

            # Check duration limit
            if duration and (time.time() - start_time) >= duration:
                print(f"[OK] Sent {total_samples} I/Q pairs in {duration} seconds")
                break

            # Rate limiting
            time.sleep(chunk_delay)

    except KeyboardInterrupt:
        elapsed = time.time() - start_time
        print(f"\n[*] Interrupted by user")
        print(f"[OK] Sent {total_samples} I/Q pairs in {elapsed:.1f} seconds")
    except Exception as e:
        print(f"\n[ERROR] {e}")
    finally:
        sock.close()
        print("[*] Connection closed")

def main():
    parser = argparse.ArgumentParser(description='Test IQ Data Sender for FFT Analyzer')
    parser.add_argument('--host', default='127.0.0.1', help='Host to connect to (default: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=5000, help='Port to connect to (default: 5000)')
    parser.add_argument('--rate', type=int, default=8000, help='Sample rate in Hz (default: 8000)')
    parser.add_argument('--signal', choices=['sine', 'sweep', 'noise', 'signal_noise', 'multi'],
                        default='sine', help='Signal type (default: sine)')
    parser.add_argument('--duration', type=float, help='Duration in seconds (default: continuous)')

    args = parser.parse_args()

    send_iq_data(args.host, args.port, args.rate, args.signal, args.duration)

if __name__ == '__main__':
    main()
