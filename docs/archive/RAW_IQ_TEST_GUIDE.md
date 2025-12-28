# Raw IQ Recording Test Guide

## Quick Start

### 1. Start the FFT Analyzer

Open a terminal and run:
```bash
./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp
```

This will:
- Start the FFT analyzer waiting for network input
- Launch web interface at http://localhost:8080
- Wait for connection on port 5000

### 2. Send Test Data

Open **another terminal** and run:
```bash
python test_iq_sender.py --port 5000 --rate 8000 --signal sine
```

This will:
- Connect to the analyzer
- Send 8 kHz I/Q data with a 1 kHz sine wave
- Continue sending until you press Ctrl+C

### 3. Start Raw IQ Recording

1. Open browser to http://localhost:8080
2. Click **"Start Logging"** button
3. Select format: **"raw_iq"**
4. Click **"Confirm"**

You should see:
- Console message: `[WEB] Starting RAW IQ streaming`
- Console message: `[OK] Disk writer thread created`
- File created in `logs/iq_data_YYYYMMDD_HHMMSSZ.h5`

### 4. Stop Recording

1. Click **"Stop Logging"** in web interface
2. Console will show:
   ```
   [*] Stopping disk writer thread...
   [OK] Disk writer thread stopped
   [LOGGER] Stopped logging. Wrote XXXXXX IQ samples to: logs/iq_data_*.h5
   ```

### 5. Verify the File

```bash
python read_iq_file.py logs/iq_data_*.h5
```

You should see:
- Sample rate: 8,000 Hz
- Format: complex_interleaved_float32
- Duration in seconds
- File size in MB
- Statistics (mean, std dev, peak)

---

## Test Scenarios

### Scenario 1: Audio Rate (8 kHz)

**Purpose:** Verify basic functionality

**Steps:**
```bash
# Terminal 1
./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp

# Terminal 2
python test_iq_sender.py --port 5000 --rate 8000 --signal sine --duration 10
```

**Expected:**
- Smooth FFT display in web interface
- 10 seconds of data recorded
- File size: ~320 KB (8000 samples/sec × 2 I/Q × 4 bytes × 10 sec)

---

### Scenario 2: Ultrasonic (100 kHz)

**Purpose:** Verify higher data rates

**Steps:**
```bash
# Terminal 2
python test_iq_sender.py --port 5000 --rate 100000 --signal sweep --duration 10
```

**Expected:**
- File size: ~4 MB
- Sweep from 100 Hz to 4 kHz visible in FFT
- No buffer overflow warnings

---

### Scenario 3: RF Low (1 MHz)

**Purpose:** Verify MHz-range capability

**Steps:**
```bash
# Terminal 2
python test_iq_sender.py --port 5000 --rate 1000000 --signal signal_noise --duration 5
```

**Expected:**
- File size: ~40 MB
- 1 kHz tone visible above noise floor
- Web interface still responsive
- No overflow warnings

---

### Scenario 4: RF High (2 MHz)

**Purpose:** Stress test

**Steps:**
```bash
# Terminal 2
python test_iq_sender.py --port 5000 --rate 2000000 --signal multi --duration 5
```

**Expected:**
- File size: ~80 MB
- Three tones visible (500 Hz, 1 kHz, 2 kHz)
- System should handle this smoothly
- Watch for buffer warnings

---

## Analysis Examples

### View File Information

```bash
python read_iq_file.py logs/iq_data_20251227_123456Z.h5
```

Output:
```
============================================================
File: logs/iq_data_20251227_123456Z.h5
============================================================
Sample rate:    8,000 Hz (8.0 kHz)
Start time:     1735308896 (Unix timestamp)
Format:         complex_interleaved_float32
Dataset shape:  (80000,)
Dataset dtype:  [('r', '<f4'), ('i', '<f4')]
Complex samples: 80,000
Duration:       10.000 seconds
Data size:      0.61 MB

Compound type detected (native HDF5 complex)

First 10000 samples statistics:
  I (Real):      mean=0.000123, std=0.707123
  Q (Imaginary): mean=-0.000456, std=0.707089
  Magnitude:     mean=0.998234, max=1.000012
  Phase (deg):   mean=0.3, std=89.7
```

### Compute FFT Spectrum

```bash
python read_iq_file.py logs/iq_data_*.h5 --fft --samples 8192
```

Output:
```
FFT Analysis (8192 points):
  Peak frequency: 1000.00 Hz (1.000 kHz)
  Peak magnitude: 89.23 dB

Top 5 frequency components:
    1000.0 Hz:  89.23 dB
    -1000.0 Hz:  89.22 dB
      42.5 Hz: -45.67 dB
    -127.3 Hz: -48.91 dB
     215.8 Hz: -52.34 dB
```

### Plot Data (requires matplotlib)

```bash
python read_iq_file.py logs/iq_data_*.h5 --plot --samples 1024
```

This will show:
- I/Q time domain traces
- Signal magnitude over time
- I/Q constellation diagram
- FFT spectrum

---

## Reading Data in Python

### Simple Example

```python
import h5py
import numpy as np

# Open file
f = h5py.File('logs/iq_data_20251227_123456Z.h5', 'r')

# Read metadata
sample_rate = f.attrs['sample_rate']
start_time = f.attrs['start_time']

# Read IQ data
iq_data = f['/iq_samples'][:]

# Convert to complex
iq_complex = iq_data['r'] + 1j * iq_data['i']

# Now you can use iq_complex for analysis
fft = np.fft.fft(iq_complex)
# ... your processing here ...

f.close()
```

### Full Analysis Example

```python
import h5py
import numpy as np
import matplotlib.pyplot as plt

# Read file
f = h5py.File('logs/iq_data_20251227_123456Z.h5', 'r')
sample_rate = f.attrs['sample_rate']
iq_data = f['/iq_samples'][:]
iq_complex = iq_data['r'] + 1j * iq_data['i']
f.close()

# Compute spectrogram
from scipy import signal
frequencies, times, Sxx = signal.spectrogram(
    iq_complex,
    fs=sample_rate,
    nperseg=1024,
    noverlap=512
)

# Plot
plt.figure(figsize=(12, 6))
plt.pcolormesh(times, frequencies/1e3, 10*np.log10(Sxx), shading='gouraud')
plt.ylabel('Frequency (kHz)')
plt.xlabel('Time (s)')
plt.title('Spectrogram')
plt.colorbar(label='Power (dB)')
plt.show()
```

---

## Signal Types Reference

### Available Test Signals

| Signal Type | Description | Use Case |
|-------------|-------------|----------|
| `sine` | 1 kHz pure tone | Basic functionality test |
| `sweep` | 100 Hz to 4 kHz chirp | Frequency response test |
| `noise` | White noise | Noise floor measurement |
| `signal_noise` | 1 kHz + noise (10 dB SNR) | SNR test |
| `multi` | 500 Hz + 1 kHz + 2 kHz | Multi-tone test |

### Custom Signals

Edit `test_iq_sender.py` to add your own signal generators. The format is:

```python
def generate_my_signal(sample_rate, num_samples):
    samples = []
    for i in range(num_samples):
        t = i / float(sample_rate)
        # Your signal here
        i_val = math.cos(2 * math.pi * freq * t)
        q_val = math.sin(2 * math.pi * freq * t)
        samples.extend([i_val, q_val])
    return samples
```

---

## Performance Monitoring

### Check for Issues

While recording, watch the console for:

**Good (no issues):**
```
[OK] Disk writer thread started
[OK] Network receiver thread started
# ... no warnings ...
```

**Warning (buffer pressure):**
```
[WARN] Ring buffer overflow! Dropping 512 samples
[WARN] Ring buffer overflow (100 times) - increase buffer or reduce sample rate
```
**Fix:** Increase `RING_BUFFER_FRAMES` in fft_analyzer_network.c

**Error (network slow):**
```
[WARN] Ring buffer underrun (100 times) - network may be slow
```
**Fix:** Check network connection, reduce sender rate

**Error (disk too slow):**
```
[ERROR] Failed to write IQ data to disk
[LOGGER] HDF5 Error: Failed to write IQ data (65536 samples)
```
**Fix:** Use faster disk (SSD), or reduce sample rate

---

## Expected File Sizes

| Sample Rate | Duration | File Size (uncompressed) |
|-------------|----------|--------------------------|
| 8 kHz | 10 sec | 320 KB |
| 8 kHz | 1 min | 1.9 MB |
| 100 kHz | 10 sec | 4 MB |
| 100 kHz | 1 min | 24 MB |
| 1 MHz | 10 sec | 40 MB |
| 1 MHz | 1 min | 240 MB |
| 2 MHz | 10 sec | 80 MB |
| 2 MHz | 1 min | 480 MB |

**Formula:** `size_bytes = sample_rate × duration_sec × 2 (I/Q) × 4 (bytes per float)`

---

## Troubleshooting

### Issue: "Connection refused"

**Symptom:**
```
[ERROR] Connection failed: [Errno 111] Connection refused
```

**Fix:**
1. Make sure FFT analyzer is running first
2. Check port number matches (default 5000)
3. Check firewall settings

---

### Issue: "No module named 'h5py'"

**Symptom:**
```
ModuleNotFoundError: No module named 'h5py'
```

**Fix:**
```bash
pip install h5py
pip install numpy  # also needed
```

---

### Issue: Web interface shows no data

**Symptom:** FFT display is blank or frozen

**Fix:**
1. Check that test sender is running and connected
2. Look for "Connected" message in analyzer console
3. Try stopping and restarting sender
4. Check sample rate matches expected (8 kHz default)

---

### Issue: Huge file sizes

**Symptom:** HDF5 files growing too large

**Solutions:**
1. **Reduce duration** - record shorter segments
2. **Lower sample rate** - use 8 kHz instead of MHz for testing
3. **Enable compression** - edit data_logger.c line 808, uncomment:
   ```c
   H5Pset_deflate(prop, 6);  // Enable gzip compression
   ```
   This reduces file size by 50-70% but increases CPU usage

---

## Advanced Usage

### Continuous Recording

For long-duration recording:
```bash
# Record for 1 hour at 100 kHz
python test_iq_sender.py --port 5000 --rate 100000 --signal sine --duration 3600
```

File size will be: 100,000 × 3600 × 2 × 4 = **2.88 GB**

### Multiple Sessions

The analyzer creates unique filenames with timestamps:
```
logs/iq_data_20251227_120000Z.h5
logs/iq_data_20251227_123000Z.h5
logs/iq_data_20251227_130000Z.h5
```

You can start/stop recording multiple times without overwriting.

### Network Recording

To record from a remote source:
```bash
# On analyzer machine
./fft_analyzer_network.exe --source 192.168.1.100:5000 --protocol tcp

# On remote machine with SDR
python test_iq_sender.py --host 192.168.1.50 --port 5000 --rate 2000000 --signal sine
```

---

## Summary

✅ Raw IQ recording integrated
✅ Web interface controls ready
✅ Test scripts provided
✅ File verification tools included
✅ Handles MHz sample rates

**Next Steps:**
1. Run basic test (8 kHz sine wave)
2. Verify file with `read_iq_file.py`
3. Try higher sample rates
4. Monitor for buffer warnings
5. Adjust `RING_BUFFER_FRAMES` if needed
