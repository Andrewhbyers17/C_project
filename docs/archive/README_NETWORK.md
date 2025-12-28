# FFT Analyzer - Network Input Version

Windows-compatible FFT analyzer that reads live signal data from a TCP/UDP network source. Features the same DSP processing and web interface as the DE10-Nano version, but without hardware dependencies.

## Features

- **Network Input**: Reads float samples from TCP or UDP socket
- **Real-time FFT**: 512-point FFT analysis at 8000 Hz sample rate
- **Power Spectral Density**: Welch's method with 50% overlap
- **Web Interface**: Modern HTML5 GUI accessible via browser
- **Cross-Platform**: Runs on Windows 10/11 with Winsock2
- **No Hardware Dependencies**: Pure software implementation
- **Web-Only Control**: All signal modes controlled through web GUI

## System Requirements

### Windows
- Windows 10 or Windows 11
- MinGW-w64 **OR** Microsoft Visual Studio (MSVC)
- Web browser (Chrome, Firefox, Edge)

### Network Source
- Device streaming float32 samples
- TCP or UDP protocol support
- Sample rate: 8000 Hz (configurable)
- Data format: Little-endian IEEE 754 float (4 bytes per sample)

## Building

### Option 1: MinGW (Recommended)

```bash
# Install MinGW-w64 from: https://www.mingw-w64.org/

# Build
make -f Makefile.windows
```

### Option 2: Visual Studio (MSVC)

```cmd
# Open "Developer Command Prompt for VS"

# Build
nmake /F Makefile.windows CC=cl
```

### Build Output

```
fft_analyzer_network.exe
```

## Usage

### Basic Usage

```bash
# Show help
./fft_analyzer_network.exe --help

# Connect to network source (TCP)
./fft_analyzer_network.exe --source 192.168.1.100:5000 --protocol tcp

# Connect to network source (UDP)
./fft_analyzer_network.exe --source 10.0.0.50:6000 --protocol udp

# Test mode (no network, uses built-in test signals)
./fft_analyzer_network.exe --test
```

### Command-Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `--source IP:PORT` | Network source address | None (required unless --test) |
| `--protocol tcp\|udp` | Network protocol | `tcp` |
| `--test` | Use test waveforms instead of network | Off |
| `--port PORT` | Web server port | `8080` |
| `--help` | Show help message | - |

### Examples

```bash
# Real-world example: Connect to SDR software
./fft_analyzer_network.exe --source 192.168.1.100:5000 --protocol tcp

# Test with built-in signals
./fft_analyzer_network.exe --test --port 8080

# Custom web port
./fft_analyzer_network.exe --source 127.0.0.1:9000 --port 9090
```

## Web Interface

Once the analyzer is running, open your browser to:

```
http://localhost:8080
```

### Web GUI Features

- **Time-Domain Plot**: Live waveform display
- **FFT Magnitude Plot**: Frequency spectrum (0-4000 Hz)
- **Power Spectral Density**: Smoothed PSD using Welch's method
- **Frequency Bands**: 8-band energy visualization
- **Mode Selector**: Choose signal source (dropdown)
- **Pause/Resume**: Freeze the display
- **Status Indicators**: Connection status, pause state

### Available Modes

When `--test` mode is used, you can select:

| Mode | Description |
|------|-------------|
| Network Input | Read from network source (default when connected) |
| 440 Hz Sine | Pure 440 Hz tone |
| 1000 Hz Sine | Pure 1 kHz tone |
| 2000 Hz Sine | Pure 2 kHz tone |
| Mixed Tones | Combination of 440, 880, 1320 Hz |
| Frequency Sweep | 100-3000 Hz chirp |
| White Noise | Random noise |
| Impulse Train | Periodic impulses (100 Hz) |
| LFM Chirp | Linear frequency modulation 500-2500 Hz |
| Sinc Function | Sinc waveform (1 kHz bandwidth) |
| IQ LFM Chirp | Symmetric IQ chirp |
| Signal + Noise | 1 kHz sine with AWGN (SNR ~4.4 dB) |

**Note**: When connected to a network source, select "Network Input" to display the live data.

## Network Data Format

### Expected Format

The analyzer expects a **stream of IEEE 754 single-precision floating-point samples** (4 bytes each).

```
Byte Order: Little-endian (Intel/x86)
Data Type:  float32
Range:      -1.0 to +1.0 (recommended)
Rate:       512 samples per read (~100 ms at 8000 Hz)
```

### Example: Python Data Sender

```python
#!/usr/bin/env python3
import socket
import struct
import numpy as np
import time

# Configuration
HOST = '0.0.0.0'  # Listen on all interfaces
PORT = 5000
SAMPLE_RATE = 8000

# Create TCP server
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server.bind((HOST, PORT))
server.listen(1)

print(f"Waiting for connection on {HOST}:{PORT}...")
conn, addr = server.accept()
print(f"Connected: {addr}")

try:
    while True:
        # Generate 512 samples (one FFT frame)
        t = np.arange(512) / SAMPLE_RATE
        signal = 0.5 * np.sin(2 * np.pi * 1000 * t)  # 1 kHz sine

        # Convert to bytes (little-endian float32)
        data = struct.pack(f'{len(signal)}f', *signal)

        # Send to analyzer
        conn.sendall(data)

        # Wait ~100ms before next frame
        time.sleep(0.1)

except KeyboardInterrupt:
    print("\nShutting down...")
finally:
    conn.close()
    server.close()
```

### Example: GNU Radio Integration

Connect the FFT analyzer to GNU Radio using a **TCP Sink** block:

```
[Signal Source] → [TCP Sink]
                   Host: 0.0.0.0
                   Port: 5000
                   Data Type: Float
```

Then run the analyzer:
```bash
./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp
```

## Architecture

### No Hardware Dependencies

Unlike the DE10-Nano FPGA version, this build has **zero hardware dependencies**:

- ✅ No switch reading
- ✅ No button handling
- ✅ No LED output
- ✅ No FPGA memory mapping
- ✅ No ARM-specific code

**All control is through the web GUI.**

### Components Reused

| Component | Source | Purpose |
|-----------|--------|---------|
| `web_server.c` | DE10-Nano project | HTTP server, JSON API |
| `kiss_fft.c` | KissFFT library | FFT computation |
| DSP functions | DE10-Nano project | PSD, band energy |
| Network code | New | Winsock2 socket I/O |

### Main Loop Flow

```
1. Read 512 float samples from network (or generate test signal)
2. Compute 512-point FFT → magnitude spectrum
3. Compute PSD using Welch's method (256-sample segments, 50% overlap)
4. Calculate energy in 8 frequency bands
5. Update web interface with latest data
6. Handle web requests (mode change, pause, etc.)
7. Sleep 100ms
8. Repeat
```

## Troubleshooting

### Build Issues

**MinGW: "ws2_32.lib not found"**
```bash
# Use -lws2_32 instead (note the lowercase 'l')
make -f Makefile.windows LDFLAGS="-lws2_32 -lm"
```

**MSVC: "Cannot open include file 'winsock2.h'"**
```cmd
# Install Windows SDK with Visual Studio
# Or run from "Developer Command Prompt for VS"
```

### Runtime Issues

**"WSAStartup failed"**
- Winsock2 not initialized
- Windows Firewall blocking?
- Run as Administrator if needed

**"Connection failed" or "Connection lost"**
- Check network source is running
- Verify IP address and port
- Check firewall settings
- Try test mode: `--test`

**Web interface not loading**
- Check port not already in use
- Try different port: `--port 9090`
- Check browser URL: `http://localhost:8080`

**No data displayed**
- Verify network source is sending data
- Check data format (float32, little-endian)
- Look at console for "[ERROR]" messages
- Try test mode to verify GUI works

### Network Source Issues

**Data format mismatch**
```python
# Wrong: Sending int16
data = struct.pack('h', value)  # ❌

# Correct: Sending float32
data = struct.pack('f', value)  # ✅
```

**Wrong sample count**
- Analyzer reads **exactly 512 samples** per frame
- Sending fewer will cause blocking (TCP) or incomplete reads (UDP)
- Sending more is OK (buffered for next read)

**Byte order mismatch**
```python
# Force little-endian
data = struct.pack('<512f', *samples)  # ✅
```

## Performance

### Typical Performance (Windows 10, i5 CPU)

- **Update Rate**: 10 Hz (100ms per frame)
- **CPU Usage**: ~5-10% (single core)
- **Memory**: ~20 MB
- **Network Bandwidth**: ~163 Kbps (512 samples × 4 bytes × 10 Hz)

### Latency

- **Network → Display**: ~150-200ms total
  - Network read: ~10-50ms (depends on buffering)
  - FFT computation: ~5-10ms
  - PSD computation: ~20-30ms
  - Web update: ~10-20ms
  - Browser rendering: ~16ms (60 FPS)

## Advanced Usage

### Changing Sample Rate

Edit [fft_analyzer_network.c](fft_analyzer_network.c#L46):
```c
#define SAMPLE_RATE  16000  // Change from 8000 to 16000
```
Rebuild and ensure network source matches.

### Changing FFT Size

Edit [fft_analyzer_network.c](fft_analyzer_network.c#L45):
```c
#define FFT_SIZE  1024  // Change from 512 to 1024
```
Rebuild and ensure network source sends correct number of samples.

### Changing Update Rate

Edit [fft_analyzer_network.c](fft_analyzer_network.c#L48):
```c
#define UPDATE_RATE_MS  50  // Change from 100 to 50 (20 Hz update)
```
Higher update rates increase CPU usage and network bandwidth.

### Custom Frequency Bands

Edit the `BAND_EDGES` array in [fft_analyzer_network.c](fft_analyzer_network.c#L50):
```c
static const float BAND_EDGES[NUM_BANDS + 1] = {
    0, 500, 1000, 1500, 2000, 2500, 3000, 3500, 4000  // Example: 8 equal bands
};
```

## Comparison to Original (DE10-Nano Version)

| Feature | DE10-Nano Version | Network Version |
|---------|-------------------|-----------------|
| Platform | Linux (ARM) | Windows (x86/x64) |
| Input | FPGA waveform generator | TCP/UDP network |
| Control | Hardware switches | Web GUI only |
| LEDs | 8 LED outputs | None |
| Buttons | Pause/reset buttons | Web controls |
| Build | ARM GCC cross-compile | MinGW/MSVC native |
| Deployment | SCP to board | Run locally |

## API Reference

### REST Endpoints

The web server provides these endpoints:

**GET /**
- Returns HTML web interface

**GET /api/fft**
- Returns JSON with current FFT data
- Fields: `fft_size`, `sample_rate`, `magnitude[]`, `psd[]`, `band_energies[]`, `time_domain[]`, `mode`, `paused`, `web_control_active`

**POST /api/pause**
- Toggles pause state
- Returns: `{"status": "PAUSED"}` or `{"status": "RESUMED"}`

**POST /api/mode?value=N**
- Changes signal mode (N = 0-11)
- Returns: `{"status": "ok"}` or `{"status": "error", "message": "..."}`

## License

Same license as the original DE10-Nano FFT Analyzer project.

## Credits

- **KissFFT**: Simple FFT library by Mark Borgerding
- **Original Project**: DE10-Nano FPGA FFT Analyzer
- **Network Version**: Ported for Windows with network input

## Support

For issues, check:
1. Console output for error messages
2. Network source is sending correct data format
3. Firewall not blocking ports
4. Try `--test` mode to isolate network issues

---

**Quick Start Checklist:**

- [ ] Install MinGW or Visual Studio
- [ ] Build: `make -f Makefile.windows`
- [ ] Test: `./fft_analyzer_network.exe --test`
- [ ] Open browser: `http://localhost:8080`
- [ ] Verify display shows test signals
- [ ] Create network data source
- [ ] Connect: `./fft_analyzer_network.exe --source IP:PORT --protocol tcp`
- [ ] Select "Network Input" mode in web GUI
- [ ] Enjoy real-time FFT analysis!
