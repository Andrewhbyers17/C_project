# Network Streaming Guide - FFT Analyzer v1.0.0

This guide explains how to stream audio data from a remote IP address and port to the FFT Analyzer.

---

## Quick Start

To stream from a specific IP address and port, use the `--source` option:

```bash
fft_analyzer_network.exe --source 192.168.1.100:5000
```

This will:
1. Connect to IP `192.168.1.100` on port `5000`
2. Stream audio data via TCP (default protocol)
3. Start the web server on port `8080` (default)
4. Display real-time FFT analysis in the browser

---

## Command-Line Options

### Basic Streaming

```bash
fft_analyzer_network.exe --source <IP>:<PORT>
```

**Parameters:**
- `<IP>` - IP address of the data source (e.g., `192.168.1.100`, `10.0.0.50`)
- `<PORT>` - Port number (e.g., `5000`, `8000`, `12345`)

**Example:**
```bash
fft_analyzer_network.exe --source 192.168.1.100:5000
```

### Change Network Protocol

By default, the analyzer uses **TCP** for reliable streaming. You can also use **UDP** for lower-latency streaming:

```bash
fft_analyzer_network.exe --source 192.168.1.100:5000 --protocol udp
```

**TCP vs UDP:**
- **TCP** (default):
  - ✅ Reliable - no data loss
  - ✅ Ordered delivery
  - ❌ Slightly higher latency
  - **Use for:** Recordings, precise analysis

- **UDP**:
  - ✅ Lower latency
  - ✅ Faster
  - ❌ May drop packets on poor networks
  - **Use for:** Real-time monitoring, live displays

### Change Web Server Port

If port `8080` is already in use, change the web server port:

```bash
fft_analyzer_network.exe --source 192.168.1.100:5000 --port 9000
```

Then open your browser to: `http://localhost:9000`

---

## Complete Examples

### Example 1: Stream from FPGA Board (TCP)
```bash
fft_analyzer_network.exe --source 192.168.1.50:5000 --protocol tcp
```
- Connects to FPGA at `192.168.1.50:5000`
- Uses reliable TCP streaming
- Web interface at `http://localhost:8080`

### Example 2: Stream from SDR (UDP, custom port)
```bash
fft_analyzer_network.exe --source 10.0.0.100:8000 --protocol udp --port 9090
```
- Connects to SDR at `10.0.0.100:8000`
- Uses fast UDP streaming
- Web interface at `http://localhost:9090`

### Example 3: Stream from Microcontroller
```bash
fft_analyzer_network.exe --source 192.168.4.1:6000
```
- Connects to microcontroller at `192.168.4.1:6000`
- Uses TCP by default
- Web interface at `http://localhost:8080`

---

## Data Format Requirements

The FFT Analyzer expects the network source to send **float32 audio samples** in **little-endian** format.

### Technical Specifications:
- **Sample Rate:** 8000 Hz (8 kHz)
- **Sample Format:** IEEE 754 float32 (4 bytes per sample)
- **Byte Order:** Little-endian
- **Continuous Stream:** Send samples continuously without gaps

### Example Data Stream:
```
[float32] [float32] [float32] [float32] ...
 sample0   sample1   sample2   sample3
```

Each sample is a 4-byte floating-point value representing the signal amplitude.

---

## Setting Up Your Data Source

To stream data to the FFT Analyzer, your device (FPGA, microcontroller, SDR, etc.) must:

1. **Listen on a port** (e.g., port `5000`)
2. **Accept incoming TCP/UDP connections**
3. **Send float32 samples** continuously at 8 kHz

### Example: Python Data Source (for testing)

Here's a simple Python script to simulate a data source:

```python
import socket
import struct
import math
import time

# Configuration
HOST = '0.0.0.0'  # Listen on all interfaces
PORT = 5000       # Port to listen on
SAMPLE_RATE = 8000
FREQUENCY = 440   # 440 Hz sine wave

# Create TCP socket
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.bind((HOST, PORT))
server.listen(1)
print(f"Listening on {HOST}:{PORT}...")

# Wait for FFT Analyzer to connect
client, addr = server.accept()
print(f"Connected by {addr}")

# Stream sine wave samples
phase = 0.0
phase_increment = 2.0 * math.pi * FREQUENCY / SAMPLE_RATE

try:
    while True:
        # Generate sine wave sample
        sample = math.sin(phase)
        phase += phase_increment
        if phase > 2.0 * math.pi:
            phase -= 2.0 * math.pi

        # Convert to float32 little-endian bytes
        data = struct.pack('<f', sample)
        client.send(data)

        # Sleep to maintain sample rate
        time.sleep(1.0 / SAMPLE_RATE)
except KeyboardInterrupt:
    print("\nStopping...")
finally:
    client.close()
    server.close()
```

**Usage:**
1. Save as `audio_source.py`
2. Run: `python audio_source.py`
3. In another terminal: `fft_analyzer_network.exe --source 127.0.0.1:5000`
4. Open browser to `http://localhost:8080`
5. You should see a 440 Hz peak in the FFT!

---

## Test Mode vs Network Mode

### Test Mode (Built-in Signals)
```bash
fft_analyzer_network.exe --test
```
- Uses built-in test waveforms (sine waves, sweeps, noise)
- No network connection required
- Perfect for demos and testing
- **Default mode** if no `--source` is specified

### Network Mode (Real Data)
```bash
fft_analyzer_network.exe --source 192.168.1.100:5000
```
- Streams from real hardware
- Requires active network source
- Production use case

---

## Troubleshooting

### Problem: "Connection refused"
**Cause:** The data source is not running or not listening on the specified port.

**Solution:**
1. Verify the data source is running
2. Check the IP address is correct
3. Verify the port is open (use `telnet <IP> <PORT>` to test)
4. Check firewall settings

### Problem: "No data appearing"
**Cause:** Data source is not sending data, or data format is incorrect.

**Solution:**
1. Verify data source is sending float32 samples
2. Check byte order (must be little-endian)
3. Ensure sample rate is approximately 8 kHz
4. Use Wireshark to verify data is being transmitted

### Problem: "Choppy or distorted audio"
**Cause:** Network congestion, packet loss (UDP), or incorrect sample rate.

**Solution:**
1. Switch from UDP to TCP for reliability
2. Check network bandwidth
3. Verify data source sample rate matches 8 kHz
4. Reduce network traffic on the same connection

### Problem: "Web interface shows old data"
**Cause:** Network connection interrupted, but web server still running.

**Solution:**
1. Restart the FFT Analyzer
2. Check network connection status
3. Verify data source is still transmitting

---

## Network Configuration Tips

### Finding Your IP Address

**Windows:**
```bash
ipconfig
```
Look for "IPv4 Address" under your active network adapter.

**Linux:**
```bash
ifconfig
```
or
```bash
ip addr show
```

### Connecting Over WiFi

If streaming over WiFi:
1. Ensure both devices are on the same network
2. Use the analyzer's WiFi IP address (not `127.0.0.1`)
3. Consider TCP for better reliability on WiFi

### Connecting Over Ethernet

For best performance (low latency, high reliability):
1. Use wired Ethernet connection
2. Use static IP addresses to avoid DHCP issues
3. TCP or UDP both work well on wired connections

---

## Advanced: Multiple Connections

The FFT Analyzer supports **only one data source** at a time. If you need to switch sources:

1. **Stop the current analyzer** (Ctrl+C)
2. **Start with new source:**
   ```bash
   fft_analyzer_network.exe --source <NEW_IP>:<NEW_PORT>
   ```

---

## Firewall Configuration

### Windows Firewall

If the connection fails, you may need to allow the FFT Analyzer through Windows Firewall:

1. Open **Windows Security** → **Firewall & network protection**
2. Click **Allow an app through firewall**
3. Click **Change settings** → **Allow another app**
4. Browse to `fft_analyzer_network.exe`
5. Check both **Private** and **Public** networks
6. Click **OK**

### Linux Firewall (ufw)

```bash
sudo ufw allow 8080/tcp   # Web server port
sudo ufw allow 5000/tcp   # Data source port (if hosting)
```

---

## Summary: Complete Workflow

### Step 1: Start Your Data Source
Your FPGA, SDR, or microcontroller should be:
- Listening on a specific port (e.g., `5000`)
- Ready to send float32 samples at 8 kHz

### Step 2: Start FFT Analyzer
```bash
fft_analyzer_network.exe --source <IP>:<PORT> --protocol tcp
```

### Step 3: Open Web Browser
Navigate to: `http://localhost:8080`

### Step 4: Monitor & Record
- View real-time FFT and spectrogram
- Click **Record** dropdown → Select format
- Click **Record** to start logging data to `logs/` folder

---

## Quick Reference

| Option | Description | Example |
|--------|-------------|---------|
| `--source <IP>:<PORT>` | Network data source | `--source 192.168.1.100:5000` |
| `--protocol tcp\|udp` | Network protocol | `--protocol udp` |
| `--port <PORT>` | Web server port | `--port 9000` |
| `--test` | Use built-in test signals | `--test` |
| `--help` | Show help message | `--help` |

---

## Need Help?

If you encounter issues:

1. **Test with built-in signals first:**
   ```bash
   fft_analyzer_network.exe --test
   ```
   If this works, the issue is with your network source.

2. **Verify network connectivity:**
   ```bash
   ping <IP>
   telnet <IP> <PORT>
   ```

3. **Check the console output** for error messages when starting the analyzer.

4. **Use the Python test script** (above) to verify the analyzer works with a known-good source.

---

**Version:** 1.0.0
**Last Updated:** December 24, 2025
