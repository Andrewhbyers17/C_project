# Start Guide - All Methods

## Easiest Way: One-Click Start

### Option 1: Quick Test (Automatic)

```bash
start_test.bat
```

**This automatically:**
- ✅ Starts IQ server on port 5000
- ✅ Starts FFT analyzer (connects to server)
- ✅ Opens web browser to http://localhost:8080
- ✅ Uses 2 MHz sine wave by default

**Customization:**
```bash
# 100 kHz sweep
start_test.bat --rate 100000 --signal sweep

# 10 MHz multi-tone
start_test.bat --rate 10000000 --signal multi

# 8 kHz audio
start_test.bat --rate 8000 --signal sine
```

---

## Interactive Testing

### Option 2: Interactive Test Script

```bash
test_raw_iq.bat
```

**This does:**
- Starts IQ server automatically
- Starts FFT analyzer
- You see console output (not minimized)
- Edit the .bat file to change signal/rate

---

## Manual Control (Two Terminals)

### Option 3: Full Manual Control

**Terminal 1 - IQ Server:**
```bash
python test_iq_server.py --port 5000 --rate 2000000 --signal sine
```

**Terminal 2 - FFT Analyzer:**
```bash
fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp
```

**Browser:**
```bash
start http://localhost:8080
```

---

## Comparison

| Method | Auto-Open Browser | Visibility | Best For |
|--------|------------------|------------|----------|
| `start_test.bat` | ✅ Yes | Minimized | Quick testing |
| `test_raw_iq.bat` | ❌ No | Full console | Debugging |
| Manual (2 terminals) | ❌ No | Full console | Development |

---

## Sample Rates Available

### Audio Rates
```bash
# 8 kHz - standard audio
start_test.bat --rate 8000 --signal sine
```

### Ultrasonic
```bash
# 100 kHz
start_test.bat --rate 100000 --signal sweep
```

### RF Low
```bash
# 1 MHz
start_test.bat --rate 1000000 --signal multi
```

### RF High (Default)
```bash
# 2 MHz (default)
start_test.bat
```

### Maximum
```bash
# 10 MHz (requires good PC)
start_test.bat --rate 10000000 --signal noise
```

---

## Signal Types

### Sine Wave (Default)
```bash
start_test.bat --signal sine
```
- 1 kHz pure tone
- Good for frequency accuracy testing
- Clean spectrum

### Frequency Sweep
```bash
start_test.bat --signal sweep
```
- 100 Hz to 4 kHz chirp
- Tests frequency response
- Dynamic spectrum

### White Noise
```bash
start_test.bat --signal noise
```
- Broadband noise
- Tests noise floor
- Flat spectrum

### Signal + Noise
```bash
start_test.bat --signal signal_noise
```
- 1 kHz tone + noise (10 dB SNR)
- Tests SNR performance
- Realistic signal

### Multi-Tone
```bash
start_test.bat --signal multi
```
- 500 Hz + 1 kHz + 2 kHz
- Tests multi-signal handling
- Multiple peaks

---

## What Happens When You Start

### Console Output

**IQ Server window:**
```
[*] IQ Data Server listening on port 5000
[*] Sample rate: 2000000 Hz (2.00 MHz)
[*] Signal type: sine
[*] Waiting for FFT analyzer to connect...
[OK] Client connected from 127.0.0.1:XXXXX
[*] Sent 512000 I/Q pairs (1.0s) - 512000 samples/sec
[*] Sent 1024000 I/Q pairs (2.0s) - 512000 samples/sec
...
```

**FFT Analyzer window:**
```
[*] Connecting to 127.0.0.1:5000 (TCP)...
[OK] Connected to 127.0.0.1:5000
[OK] Ring buffer allocated (5000 frames, 80.00 MB)
[OK] Network receiver thread started
[AUTO] Detected sample rate: 2000000 Hz (2.00 MHz)
[AUTO] Decimation factor: 100 (display rate: 20000 Hz)
[OK] Ready!
```

**Web browser:** Opens to http://localhost:8080 showing FFT display

---

## Recording Raw IQ Data

Once the web interface is open:

### Step 1: Start Recording
1. Click **"Start Logging"** button
2. Select format: **"raw_iq"**
3. Click **"Confirm"**

### Step 2: Verify Recording
Console shows:
```
[WEB] Starting RAW IQ streaming at 2000000 Hz (2.00 MHz)
[OK] Disk writer thread created
[OK] Disk writer thread started
```

### Step 3: Check Files
```
logs/iq_data_20251227_HHMMSSZ.h5
```

File grows as data is recorded:
- 2 MHz: ~16 MB/second
- 10 seconds: ~160 MB
- 1 minute: ~960 MB

### Step 4: Stop Recording
Click **"Stop Logging"** in web interface

Console shows:
```
[*] Stopping disk writer thread...
[OK] Disk writer thread stopped
[LOGGER] Stopped logging. Wrote XXXXXX IQ samples to: logs/iq_data_*.h5
```

---

## Analyzing Recorded Data

### Quick Check
```bash
python read_iq_file.py logs/iq_data_*.h5
```

Output:
```
============================================================
File: logs/iq_data_20251227_123456Z.h5
============================================================
Sample rate:    2000000 Hz (2.00 MHz)
Duration:       10.000 seconds
Data size:      152.59 MB
Complex samples: 20,000,000
```

### FFT Analysis
```bash
python read_iq_file.py logs/iq_data_*.h5 --fft --samples 8192
```

### Plot Data
```bash
python read_iq_file.py logs/iq_data_*.h5 --plot --samples 1024
```

---

## Stopping Everything

### Method 1: Close Windows
- Close the "IQ Server" window (red X)
- Close the "FFT Analyzer" window (red X)

### Method 2: Ctrl+C
Press Ctrl+C in either window

### Method 3: Task Manager
- Right-click taskbar → Task Manager
- Find "python.exe" (IQ Server)
- Find "fft_analyzer_network.exe"
- End both tasks

### Method 4: Command Line
```bash
taskkill /FI "WINDOWTITLE eq *IQ Server*" /F
taskkill /FI "WINDOWTITLE eq *FFT Analyzer*" /F
```

---

## Troubleshooting

### Browser Doesn't Open

**Problem:** `start_test.bat` didn't open browser

**Solution:** Open manually
```bash
start http://localhost:8080
```

Or type in browser: `http://localhost:8080`

---

### Connection Refused

**Problem:**
```
[ERROR] Connection failed: [WinError 10061]
```

**Check:** Is IQ server running?

**Fix:**
1. Close both windows
2. Run `start_test.bat` again (correct order)

---

### Ring Buffer Overflow

**Problem:**
```
[WARN] Ring buffer overflow! Dropping 4096 samples
```

**This is now FIXED** in the latest build (80 MB buffer)

If you still see it:
1. Check disk speed (should be SSD for 10 MHz)
2. Reduce sample rate
3. Close other programs

---

### Port Already in Use

**Problem:**
```
[ERROR] Address already in use
```

**Fix:** Kill old processes
```bash
taskkill /FI "WINDOWTITLE eq *IQ Server*" /F
taskkill /FI "WINDOWTITLE eq *FFT Analyzer*" /F
```

Then try again.

---

### No Auto-Detection

**Problem:** Always shows 10 MHz

**Wait:** Auto-detection runs every second

**Check:** IQ server should show:
```
[*] Sent XXXXX I/Q pairs
```

If not sending, restart both.

---

## Advanced Usage

### Custom Port
```bash
# IQ Server on different port
python test_iq_server.py --port 5001 --rate 2000000 --signal sine

# Analyzer connects to it
fft_analyzer_network.exe --source 127.0.0.1:5001 --protocol tcp
```

### Limited Duration
```bash
# Server runs for 60 seconds then stops
python test_iq_server.py --port 5000 --rate 2000000 --signal sine --duration 60
```

### Remote Connection
```bash
# IQ Server on one machine (192.168.1.100)
python test_iq_server.py --port 5000 --rate 2000000 --signal sine

# Analyzer on another machine connects to it
fft_analyzer_network.exe --source 192.168.1.100:5000 --protocol tcp
```

---

## Files Reference

| Script | Purpose | Auto-Open Browser | Auto-Start Server |
|--------|---------|------------------|-------------------|
| `start_test.bat` | One-click start | ✅ Yes | ✅ Yes |
| `test_raw_iq.bat` | Interactive test | ❌ No | ✅ Yes |
| `quick_test.bat` | Automated test | ❌ No | ✅ Yes |
| `launch_fft_analyzer.bat` | Analyzer only | ✅ Yes | ❌ No |

---

## Quick Command Reference

### Start Everything (Recommended)
```bash
start_test.bat
```

### Different Rates
```bash
start_test.bat --rate 100000    # 100 kHz
start_test.bat --rate 1000000   # 1 MHz
start_test.bat --rate 10000000  # 10 MHz
```

### Different Signals
```bash
start_test.bat --signal sweep         # Frequency sweep
start_test.bat --signal noise         # White noise
start_test.bat --signal signal_noise  # Signal + noise
start_test.bat --signal multi         # Multi-tone
```

### Combine Options
```bash
start_test.bat --rate 10000000 --signal multi
```

---

## Summary

✅ **Easiest:** `start_test.bat` (one click, auto-opens browser)
✅ **Debugging:** `test_raw_iq.bat` (see console output)
✅ **Manual:** Two terminals (full control)

All methods now use **correct connection order**:
1. IQ server starts first (listens)
2. FFT analyzer connects to it
3. Browser opens (automatic with `start_test.bat`)

**No more connection errors!** 🎉
