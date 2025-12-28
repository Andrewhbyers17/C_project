# High-Speed Configuration Guide

## What's New - System Now Optimized for MHz Rates!

Your FFT analyzer is now configured for **high data rate** operation (10+ MHz) with:

✅ **Auto sample rate detection** - Automatically detects actual data rate from network
✅ **Dynamic decimation** - Adjusts display decimation based on detected rate
✅ **Large ring buffer** - 32 MB buffer (2000 frames × 4096 samples)
✅ **Fast FFT updates** - 100 Hz display rate (10ms updates)
✅ **Larger FFT size** - 4096 points for better frequency resolution

---

## Current Configuration

### Optimized for High Data Rates

```c
// In fft_analyzer_network.c:

#define FFT_SIZE            4096        // Was 512
#define SAMPLE_RATE         10000000    // Was 8000 (10 MHz default)
#define UPDATE_RATE_MS      10          // Was 50 (100 Hz updates)
#define RING_BUFFER_FRAMES  2000        // Was 100 (32 MB buffer!)

// Auto detection enabled
#define AUTO_DETECT_SAMPLE_RATE  true
#define TARGET_DISPLAY_RATE      20000  // Target 20 kHz for FFT display
```

### What This Means

**Ring Buffer:**
- Size: 2000 frames × 4096 samples × 4 bytes = **32 MB**
- At 10 MHz: ~819ms of buffering
- At 2 MHz: ~4 seconds of buffering
- At 100 kHz: ~81 seconds of buffering

**FFT Display:**
- 4096-point FFT = better frequency resolution
- Updates every 10ms (100 Hz) = smooth display
- Auto-decimation keeps display manageable at any rate

**Auto Sample Rate Detection:**
- Measures actual network data rate every second
- Adjusts decimation automatically
- Target: ~20 kHz effective display rate
- Records actual detected rate to HDF5 files

---

## How Auto Detection Works

### Example: 2 MHz Signal

```
[AUTO] Detected sample rate: 2000000 Hz (2.00 MHz)
[AUTO] Decimation factor: 100 (display rate: 20000 Hz)
[WEB] Starting RAW IQ streaming at 2000000 Hz (2.00 MHz)
```

**What happens:**
1. Network sends 2M samples/sec
2. System detects: "getting 2M samples per second"
3. Calculates decimation: 2,000,000 / 20,000 = 100
4. FFT display processes 1 out of every 100 samples
5. Disk writer records **ALL** 2M samples/sec
6. HDF5 file gets tagged with correct 2 MHz rate

### Example: 100 kHz Signal

```
[AUTO] Detected sample rate: 100000 Hz (0.10 MHz)
[AUTO] Decimation factor: 5 (display rate: 20000 Hz)
```

- Decimation: 100,000 / 20,000 = 5
- FFT processes 1 out of every 5 samples
- Display shows 20 kHz effective rate
- All 100 kHz recorded to disk

### Example: 8 kHz Signal

```
[AUTO] Detected sample rate: 8000 Hz (0.01 MHz)
[AUTO] Decimation factor: 1 (display rate: 8000 Hz)
```

- No decimation needed (8 kHz < 20 kHz target)
- All samples shown in FFT
- Works exactly as before

---

## Performance Benchmarks

### Updated Performance Table

| Sample Rate | Ring Buffer | Decimation | Display Rate | CPU Usage | Status |
|-------------|-------------|------------|--------------|-----------|--------|
| 8 kHz | 81 sec | 1:1 | 8 kHz | ~5% | ✅ Trivial |
| 100 kHz | 81 sec | 5:1 | 20 kHz | ~12% | ✅ Easy |
| 1 MHz | 8 sec | 50:1 | 20 kHz | ~28% | ✅ Comfortable |
| 2 MHz | 4 sec | 100:1 | 20 kHz | ~45% | ✅ Viable |
| 10 MHz | 819 ms | 500:1 | 20 kHz | ~75% | ✅ Ready! |
| 20 MHz | 410 ms | 1000:1 | 20 kHz | ~95% | ⚠️ Near limit |

**Note:** These are estimates. Actual performance depends on your CPU and disk speed.

---

## Testing High Data Rates

### Test 1: 100 kHz (Baseline)

```bash
# Terminal 1
./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp

# Terminal 2
python test_iq_sender.py --port 5000 --rate 100000 --signal sine --duration 30
```

**Expected console output:**
```
[AUTO] Detected sample rate: 100000 Hz (0.10 MHz)
[AUTO] Decimation factor: 5 (display rate: 20000 Hz)
```

**Result:**
- File size: ~12 MB (30 seconds × 100k samples/sec × 8 bytes)
- No buffer warnings
- Smooth FFT display

---

### Test 2: 2 MHz (High Speed)

```bash
# Terminal 2
python test_iq_sender.py --port 5000 --rate 2000000 --signal multi --duration 10
```

**Expected console output:**
```
[AUTO] Detected sample rate: 2000000 Hz (2.00 MHz)
[AUTO] Decimation factor: 100 (display rate: 20000 Hz)
```

**Result:**
- File size: ~160 MB (10 seconds × 2M samples/sec × 8 bytes)
- Should handle smoothly
- FFT still responsive (only processing 20 kHz worth)

---

### Test 3: 10 MHz (Maximum)

```bash
# Terminal 2
python test_iq_sender.py --port 5000 --rate 10000000 --signal sweep --duration 5
```

**Expected console output:**
```
[AUTO] Detected sample rate: 10000000 Hz (10.00 MHz)
[AUTO] Decimation factor: 500 (display rate: 20000 Hz)
```

**Result:**
- File size: ~400 MB (5 seconds × 10M samples/sec × 8 bytes)
- High CPU usage but manageable
- May need SSD for disk writes
- Watch for buffer warnings

---

## Buffer Health Monitoring

### Good Signs (No Issues)

```
[OK] Ring buffer allocated (2000 frames, 32.00 MB)
[OK] Network receiver thread started
[OK] Disk writer thread started
[AUTO] Detected sample rate: 2000000 Hz (2.00 MHz)
[AUTO] Decimation factor: 100 (display rate: 20000 Hz)
```

No warnings = system handling the load perfectly!

---

### Warning Signs

**Buffer Overflow:**
```
[WARN] Ring buffer overflow! Dropping 4096 samples
[WARN] Ring buffer overflow (10 times) - increase buffer or reduce sample rate
```

**Cause:** Network sending faster than disk can write

**Solutions:**
1. Use faster disk (SSD instead of HDD)
2. Enable larger ring buffer (increase `RING_BUFFER_FRAMES`)
3. Reduce sample rate from sender
4. Check CPU usage (may be maxed out)

---

**Buffer Underrun:**
```
[WARN] Ring buffer underrun (50 times) - network may be slow
```

**Cause:** Network slower than expected, or connection issues

**Solutions:**
1. Check network connection quality
2. Verify sender is running and sending data
3. Check for network congestion
4. Try reducing sender rate

---

## File Size Calculations

### Formula
```
Size (MB) = (Sample Rate × Duration × 8) / 1,048,576
```

Where:
- Sample Rate = samples per second
- Duration = seconds
- 8 = bytes per complex sample (4 for I, 4 for Q)
- 1,048,576 = bytes per MB

### Quick Reference Table

| Rate | 10 sec | 1 min | 10 min | 1 hour |
|------|--------|-------|--------|--------|
| 100 kHz | 7.6 MB | 45.8 MB | 458 MB | 2.7 GB |
| 1 MHz | 76.3 MB | 458 MB | 4.6 GB | 27.5 GB |
| 2 MHz | 152.6 MB | 915 MB | 9.2 GB | 55 GB |
| 10 MHz | 762.9 MB | 4.6 GB | 45.8 GB | 275 GB |

**Storage tip:** Use compression for long captures (edit data_logger.c to enable), reduces file size by 50-70%.

---

## Adjusting Target Display Rate

If you want a different display rate (default 20 kHz), edit:

```c
#define TARGET_DISPLAY_RATE      20000  // Change this
```

**Examples:**

- `10000` = 10 kHz display (less CPU, coarser display)
- `50000` = 50 kHz display (more CPU, finer display)
- `100000` = 100 kHz display (high CPU, very fine detail)

**After changing, rebuild:**
```bash
mingw32-make -f Makefile.windows clean
mingw32-make -f Makefile.windows
```

---

## Disabling Auto Detection

If you want to manually control sample rate and decimation:

```c
#define AUTO_DETECT_SAMPLE_RATE  false  // Disable auto detection
```

Then manually set:
```c
#define SAMPLE_RATE         2000000  // Your fixed rate
```

And calculate decimation:
```c
// In code, set manually:
g_decimation_factor = SAMPLE_RATE / TARGET_DISPLAY_RATE;
```

---

## Real-World SDR Integration

### With RTL-SDR (2.4 MHz)

```bash
# RTL-SDR streams at 2.4 MHz typically
# Your existing SDR software sends to TCP port 5000

./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp
```

**Expected:**
```
[AUTO] Detected sample rate: 2400000 Hz (2.40 MHz)
[AUTO] Decimation factor: 120 (display rate: 20000 Hz)
```

### With HackRF (up to 20 MHz)

```bash
# HackRF can do 2-20 MHz
./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp
```

**At 10 MHz:**
```
[AUTO] Detected sample rate: 10000000 Hz (10.00 MHz)
[AUTO] Decimation factor: 500 (display rate: 20000 Hz)
```

**At 20 MHz:** May need to increase ring buffer further

### With LimeSDR (up to 61.44 MHz)

For very high rates (20+ MHz), you'll need to:

1. **Increase ring buffer:**
   ```c
   #define RING_BUFFER_FRAMES  5000  // 40 MB buffer
   ```

2. **Ensure SSD storage** - NVMe preferred

3. **Monitor buffer health** - Watch for overflow warnings

4. **Consider compression** - Enable HDF5 compression for long captures

---

## System Requirements by Rate

### 100 kHz - 1 MHz (Low Speed)
- **CPU:** Any modern i3 or better
- **RAM:** 2 GB minimum
- **Disk:** HDD okay (10 MB/s write needed)
- **Network:** 100 Mbps fine

### 2 MHz - 5 MHz (Medium Speed)
- **CPU:** i5 or better
- **RAM:** 4 GB minimum
- **Disk:** SSD recommended (20 MB/s write)
- **Network:** 100 Mbps or better

### 10 MHz (High Speed)
- **CPU:** i7 or Ryzen 7
- **RAM:** 8 GB minimum
- **Disk:** NVMe SSD (100 MB/s write)
- **Network:** Gigabit Ethernet

### 20+ MHz (Maximum Speed)
- **CPU:** High-end i7/i9 or Ryzen 9
- **RAM:** 16 GB minimum
- **Disk:** NVMe SSD (200+ MB/s write)
- **Network:** Gigabit Ethernet
- **Note:** May need additional optimization

---

## Advanced: Tweaking for Maximum Performance

### 1. Increase Ring Buffer Further

For 20+ MHz:
```c
#define RING_BUFFER_FRAMES  10000  // 160 MB buffer!
```

Trade-off: Uses more RAM but prevents overflows.

### 2. Reduce FFT Update Rate

Less frequent updates = more CPU for disk writes:
```c
#define UPDATE_RATE_MS      20  // 50 Hz instead of 100 Hz
```

### 3. Smaller FFT Size

Faster FFT computation:
```c
#define FFT_SIZE            2048  // Half the points
```

Trade-off: Less frequency resolution.

### 4. Enable Compression

For long captures (edit data_logger.c line 771):
```c
H5Pset_deflate(prop, 6);  // Enable compression
```

Trade-off: 50-70% smaller files but slower writes.

---

## Summary of Changes

### Before (Original Config)
```c
FFT_SIZE: 512
SAMPLE_RATE: 8000 Hz
RING_BUFFER_FRAMES: 100 (~200 KB)
UPDATE_RATE_MS: 50 (20 Hz)
Auto detection: NO
Decimation: Fixed at 1:1
```

**Good for:** Audio-rate signals (8 kHz)
**Struggles with:** Anything above 100 kHz

---

### After (New Config)
```c
FFT_SIZE: 4096
SAMPLE_RATE: 10 MHz (auto-detected)
RING_BUFFER_FRAMES: 2000 (~32 MB)
UPDATE_RATE_MS: 10 (100 Hz)
Auto detection: YES
Decimation: Dynamic (1 to 500+)
```

**Good for:** 8 kHz to 10+ MHz
**Optimal at:** 1-10 MHz range
**Still works for:** Audio (8 kHz) with auto-detection

---

## Verification Test

Run this to verify everything works:

```bash
# Quick test at multiple rates
python test_iq_sender.py --port 5000 --rate 100000 --signal sine --duration 5
# Watch for: "[AUTO] Detected sample rate: 100000 Hz"

python test_iq_sender.py --port 5000 --rate 2000000 --signal sine --duration 5
# Watch for: "[AUTO] Detected sample rate: 2000000 Hz"
```

Both should show correct detection and appropriate decimation!

---

## Need Even Higher Rates?

For 20-50 MHz and beyond, contact for optimization options:
- Lock-free ring buffer (atomic operations)
- SIMD-optimized FFT
- Multi-threaded disk writing
- GPU FFT offloading
- Direct SDR hardware integration

The current architecture can handle it with additional optimization!
