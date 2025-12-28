# Test Raw IQ Recording

## Issue
When selecting "Raw IQ (.h5)" format in web interface, files are created as .bin instead of .h5

## Root Cause Analysis

**Potential issues:**
1. Format parameter not being parsed correctly from URL
2. String comparison failing due to extra characters
3. HDF5 support not compiled in

## Test Procedure

### Step 1: Start System
```bash
# Terminal 1: Start data source
python test_iq_server.py --port 5000 --rate 2000000 --signal sine

# Terminal 2: Start analyzer (watch console output!)
./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp
```

### Step 2: Record with Raw IQ Format

1. Open browser: http://localhost:8080
2. Click "Start Logging" button
3. Select "Raw IQ (.h5)" from dropdown
4. Click "Confirm"

### Step 3: Check Console Output

**Look for these messages:**
```
[WEB] Received log start request with format: 'raw_iq'    <- NEW DEBUG OUTPUT
[WEB] Starting RAW IQ streaming at 2000000 Hz (2.00 MHz)
[OK] Disk writer thread created
[OK] Disk writer thread started
[LOGGER] Started raw IQ streaming to: logs\iq_data_YYYYMMDD_HHMMSSZ.h5
```

**If you see:**
```
[WEB] Received log start request with format: 'binary'
[WEB] Starting BINARY logging
```
Then the format parameter is not being sent correctly from the web interface.

### Step 4: Verify File Created

```bash
ls -lh logs/
```

**Expected:**
```
iq_data_20251227_HHMMSSZ.h5    (growing at ~16 MB/sec for 2 MHz)
```

**If you see:**
```
fft_data_20251227_HHMMSSZ.bin
```
Then raw_iq format is not being selected - it's defaulting to binary.

### Step 5: Stop Recording

Click "Stop Logging" in web interface.

**Console should show:**
```
[*] Stopping disk writer thread...
[OK] Disk writer thread stopped
[LOGGER] Stopped logging. Wrote XXXXXX IQ samples to: logs\iq_data_*.h5
```

## Debugging

### Check Format String in Console

The new debug output shows exactly what format string was received:
```
[WEB] Received log start request with format: 'raw_iq'
```

If it shows something else, the web interface isn't sending the right value.

### Check HDF5 Support

```bash
./fft_analyzer_network.exe --help | grep -i format
```

Should mention hdf5 and raw_iq formats.

### Manual API Test

```bash
curl -X POST "http://localhost:8080/api/log/start?format=raw_iq"
```

Check console for debug output.

## Expected Results

✅ Console shows: `[WEB] Received log start request with format: 'raw_iq'`
✅ Console shows: `[WEB] Starting RAW IQ streaming...`
✅ Console shows: `[OK] Disk writer thread started`
✅ File created: `logs/iq_data_*.h5` (NOT .bin)
✅ File grows at ~16 MB/sec for 2 MHz

## Common Issues

### Format shows 'binary' instead of 'raw_iq'

**Cause:** JavaScript not sending correct format parameter

**Fix:** Check web browser console (F12) for JavaScript errors

### Format shows 'raw_iq' but binary file created

**Cause:** String comparison failing (extra characters)

**Fix:** Already fixed - added space detection in format parsing

### "Unknown logging format: raw_iq"

**Cause:** HDF5 support not compiled in

**Fix:** Rebuild with `mingw32-make -f Makefile.windows` (HDF5 enabled by default)

## Success Criteria

After following the test procedure, you should have:
- .h5 file in logs/ directory (not .bin)
- File size grows continuously while recording
- Console shows raw IQ messages (not binary messages)
- File can be read with `python read_iq_file.py logs/iq_data_*.h5`
