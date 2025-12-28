#!/usr/bin/env python3
"""
Read and analyze raw IQ HDF5 files created by the FFT analyzer.

Usage:
    python read_iq_file.py logs/iq_data_*.h5
    python read_iq_file.py logs/iq_data_*.h5 --plot
    python read_iq_file.py logs/iq_data_*.h5 --fft
"""

import h5py
import numpy as np
import argparse
import sys

def read_iq_file(filename):
    """Read IQ data from HDF5 file"""
    try:
        f = h5py.File(filename, 'r')

        # Read metadata
        sample_rate = f.attrs['sample_rate']
        start_time = f.attrs['start_time']
        format_str = f.attrs['format']

        # Read IQ data
        iq_dataset = f['/iq_samples']

        print("=" * 60)
        print(f"File: {filename}")
        print("=" * 60)
        print(f"Sample rate:    {sample_rate:,} Hz ({sample_rate/1e6:.2f} MHz)" if sample_rate >= 1e6
              else f"Sample rate:    {sample_rate:,} Hz ({sample_rate/1e3:.1f} kHz)")
        print(f"Start time:     {start_time} (Unix timestamp)")
        print(f"Format:         {format_str}")
        print(f"Dataset shape:  {iq_dataset.shape}")
        print(f"Dataset dtype:  {iq_dataset.dtype}")
        print(f"Complex samples: {iq_dataset.shape[0]:,}")

        # Calculate duration
        duration = iq_dataset.shape[0] / sample_rate
        print(f"Duration:       {duration:.3f} seconds")

        # Calculate file size
        file_size_mb = iq_dataset.shape[0] * iq_dataset.dtype.itemsize / (1024 * 1024)
        print(f"Data size:      {file_size_mb:.2f} MB")
        print()

        # Read a small chunk to show statistics
        chunk_size = min(10000, iq_dataset.shape[0])
        chunk = iq_dataset[:chunk_size]

        # Convert structured array to complex
        if chunk.dtype.names:  # Compound type with 'r' and 'i' fields
            iq_complex = chunk['r'] + 1j * chunk['i']
            print("Compound type detected (native HDF5 complex)")
        else:  # Interleaved float array
            iq_complex = chunk[0::2] + 1j * chunk[1::2]
            print("Interleaved float type")

        print(f"\nFirst {chunk_size} samples statistics:")
        print(f"  I (Real):      mean={np.real(iq_complex).mean():.6f}, std={np.real(iq_complex).std():.6f}")
        print(f"  Q (Imaginary): mean={np.imag(iq_complex).mean():.6f}, std={np.imag(iq_complex).std():.6f}")
        print(f"  Magnitude:     mean={np.abs(iq_complex).mean():.6f}, max={np.abs(iq_complex).max():.6f}")
        print(f"  Phase (deg):   mean={np.angle(iq_complex, deg=True).mean():.1f}, std={np.angle(iq_complex, deg=True).std():.1f}")

        f.close()

        return {
            'filename': filename,
            'sample_rate': sample_rate,
            'start_time': start_time,
            'format': format_str,
            'num_samples': iq_dataset.shape[0],
            'duration': duration
        }

    except Exception as e:
        print(f"[ERROR] Failed to read file: {e}")
        return None

def plot_iq_data(filename, num_samples=1024):
    """Plot IQ data (time domain and spectrum)"""
    try:
        import matplotlib.pyplot as plt

        f = h5py.File(filename, 'r')
        sample_rate = f.attrs['sample_rate']
        iq_dataset = f['/iq_samples']

        # Read samples
        chunk_size = min(num_samples, iq_dataset.shape[0])
        chunk = iq_dataset[:chunk_size]

        # Convert to complex
        if chunk.dtype.names:
            iq_complex = chunk['r'] + 1j * chunk['i']
        else:
            iq_complex = chunk[0::2] + 1j * chunk[1::2]

        f.close()

        # Create plots
        fig, axes = plt.subplots(2, 2, figsize=(12, 8))
        fig.suptitle(f'IQ Data Analysis: {filename}', fontsize=14)

        # Time axis
        t = np.arange(len(iq_complex)) / sample_rate * 1000  # milliseconds

        # Plot I/Q time domain
        axes[0, 0].plot(t, np.real(iq_complex), 'b-', label='I (Real)', alpha=0.7)
        axes[0, 0].plot(t, np.imag(iq_complex), 'r-', label='Q (Imaginary)', alpha=0.7)
        axes[0, 0].set_xlabel('Time (ms)')
        axes[0, 0].set_ylabel('Amplitude')
        axes[0, 0].set_title('I/Q Time Domain')
        axes[0, 0].legend()
        axes[0, 0].grid(True, alpha=0.3)

        # Plot magnitude
        axes[0, 1].plot(t, np.abs(iq_complex), 'g-')
        axes[0, 1].set_xlabel('Time (ms)')
        axes[0, 1].set_ylabel('Magnitude')
        axes[0, 1].set_title('Signal Magnitude')
        axes[0, 1].grid(True, alpha=0.3)

        # Plot I/Q constellation
        axes[1, 0].plot(np.real(iq_complex), np.imag(iq_complex), 'b.', alpha=0.3, markersize=2)
        axes[1, 0].set_xlabel('I (Real)')
        axes[1, 0].set_ylabel('Q (Imaginary)')
        axes[1, 0].set_title('I/Q Constellation')
        axes[1, 0].grid(True, alpha=0.3)
        axes[1, 0].axis('equal')

        # Plot FFT spectrum
        fft = np.fft.fft(iq_complex)
        freqs = np.fft.fftfreq(len(iq_complex), 1/sample_rate) / 1000  # kHz
        magnitude_db = 20 * np.log10(np.abs(fft) + 1e-10)

        # Sort for plotting
        idx = np.argsort(freqs)
        axes[1, 1].plot(freqs[idx], magnitude_db[idx], 'b-', linewidth=0.5)
        axes[1, 1].set_xlabel('Frequency (kHz)')
        axes[1, 1].set_ylabel('Magnitude (dB)')
        axes[1, 1].set_title(f'FFT Spectrum ({len(iq_complex)} points)')
        axes[1, 1].grid(True, alpha=0.3)

        plt.tight_layout()
        plt.show()

    except ImportError:
        print("[ERROR] matplotlib not installed. Install with: pip install matplotlib")
    except Exception as e:
        print(f"[ERROR] Failed to plot: {e}")

def compute_fft(filename, fft_size=8192):
    """Compute and display FFT of IQ data"""
    try:
        f = h5py.File(filename, 'r')
        sample_rate = f.attrs['sample_rate']
        iq_dataset = f['/iq_samples']

        # Read samples
        chunk_size = min(fft_size, iq_dataset.shape[0])
        chunk = iq_dataset[:chunk_size]

        # Convert to complex
        if chunk.dtype.names:
            iq_complex = chunk['r'] + 1j * chunk['i']
        else:
            iq_complex = chunk[0::2] + 1j * chunk[1::2]

        f.close()

        # Compute FFT
        fft = np.fft.fft(iq_complex)
        freqs = np.fft.fftfreq(len(iq_complex), 1/sample_rate)
        magnitude_db = 20 * np.log10(np.abs(fft) + 1e-10)

        # Find peak
        peak_idx = np.argmax(magnitude_db)
        peak_freq = freqs[peak_idx]
        peak_mag = magnitude_db[peak_idx]

        print(f"\nFFT Analysis ({len(iq_complex)} points):")
        print(f"  Peak frequency: {peak_freq:.2f} Hz ({peak_freq/1000:.3f} kHz)")
        print(f"  Peak magnitude: {peak_mag:.2f} dB")

        # Show strongest components
        sorted_idx = np.argsort(magnitude_db)[::-1]
        print(f"\nTop 5 frequency components:")
        for i in range(min(5, len(sorted_idx))):
            idx = sorted_idx[i]
            print(f"    {freqs[idx]:8.1f} Hz: {magnitude_db[idx]:6.2f} dB")

    except Exception as e:
        print(f"[ERROR] Failed to compute FFT: {e}")

def main():
    parser = argparse.ArgumentParser(description='Read and analyze IQ HDF5 files')
    parser.add_argument('filename', help='HDF5 file to read')
    parser.add_argument('--plot', action='store_true', help='Plot IQ data (requires matplotlib)')
    parser.add_argument('--fft', action='store_true', help='Compute and display FFT')
    parser.add_argument('--samples', type=int, default=1024, help='Number of samples to plot/analyze (default: 1024)')

    args = parser.parse_args()

    # Read basic info
    info = read_iq_file(args.filename)

    if info is None:
        return 1

    # Optional FFT analysis
    if args.fft:
        compute_fft(args.filename, args.samples)

    # Optional plotting
    if args.plot:
        plot_iq_data(args.filename, args.samples)

    return 0

if __name__ == '__main__':
    sys.exit(main())
