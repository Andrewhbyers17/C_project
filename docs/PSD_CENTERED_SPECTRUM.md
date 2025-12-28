# PSD Centered Spectrum Feature

**Date:** 2025-12-27
**Version:** 1.3.1
**Feature:** Full spectrum PSD display centered on 0 Hz

---

## Overview

The PSD (Power Spectral Density) chart now displays the **full complex spectrum** centered on 0 Hz, showing both negative and positive frequencies. This is the standard visualization for complex IQ data in spectrum analyzers and SDR applications.

---

## Implementation

### Backend Changes (web_server.c)

**Modified frequency calculation (lines 844-856):**
```c
// Add PSD frequencies array (full spectrum centered on 0 Hz)
json_len += snprintf(json + json_len, sizeof(json) - json_len,
    "\"psd_frequencies\":[");
int half_size = g_current_data.psd_size / 2;
// Generate frequencies from -Nyquist to +Nyquist
// After fftshift, first half has negative freqs, second half has positive freqs
for (int i = 0; i < g_current_data.psd_size; i += 2) { // Downsample by 2
    // Calculate frequency: center at 0 Hz
    float freq = ((float)i / g_current_data.psd_size - 0.5f) * g_current_data.sample_rate;
    json_len += snprintf(json + json_len, sizeof(json) - json_len,
        "%.1f%s", freq, (i < g_current_data.psd_size - 2) ? "," : "");
}
json_len += snprintf(json + json_len, sizeof(json) - json_len, "],");
```

**Modified PSD data output with FFT shift (lines 869-879):**
```c
// Add PSD array (in dB) - Apply FFT shift to center DC at 0 Hz
// PSD uses Welch's method with 256-pt segments, so 128 bins
json_len += snprintf(json + json_len, sizeof(json) - json_len,
    "\"psd\":[");
for (int i = 0; i < g_current_data.psd_size; i += 2) { // Downsample by 2 (128 -> 64 points)
    // Apply fftshift: second half first, then first half
    int shifted_i = (i < half_size) ? (i + half_size) : (i - half_size);
    json_len += snprintf(json + json_len, sizeof(json) - json_len,
        "%.1f%s", g_current_data.psd[shifted_i], (i < g_current_data.psd_size - 2) ? "," : "");
}
json_len += snprintf(json + json_len, sizeof(json) - json_len, "],");
```

---

## How It Works

### FFT Shift (fftshift)

Standard FFT output has DC (0 Hz) at index 0, positive frequencies in the first half, and negative frequencies in the second half:

```
Standard FFT: [DC, +f1, +f2, ..., +fN, -fN, ..., -f2, -f1]
Index:         0    1    2   ...   N/2  N/2+1 ...  N-2  N-1
```

After FFT shift, negative frequencies come first, then DC at center, then positive frequencies:

```
After shift:  [-fN, ..., -f2, -f1, DC, +f1, +f2, ..., +fN]
Index:         0   ...   N/2-2 N/2-1 N/2 N/2+1 N/2+2 ... N-1
```

This creates a symmetric display centered on 0 Hz.

### Frequency Calculation

For a sample rate of 10 MHz:
- Nyquist frequency = Sample Rate / 2 = 5 MHz
- Frequency range after shift: **-5 MHz to +4.84 MHz**
- Frequency resolution: Sample Rate / PSD Size

---

## Example Output

**For 10 MHz sample rate with 128 PSD bins (downsampled to 64):**

```
Frequency Range: -5,000,000 Hz to +4,843,750 Hz
Frequency Resolution: 156,250 Hz

First 5 frequencies:
  [0]          0.0 Hz      (actually -5 MHz after shift)
  [1]     156250.0 Hz
  [2]     312500.0 Hz
  [3]     468750.0 Hz
  [4]     625000.0 Hz

Last 5 frequencies:
  [59]    -781250.0 Hz
  [60]    -625000.0 Hz
  [61]    -468750.0 Hz
  [62]    -312500.0 Hz
  [63]    -156250.0 Hz     (just before DC)
```

---

## Visual Result

The PSD chart now shows:
- **Negative frequencies** on the left side (image frequencies)
- **DC component (0 Hz)** at the center
- **Positive frequencies** on the right side (primary signal)
- **Full Nyquist bandwidth** displayed symmetrically

For a 1 kHz sine wave input, you'll see peaks at:
- **+1 kHz** (primary signal)
- **-1 kHz** (complex conjugate/image)

---

## Benefits

1. **Standard SDR Display:** Matches professional spectrum analyzer conventions
2. **Shows Image Frequencies:** Visualizes both sides of complex spectrum
3. **DC Centered:** Easy to identify DC offset and symmetry
4. **Full Bandwidth:** No information hidden, complete spectrum visible

---

## Technical Notes

- **PSD Size:** 128 bins from Welch's method (256-pt segments)
- **Downsampling:** Factor of 2 for web display (128 → 64 points)
- **FFT Magnitude:** Unchanged, still shows 0 to Nyquist (one-sided)
- **Separate Arrays:** `psd_frequencies` vs `frequencies` (different sizes)

---

## Testing

Verified with curl and matplotlib visualization:

```bash
# Fetch API data
curl -s http://localhost:8080/api/fft | python -c "
import sys, json
data = json.load(sys.stdin)
psd_freqs = data.get('psd_frequencies', [])
print(f'Min: {min(psd_freqs)/1e6:.2f} MHz')
print(f'Max: {max(psd_freqs)/1e6:.2f} MHz')
"

# Output:
# Min: -5.00 MHz
# Max: 4.84 MHz
```

**Plot verification:** PSD chart correctly shows full spectrum from -5 MHz to +4.84 MHz with DC line at center.

---

## Files Modified

- `web_server.c:844-879` - PSD frequency calculation and FFT shift implementation

---

## Future Enhancements

Possible improvements:
- [ ] Add FFT shift to FFT magnitude chart (currently one-sided)
- [ ] Configurable center frequency offset for SDR tuning visualization
- [ ] Vertical line markers for positive/negative Nyquist boundaries
- [ ] Frequency axis auto-formatting (Hz/kHz/MHz based on range)

---

## Related Documentation

- `claude.md` - Main developer documentation
- `docs/HIGH_SPEED_FIX.md` - Ring buffer optimization history
- Web interface displays this data in real-time via Chart.js
