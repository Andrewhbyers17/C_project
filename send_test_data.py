#!/usr/bin/env python3
"""
Network Data Sender for FFT Analyzer
Sends test signals over TCP or UDP to fft_analyzer_network

Usage:
    python send_test_data.py --port 5000 --protocol tcp --signal sine
    python send_test_data.py --port 5000 --protocol udp --signal chirp
"""

import socket
import struct
import numpy as np
import time
import argparse
import sys

# Configuration
SAMPLE_RATE = 8000
FFT_SIZE = 512
UPDATE_RATE = 0.1  # 100ms between frames (10 Hz)

class SignalGenerator:
    """Generate various test signals"""

    def __init__(self, sample_rate=SAMPLE_RATE):
        self.sample_rate = sample_rate
        self.phase = 0.0
        self.chirp_freq = 500.0

    def sine(self, samples, freq=1000.0, amplitude=0.5):
        """Generate sine wave"""
        t = np.arange(samples) / self.sample_rate
        return amplitude * np.sin(2 * np.pi * freq * t)

    def multi_tone(self, samples):
        """Generate multiple sine waves"""
        t = np.arange(samples) / self.sample_rate
        sig = 0.3 * np.sin(2 * np.pi * 440 * t)
        sig += 0.2 * np.sin(2 * np.pi * 880 * t)
        sig += 0.15 * np.sin(2 * np.pi * 1320 * t)
        return sig

    def chirp(self, samples):
        """Generate linear chirp (LFM)"""
        f0, f1 = 500, 2500  # Start and end frequency
        t = np.arange(samples) / self.sample_rate
        # Linear frequency sweep
        freq = f0 + (f1 - f0) * t / (samples / self.sample_rate)
        # Integrate frequency to get phase
        phase = 2 * np.pi * np.cumsum(freq) / self.sample_rate
        return 0.8 * np.sin(phase)

    def noise(self, samples):
        """Generate white Gaussian noise"""
        return 0.3 * np.random.randn(samples)

    def signal_plus_noise(self, samples, freq=1000.0, snr_db=6.0):
        """Generate sine wave with additive noise"""
        signal = 0.5 * np.sin(2 * np.pi * freq * np.arange(samples) / self.sample_rate)
        noise_power = 0.5**2 / (10**(snr_db/10))
        noise = np.sqrt(noise_power) * np.random.randn(samples)
        return signal + noise

    def impulse_train(self, samples, rate=100):
        """Generate periodic impulses"""
        sig = np.zeros(samples)
        impulse_spacing = self.sample_rate // rate
        for i in range(0, samples, impulse_spacing):
            sig[i] = 1.0
        return sig

    def square_wave(self, samples, freq=440.0):
        """Generate square wave"""
        t = np.arange(samples) / self.sample_rate
        return 0.5 * np.sign(np.sin(2 * np.pi * freq * t))

    def sawtooth(self, samples, freq=440.0):
        """Generate sawtooth wave"""
        t = np.arange(samples) / self.sample_rate
        return 0.5 * (2 * (t * freq - np.floor(t * freq + 0.5)))

    def am_modulation(self, samples, carrier_freq=2000.0, mod_freq=100.0):
        """Generate amplitude modulated signal"""
        t = np.arange(samples) / self.sample_rate
        carrier = np.sin(2 * np.pi * carrier_freq * t)
        modulator = 0.5 * (1 + np.sin(2 * np.pi * mod_freq * t))
        return 0.5 * carrier * modulator

    def fm_modulation(self, samples, carrier_freq=2000.0, mod_freq=100.0, mod_index=5.0):
        """Generate frequency modulated signal"""
        t = np.arange(samples) / self.sample_rate
        modulator = mod_index * np.sin(2 * np.pi * mod_freq * t)
        phase = 2 * np.pi * carrier_freq * t + modulator
        return 0.5 * np.sin(phase)


class NetworkSender:
    """Send signal data over TCP or UDP"""

    def __init__(self, host='0.0.0.0', port=5000, protocol='tcp'):
        self.host = host
        self.port = port
        self.protocol = protocol.lower()
        self.socket = None
        self.conn = None

    def start(self):
        """Initialize network connection"""
        if self.protocol == 'tcp':
            # TCP Server - wait for analyzer to connect
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.socket.bind((self.host, self.port))
            self.socket.listen(1)
            print(f"[*] TCP server listening on {self.host}:{self.port}")
            print(f"[*] Waiting for FFT analyzer to connect...")
            print(f"    Run: fft_analyzer_network.exe --source 127.0.0.1:{self.port} --protocol tcp")
            self.conn, addr = self.socket.accept()
            print(f"[OK] Connected from {addr}")

        elif self.protocol == 'udp':
            # UDP - send to localhost (analyzer must be running)
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.conn = self.socket
            print(f"[*] UDP sender ready for localhost:{self.port}")
            print(f"    Run: fft_analyzer_network.exe --source 127.0.0.1:{self.port} --protocol udp")
            print(f"[OK] Ready to send")

        else:
            raise ValueError(f"Unknown protocol: {self.protocol}")

    def send(self, samples):
        """Send float32 samples to analyzer"""
        # Convert to little-endian float32 bytes
        data = struct.pack(f'<{len(samples)}f', *samples)

        if self.protocol == 'tcp':
            self.conn.sendall(data)
        else:
            # UDP - send to localhost
            self.conn.sendto(data, ('127.0.0.1', self.port))

    def close(self):
        """Close connection"""
        if self.conn:
            self.conn.close()
        if self.socket:
            self.socket.close()


def main():
    parser = argparse.ArgumentParser(description='Send test signals to FFT Analyzer')
    parser.add_argument('--host', default='0.0.0.0', help='Host to bind (TCP) or send from (UDP)')
    parser.add_argument('--port', type=int, default=5000, help='Port number (default: 5000)')
    parser.add_argument('--protocol', choices=['tcp', 'udp'], default='tcp', help='Network protocol')
    parser.add_argument('--signal', choices=[
        'sine', 'multi', 'chirp', 'noise', 'impulse', 'square', 'sawtooth',
        'am', 'fm', 'signal_noise'
    ], default='sine', help='Signal type to generate')
    parser.add_argument('--freq', type=float, default=1000.0, help='Frequency in Hz (for sine)')
    parser.add_argument('--rate', type=float, default=0.1, help='Update rate in seconds (default: 0.1)')

    args = parser.parse_args()

    print("=" * 60)
    print("  FFT Analyzer - Network Data Sender")
    print("=" * 60)
    print(f"Signal:   {args.signal}")
    print(f"Protocol: {args.protocol.upper()}")
    print(f"Port:     {args.port}")
    print()

    # Create signal generator and network sender
    gen = SignalGenerator(SAMPLE_RATE)
    sender = NetworkSender(args.host, args.port, args.protocol)

    try:
        # Start network connection
        sender.start()
        print()
        print("[*] Sending signal data (Ctrl+C to stop)...")
        print()

        # Send data loop
        frame_count = 0
        start_time = time.time()

        while True:
            # Generate signal samples
            if args.signal == 'sine':
                samples = gen.sine(FFT_SIZE, args.freq)
            elif args.signal == 'multi':
                samples = gen.multi_tone(FFT_SIZE)
            elif args.signal == 'chirp':
                samples = gen.chirp(FFT_SIZE)
            elif args.signal == 'noise':
                samples = gen.noise(FFT_SIZE)
            elif args.signal == 'impulse':
                samples = gen.impulse_train(FFT_SIZE)
            elif args.signal == 'square':
                samples = gen.square_wave(FFT_SIZE, args.freq)
            elif args.signal == 'sawtooth':
                samples = gen.sawtooth(FFT_SIZE, args.freq)
            elif args.signal == 'am':
                samples = gen.am_modulation(FFT_SIZE)
            elif args.signal == 'fm':
                samples = gen.fm_modulation(FFT_SIZE)
            elif args.signal == 'signal_noise':
                samples = gen.signal_plus_noise(FFT_SIZE, args.freq)
            else:
                samples = gen.sine(FFT_SIZE, 1000.0)

            # Send to analyzer
            sender.send(samples)

            frame_count += 1
            if frame_count % 50 == 0:
                elapsed = time.time() - start_time
                fps = frame_count / elapsed
                print(f"[*] Sent {frame_count} frames ({fps:.1f} FPS, {elapsed:.1f}s)")

            # Wait for next update
            time.sleep(args.rate)

    except KeyboardInterrupt:
        print("\n[*] Stopping...")
    except BrokenPipeError:
        print("\n[ERROR] Connection lost (analyzer disconnected)")
    except Exception as e:
        print(f"\n[ERROR] {e}")
    finally:
        sender.close()
        print("[OK] Shutdown complete")


if __name__ == '__main__':
    main()
