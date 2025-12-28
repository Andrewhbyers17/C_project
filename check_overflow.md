# Ring Buffer Overflow Diagnostic Guide

## Quick Checks

Run the newly built analyzer and look for these indicators:

### 1. Check Auto-Detection
```
[RATE] Auto-detected sample rate: 10000000 Hz (10.00 MHz)
```
If you don't see this, auto-detection failed and it's using the wrong decimation.

### 2. Check Drain Buffer Size
The latest build should show **128K samples** in drain buffer (8× network buffer).

### 3. Monitor Overflow Pattern
**Immediate overflow (< 5 sec):**
- Problem: Drain rate too slow
- Solution: Increase drain buffer or update rate

**Gradual overflow (> 30 sec):**
- Problem: Slow memory leak or FFT taking too long
- Solution: Optimize processing loop

**Bursty overflow (occasional):**
- Problem: TCP retransmissions or network jitter
- Solution: Increase ring buffer size

## Current Configuration

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Ring Buffer** | 5000 frames (80 MB) | Total capacity |
| **Network Buffer** | 16K samples (64 KB) | Per recv() call |
| **Drain Buffer** | 128K samples (512 KB) | Per display update |
| **Update Rate** | 100 Hz (10ms) | Display refresh |
| **Drain Rate** | 12.8M samples/sec | 128K × 100 Hz |

## Sample Rate vs Drain Rate

| Sample Rate | Input | Drain Capacity | Headroom |
|-------------|-------|----------------|----------|
| 2 MHz | 2M/sec | 12.8M/sec | **540%** ✅ |
| 10 MHz | 10M/sec | 12.8M/sec | **28%** ✅ |
| 15 MHz | 15M/sec | 12.8M/sec | **-15%** ❌ |
| 20 MHz | 20M/sec | 12.8M/sec | **-36%** ❌ |

## If Still Overflowing at 10 MHz

### Possibility 1: FFT Taking Too Long
The 10ms update cycle might be exceeded by FFT computation.

**Test:** Reduce FFT size temporarily
```c
#define FFT_SIZE 2048  // Was 4096 - half the size
```
If overflow stops, FFT is the bottleneck.

### Possibility 2: Not Draining Every Cycle
Check if the main loop is actually running at 100 Hz.

**Test:** Add timing debug
```c
static LARGE_INTEGER last_time;
LARGE_INTEGER now, freq;
QueryPerformanceCounter(&now);
QueryPerformanceFrequency(&freq);
if (last_time.QuadPart != 0) {
    double elapsed = (now.QuadPart - last_time.QuadPart) / (double)freq.QuadPart;
    if (elapsed > 0.020) {  // > 20ms = problem!
        printf("[WARN] Main loop slow: %.1f ms\n", elapsed * 1000);
    }
}
last_time = now;
```

### Possibility 3: Recording Mode Active
Recording uses a different code path (disk writer thread).

**Test:** Make sure you're NOT recording when testing overflow.

### Possibility 4: Sample Rate Higher Than Expected
C generator might be sending faster than configured.

**Test:** Check generator output
```
[STATS] Rate: 10.50 MHz  // Should match --rate parameter
```

## Solutions

### Solution 1: Increase Update Rate (Faster Draining)
```c
#define UPDATE_RATE_MS 5  // Was 10 - doubles drain frequency
```
New drain rate: 128K × 200 Hz = **25.6M samples/sec** (handles 20 MHz!)

### Solution 2: Larger Drain Buffer (More Per Cycle)
```c
static float drain_buffer[NETWORK_BUFFER_SIZE * 16];  // 256K samples
```
New drain rate: 256K × 100 Hz = **25.6M samples/sec** (handles 20 MHz!)

### Solution 3: Dedicated Drain Thread (Continuous)
Create a separate thread that continuously drains, not tied to display updates.

```c
DWORD WINAPI drain_thread(LPVOID param) {
    float temp[NETWORK_BUFFER_SIZE];
    while (g_running) {
        ring_buffer_read(&g_ring_buffer, temp, NETWORK_BUFFER_SIZE);
        usleep(1000);  // 1ms sleep = 1000 Hz drain rate
    }
    return 0;
}
```
Drain rate: 16K × 1000 Hz = **16M samples/sec** (continuous, handles 15 MHz)

## Recommended Next Steps

1. **First:** Verify actual sample rate being generated
   ```bash
   # Generator should print actual rate
   ./iq_generator.exe --rate 10000000 --port 5000
   # Look for: "Rate: XX.XX MHz"
   ```

2. **Second:** Test at lower rate to confirm fix works
   ```bash
   ./iq_generator.exe --rate 2000000 --port 5000
   # Should have ZERO overflow at 2 MHz
   ```

3. **Third:** If 2 MHz works but 10 MHz doesn't:
   - Use Solution 1 or 2 above
   - Rebuild and retest

4. **Fourth:** If even 2 MHz overflows:
   - Something else is wrong (maybe recording mode active?)
   - Check for other processes using CPU
   - Check disk I/O if recording

## Expected Behavior After Fix

**At 2 MHz:** Zero overflow, <5% CPU
**At 10 MHz:** Zero overflow, <20% CPU
**At 20 MHz:** Possible occasional overflow, ~30% CPU
**Above 20 MHz:** Expected overflow (exceeds design limits)
