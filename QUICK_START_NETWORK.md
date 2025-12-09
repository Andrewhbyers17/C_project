# Quick Start Guide - Network Version

Get the FFT analyzer running on Windows in 5 minutes!

## Step 1: Install Build Tools

Choose **ONE** option:

### Option A: MinGW (Recommended - Easier)
1. Download: https://www.mingw-w64.org/downloads/
2. Install to: `C:\mingw64`
3. Add to PATH: `C:\mingw64\bin`
4. Test: Open cmd and type `gcc --version`

### Option B: Visual Studio
1. Download Visual Studio Community (free)
2. Install "Desktop development with C++"
3. Open "Developer Command Prompt for VS"

## Step 2: Build the Analyzer

```bash
cd Final_project

# MinGW
make -f Makefile.windows

# OR Visual Studio (in Developer Command Prompt)
nmake /F Makefile.windows CC=cl
```

You should see:
```
========================================
  Build successful!
========================================
```

## Step 3: Test It Works

```bash
# Run in test mode (no network needed)
./fft_analyzer_network.exe --test
```

Expected output:
```
===========================================
  FFT Analyzer - Network Input Version
===========================================

[*] Using test waveforms (no network input)
[*] Initializing web server on port 8080...
[OK] Web callbacks registered
[OK] Ready!
```

## Step 4: Open Web Interface

Open your browser to:
```
http://localhost:8080
```

You should see:
- Time-domain plot (waveform)
- FFT magnitude plot
- PSD plot
- Frequency band energies
- Mode selector (dropdown)
- Pause/Resume button

**Try changing modes!** Select different waveforms from the dropdown.

## Step 5: Connect to Real Data Source

### Option A: Use the Python Test Script

**Terminal 1** - Run the data sender:
```bash
python send_test_data.py --port 5000 --protocol tcp --signal chirp
```

Wait for:
```
[*] TCP server listening on 0.0.0.0:5000
[*] Waiting for FFT analyzer to connect...
```

**Terminal 2** - Run the analyzer:
```bash
./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp
```

You should see in Terminal 1:
```
[OK] Connected from ('127.0.0.1', 12345)
[*] Sending signal data...
```

And in Terminal 2:
```
[*] Reading signal data from 127.0.0.1:5000 (TCP)
[OK] Ready!
```

**Browser**: Select "Network Input" mode to see the live chirp signal!

### Option B: Connect to Your Own Source

Your data source must:
1. Send **float32** samples (IEEE 754, little-endian)
2. Use TCP or UDP
3. Send at least **512 samples per frame**
4. Sample rate: **8000 Hz** (or modify `SAMPLE_RATE` in code)

Example Python sender:
```python
import socket
import struct
import numpy as np

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('127.0.0.1', 5000))

while True:
    # Generate 512 float samples
    samples = np.random.randn(512).astype(np.float32)

    # Send as bytes
    data = samples.tobytes()
    sock.sendall(data)

    time.sleep(0.1)  # 10 Hz update rate
```

## Troubleshooting

### "Cannot open include file 'winsock2.h'"
- Install Windows SDK or use Visual Studio installer
- Make sure you're in "Developer Command Prompt"

### "ws2_32.lib not found"
- MinGW: Use lowercase `-lws2_32`
- Check: `gcc --version` to verify MinGW is in PATH

### Web interface doesn't load
```bash
# Try different port
./fft_analyzer_network.exe --test --port 9090

# Then open: http://localhost:9090
```

### "Connection failed"
1. Make sure data sender is running FIRST (for TCP)
2. Check IP address is correct
3. Check port number matches
4. Try test mode: `--test`

### No signal displayed
1. Select "Network Input" mode in dropdown
2. Check console for "[ERROR]" messages
3. Verify data sender is sending float32 (not int16 or other)

## Common Usage Patterns

### Pattern 1: SDR Software → FFT Analyzer
```bash
# GNU Radio outputs on port 5000
./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp
```

### Pattern 2: Remote Signal Source
```bash
# Connect to signal server on another machine
./fft_analyzer_network.exe --source 192.168.1.100:8000 --protocol tcp
```

### Pattern 3: Test Signals Only
```bash
# No network, just test waveforms
./fft_analyzer_network.exe --test
```

### Pattern 4: UDP Data Stream
```bash
# Python sender: send_test_data.py --protocol udp
./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol udp
```

## Next Steps

1. **Read the full README**: [README_NETWORK.md](README_NETWORK.md)
2. **Customize signals**: Edit `send_test_data.py`
3. **Change parameters**: Edit `SAMPLE_RATE`, `FFT_SIZE` in source
4. **Create your data source**: Follow examples in README

## File Checklist

After building, you should have:
- ✅ `fft_analyzer_network.exe` (Windows executable)
- ✅ `send_test_data.py` (Python test script)
- ✅ `README_NETWORK.md` (Full documentation)
- ✅ `Makefile.windows` (Build script)

## Key Differences from DE10-Nano Version

| Feature | DE10-Nano | Network Version |
|---------|-----------|-----------------|
| Platform | Linux/ARM | Windows/x86 |
| Input | FPGA hardware | TCP/UDP network |
| Switches | 4 hardware switches | None (web only) |
| Buttons | 2 hardware buttons | None (web only) |
| LEDs | 8 LED outputs | None |
| Control | Hardware + Web | Web only |

**All control is through the web GUI!**

---

## Quick Reference Card

```
Build:    make -f Makefile.windows
Test:     ./fft_analyzer_network.exe --test
Browser:  http://localhost:8080
Help:     ./fft_analyzer_network.exe --help

Python:   python send_test_data.py --port 5000 --signal chirp
Connect:  ./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp

Stop:     Ctrl+C
```

**You're all set! Enjoy your network-based FFT analyzer on Windows!**
