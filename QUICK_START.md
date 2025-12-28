# Quick Start - Correct Order!

## ⚠️ Important: Start Order Matters!

The FFT analyzer **connects TO** the data source (it's a client, not a server).

So you need to start the **data server FIRST**, then the **analyzer connects to it**.

---

## Correct Order (Two Terminals)

### Terminal 1: Start Data Server FIRST

```bash
python test_iq_server.py --port 5000 --rate 2000000 --signal sine
```

**Output:**
```
[*] IQ Data Server listening on port 5000
[*] Sample rate: 2000000 Hz (2.00 MHz)
[*] Signal type: sine
[*] Waiting for FFT analyzer to connect...
```

**Leave this running!**

---

### Terminal 2: Start Analyzer SECOND

```bash
./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp
```

**Output:**
```
[*] Connecting to 127.0.0.1:5000 (TCP)...
[OK] Connected to 127.0.0.1:5000
[AUTO] Detected sample rate: 2000000 Hz (2.00 MHz)
[AUTO] Decimation factor: 100 (display rate: 20000 Hz)
```

**Now open browser to http://localhost:8080**

---

## Automatic Test (One Command)

The easiest way:

```bash
quick_test.bat
```

This automatically:
1. Starts the data server
2. Waits for it to be ready
3. Starts the analyzer (connects to server)
4. Runs for 15 seconds
5. Cleans up

---

## Why This Order?

```
WRONG Order (won't work):
┌─────────────┐
│ Analyzer    │ --tries to connect--> [Nothing listening!]
│ (Client)    │                        ❌ Error 10061
└─────────────┘

CORRECT Order:
┌─────────────┐                       ┌─────────────┐
│ Data Server │ <--connects to--      │ Analyzer    │
│ (Listening) │                       │ (Client)    │
└─────────────┘                       └─────────────┘
      ↑                                      |
      |    1. Server starts listening        |
      |       on port 5000                   |
      |                                      |
      +--------------------------------------+
         2. Analyzer connects to port 5000
```

---

## Different Sample Rates

### Audio Rate (8 kHz)
```bash
# Terminal 1
python test_iq_server.py --port 5000 --rate 8000 --signal sine

# Terminal 2
./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp
```

Expected: `Decimation factor: 1` (no decimation)

---

### Medium Rate (100 kHz)
```bash
# Terminal 1
python test_iq_server.py --port 5000 --rate 100000 --signal sweep

# Terminal 2
./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp
```

Expected: `Decimation factor: 5`

---

### High Rate (10 MHz)
```bash
# Terminal 1
python test_iq_server.py --port 5000 --rate 10000000 --signal multi

# Terminal 2
./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp
```

Expected: `Decimation factor: 500`

---

## Signal Types

Available test signals:

| Signal | Description |
|--------|-------------|
| `sine` | 1 kHz pure tone (default) |
| `sweep` | 100 Hz to 4 kHz chirp |
| `noise` | White noise |
| `signal_noise` | 1 kHz + noise (10 dB SNR) |
| `multi` | 500 Hz + 1 kHz + 2 kHz |

---

## Recording to HDF5

Once connected:

1. **Open browser:** http://localhost:8080
2. **Click:** "Start Logging"
3. **Select:** "raw_iq" format
4. **Click:** "Confirm"

Console shows:
```
[AUTO] Detected sample rate: 2000000 Hz (2.00 MHz)
[WEB] Starting RAW IQ streaming at 2000000 Hz (2.00 MHz)
[OK] Disk writer thread started
```

Files appear in `logs/` directory:
```
logs/iq_data_20251227_123456Z.h5
```

---

## Stopping

### Stop Recording
Click "Stop Logging" in web interface

### Stop Everything
Press **Ctrl+C** in both terminals

---

## Troubleshooting

### Error: "Connection refused" (10061)

**Problem:** Analyzer trying to connect but nothing listening

**Solution:** Start the **server first**!

```bash
# FIRST - Start server
python test_iq_server.py --port 5000 --rate 2000000 --signal sine

# SECOND - Start analyzer
./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp
```

---

### Error: "Address already in use"

**Problem:** Port 5000 already occupied

**Solution 1:** Stop previous test
```bash
# Kill any running test servers
taskkill /FI "WINDOWTITLE eq *IQ Server*" /F
```

**Solution 2:** Use different port
```bash
# Server
python test_iq_server.py --port 5001 --rate 2000000 --signal sine

# Analyzer
./fft_analyzer_network.exe --source 127.0.0.1:5001 --protocol tcp
```

---

### No Auto-Detection

**Problem:** Always shows default 10 MHz

**Wait:** Auto-detection runs every second

**Check:** Is server actually sending?
```
# Server terminal should show:
[*] Sent 512000 I/Q pairs (1.0s) - 512000 samples/sec
```

If not sending, check connection!

---

### Web Interface Not Opening

**Problem:** Can't access http://localhost:8080

**Check:** Is analyzer running?
```
# Should see:
[OK] Web server started on port 8080
```

**Try:** Explicit browser command
```bash
start http://localhost:8080
```

---

## Summary

✅ **Start order:**
1. Server first (`test_iq_server.py`)
2. Analyzer second (`fft_analyzer_network.exe`)

✅ **Or use:** `quick_test.bat` (automatic)

✅ **Then:** Open http://localhost:8080

✅ **Record:** Select "raw_iq" format

That's it!
