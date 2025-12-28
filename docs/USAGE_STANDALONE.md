# FFT Analyzer - Standalone Usage Guide

## New Feature: Start Analyzer First!

The analyzer now supports **indefinite reconnection** - you can start it first and connect the data source later.

---

## Quick Start (Standalone Mode)

### Method 1: Start Analyzer First

**Step 1:** Start the analyzer
```bash
launch_analyzer_standalone.bat
```

**What happens:**
- FFT analyzer starts immediately
- Web browser opens to http://localhost:8080
- Console shows: "Waiting for network source to become available..."
- Analyzer retries connection every 5 seconds indefinitely

**Step 2:** Start data source whenever you're ready
```bash
python test_iq_server.py --port 5000 --rate 2000000 --signal sine
```

**Result:** Analyzer automatically connects and starts processing data!

---

### Method 2: Traditional Way (Source First)

```bash
start_test.bat
```

This still works as before - starts source first, then analyzer connects.

---

## Usage Examples

### Example 1: Start Analyzer, Connect Later

```bash
# Terminal 1: Start analyzer (no data source yet)
launch_analyzer_standalone.bat

# Opens browser showing "Initializing..." or test waveforms
# Console shows: "[*] Waiting for network source to become available..."

# Terminal 2: Start data source (5 minutes later, 5 hours later - doesn't matter!)
python test_iq_server.py --port 5000 --rate 2000000 --signal sine

# Analyzer immediately shows: "[OK] Reconnected successfully"
# Web interface updates with real data
```

---

### Example 2: Stop and Restart Data Source

```bash
# Analyzer already running, data source connected

# Stop data source (Ctrl+C in Terminal 2)
# Analyzer shows: "[ERROR] Network thread: Connection lost"
#                 "[*] Attempting to connect to 127.0.0.1:5000..."

# Restart data source
python test_iq_server.py --port 5000 --rate 10000000 --signal multi

# Analyzer shows: "[OK] Reconnected successfully"
# Continues processing with new data
```

---

### Example 3: Remote Data Source

```bash
# On your PC: Start analyzer waiting for remote source
launch_analyzer_standalone.bat --source-ip 192.168.1.100 --source-port 5000

# On remote machine (192.168.1.100): Start data source
python test_iq_server.py --port 5000 --rate 2000000 --signal sweep

# Analyzer automatically connects across network
```

---

## Reconnection Behavior

### Automatic Reconnection
- **Retry interval:** 5 seconds
- **Max retries:** Unlimited (INT_MAX)
- **Messages:**
  - First 3 attempts: Shows each attempt
  - After 3: Shows reminder every 10 attempts
  - On success: Shows total attempts needed

### Console Output

**Waiting for connection:**
```
[*] Attempting to connect to 127.0.0.1:5000 (TCP)...
[*] Waiting for network source to become available...
[*] Auto-reconnect enabled. Will retry every 5 seconds.
[*] You can start the data source anytime - analyzer will connect automatically.
[*] Using test waveforms until network connection established.
```

**After first connection:**
```
[*] Attempting to connect to 127.0.0.1:5000...
[*] Attempting to connect to 127.0.0.1:5000...
[OK] Reconnected successfully after 2 attempt(s)
[AUTO] Detected sample rate: 2000000 Hz (2.00 MHz)
```

**Long wait (periodic reminders):**
```
[*] Still waiting for 127.0.0.1:5000 to become available...
```
(Shows every 10th attempt to avoid spam)

---

## Web Interface Behavior

### Before Connection
- Shows "Mode: Initializing..." or test waveforms
- No "Connection lost" error (fixed!)
- Sample rate shows 0 Hz or test value

### After Connection
- Updates to "Mode: Network Input"
- Shows actual sample rate
- Displays real FFT/PSD data

### If Connection Lost
- Shows last valid data briefly
- Then switches to test waveforms
- Mode shows "Network Input" (reconnecting in background)

---

## Command Line Options

### Analyzer (Standalone)
```bash
fft_analyzer_network.exe --source IP:PORT --protocol tcp --port WEB_PORT
```

**Options:**
- `--source IP:PORT` - Data source address
- `--protocol tcp/udp` - Network protocol (default: tcp)
- `--port PORT` - Web server port (default: 8080)
- `--help` - Show help

### Data Source (IQ Server)
```bash
python test_iq_server.py --port PORT --rate RATE --signal TYPE [--duration SEC]
```

**Options:**
- `--port PORT` - Listen port (default: 5000)
- `--rate RATE` - Sample rate in Hz (e.g., 2000000)
- `--signal TYPE` - sine, sweep, noise, signal_noise, multi
- `--duration SEC` - Optional: Stop after N seconds

---

## Troubleshooting

### "Still waiting for source..."

**Problem:** Analyzer keeps retrying, never connects

**Check:**
```bash
# Is the source actually running?
python test_iq_server.py --port 5000 --rate 2000000 --signal sine

# Should show:
# [*] IQ Data Server listening on port 5000
```

**Verify:**
- Source port matches analyzer's --source port
- No firewall blocking (especially for remote connections)
- Source is listening on correct interface (0.0.0.0 for remote)

---

### "Connection lost" immediately after connecting

**Problem:** Connects briefly, then disconnects

**Possible causes:**
1. **Source stopped** - Check if `test_iq_server.py` exited
2. **Duration limit** - Remove `--duration` flag from source
3. **Network issue** - Check network stability for remote connections

---

### Web interface shows "Initializing..." forever

**Problem:** Analyzer running but web interface stuck

**Check:**
1. **Is analyzer connected?** Look at console output
2. **Buffer overflow?** Check for "[WARN] Ring buffer overflow" messages
3. **Data flowing?** Source should show "Sent XXXXX I/Q pairs"

**Fix:**
- If buffer overflow: System can't keep up, reduce sample rate
- If no data: Check source is actually sending (should show data rate)

---

## Workflow Recommendations

### Development / Testing
```bash
# Start analyzer first (keeps running)
launch_analyzer_standalone.bat

# Test different configurations
python test_iq_server.py --port 5000 --rate 100000 --signal sine
# Ctrl+C
python test_iq_server.py --port 5000 --rate 2000000 --signal sweep
# Ctrl+C
python test_iq_server.py --port 5000 --rate 10000000 --signal multi
```

**Benefit:** No need to restart analyzer between tests!

---

### Production / Long Recording
```bash
# Use traditional start_test.bat (everything starts together)
start_test.bat --rate 2000000 --signal sine

# Or start analyzer first, then source
launch_analyzer_standalone.bat
python test_iq_server.py --port 5000 --rate 2000000 --signal sine
```

**Benefit:** Choose the workflow that fits your needs!

---

## Summary

✅ **Analyzer can start first** - No need to start source first anymore
✅ **Unlimited reconnection** - Retries forever, not just 5 times
✅ **Stop/restart source** - Analyzer automatically reconnects
✅ **Remote connections** - Works across network
✅ **No error spam** - Shows periodic reminders, not every retry
✅ **Web interface fix** - No "Connection lost" on startup

The system is now much more flexible and user-friendly!
