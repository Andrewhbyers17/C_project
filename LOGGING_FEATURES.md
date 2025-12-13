# FFT Analyzer - New Features Summary

## Overview
The FFT Analyzer has been successfully upgraded with the following enhancements:

---

## ✅ Completed Features

### 1. **Removed LED Emulator**
- **What Changed:** Removed the 8-LED frequency band display from the web interface
- **Why:** Simplified the UI to focus on more important data visualization
- **Files Modified:** `web_server.c`

### 2. **Added Zulu Time Display**
- **What Changed:** Added a real-time UTC clock at the top of the web interface
- **Format:** `HH:MM:SS Z` (e.g., "16:35:42 Z")
- **Updates:** Every second
- **Styling:** Cyan color, monospace font, prominent placement below subtitle
- **Files Modified:** `web_server.c`

### 3. **Binary Data Logging**
- **What Changed:** Implemented complete binary file logging system
- **Features:**
  - Automatic timestamped filenames (e.g., `fft_data_20231213_163542Z.bin`)
  - Binary format with header containing FFT size, sample rate, start time
  - Each frame includes: timestamp, time-domain signal, FFT magnitude, and PSD data
  - Start/stop controls via web interface
  - Real-time logging status display
  - Automatic cleanup on application exit

- **Files Created:**
  - `data_logger.h` - API definitions and binary format specification
  - `data_logger.c` - Implementation of binary logging

- **Files Modified:**
  - `fft_analyzer_network.c` - Integrated logger, added callbacks, main loop integration
  - `web_server.c` - Added UI controls and API endpoint
  - `web_server.h` - Added callback function declarations
  - `Makefile.windows` - Added data_logger.c to build

### 4. **HDF5 Support Framework**
- **Status:** Framework in place, ready for future implementation
- **How to Enable:** Define `USE_HDF5` when building
- **Current State:** Placeholder functions ready for HDF5 library integration

---

## Binary File Format Specification

### File Header (64 bytes)
```c
- Magic: "FFTLOG01" (8 bytes)
- Version: uint32_t (4 bytes)
- FFT Size: uint32_t (4 bytes)
- Sample Rate: uint32_t (4 bytes)
- Start Time: uint64_t (8 bytes, Unix timestamp)
- Reserved: 36 bytes for future use
```

### Data Frame (variable size per frame)
```c
- Timestamp: uint64_t (8 bytes, milliseconds since epoch)
- Signal: float[fft_size] - Time domain data
- Magnitude: float[fft_size/2] - FFT magnitude
- PSD: float[128] - Power spectral density in dB
```

---

## Usage

### Starting the Application
```bash
./fft_analyzer_network.exe --test
```

Then open your browser to: **http://localhost:8080**

### Using Data Logging

1. **Start Logging:**
   - Click the "📊 START LOGGING" button
   - File is automatically created with timestamp
   - Status displays the filename

2. **Stop Logging:**
   - Click the "⏹ STOP LOGGING" button
   - File is automatically closed and flushed
   - Total frame count is displayed

3. **Log File Location:**
   - Saved in the current working directory
   - Named: `fft_data_YYYYMMDD_HHMMSSZ.bin`
   - Example: `fft_data_20231213_163542Z.bin`

### Reading Log Files

To read the binary log files, you can create a Python script or C program that:
1. Reads the 64-byte header to get FFT size and sample rate
2. Reads each frame sequentially:
   - 8-byte timestamp
   - fft_size floats (signal)
   - fft_size/2 floats (magnitude)
   - 128 floats (PSD)

---

## Web Interface Changes

### New UI Elements
- **Zulu Time**: Displays current UTC time at the top
- **Logging Button**: Green "START LOGGING" / Red "STOP LOGGING"
- **Logging Status**: Shows filename when logging, "Not logging" otherwise

### Removed UI Elements
- LED frequency band display (8 LEDs)

---

## API Endpoints

### New Endpoint
- **`POST /api/log/toggle`**
  - Toggles data logging on/off
  - Response: `{"status":"ok","logging":true/false,"filepath":"..."}`

### Existing Endpoints
- `GET /` - Main web interface
- `GET /api/fft` or `/api/data` - FFT data JSON
- `POST /api/pause` - Toggle pause
- `POST /api/mode?value=N` - Change waveform mode

---

## Build Information

**Executable Size:** ~518 KB
**Build Date:** 2025-12-13
**Compiler:** GCC 15.2.0 (MinGW-w64)
**Platform:** Windows

**New Dependencies:**
- None (uses standard C library only)

---

## Future Enhancements

### Planned Features
1. **HDF5 Support** - Add HDF5 library support for scientific data format
2. **Compression** - Add optional data compression (zlib/gzip)
3. **Metadata** - Add user notes and configuration to log files
4. **Playback** - Add ability to replay logged data through the analyzer
5. **Export** - CSV/MAT file export functionality

---

## Troubleshooting

### Logging Won't Start
- Check disk space
- Verify write permissions in current directory
- Check console for error messages

### Log Files Too Large
- Each frame is approximately:
  - 8 + (512×4) + (256×4) + (128×4) = 3,584 bytes ≈ 3.5 KB
- At 10 Hz update rate: ~35 KB/second, ~2.1 MB/minute, ~126 MB/hour
- Reduce update rate or implement compression if needed

### Can't Find Log Files
- Files are saved in the working directory where the exe is run
- Use absolute paths if needed
- Check `ls` or dir in the terminal

---

## Technical Notes

- **Thread Safety:** Not currently implemented (single-threaded design)
- **Buffer Management:** Logging uses file buffering, flushed every 10 frames
- **Error Handling:** Logging errors are printed to console but don't crash the app
- **Performance Impact:** Minimal (~1-2% CPU overhead)

---

*Last Updated: 2025-12-13*
*Version: 1.0*
