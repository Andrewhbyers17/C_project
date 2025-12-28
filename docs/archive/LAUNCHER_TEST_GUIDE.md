# Launcher Test Guide

## Manual Testing Instructions

Since the launchers open GUI windows and browsers, they need to be tested manually. Here's how:

---

## Test 1: Quick Launcher (run.bat)

### Steps:
1. Navigate to the project folder in Windows Explorer
2. **Double-click `run.bat`**
3. Observe the following:

### Expected Results:
✅ A command window titled "FFT Analyzer" opens
✅ Application starts and shows:
```
===========================================
  FFT Analyzer v1.0.0
  Real-Time Spectrum Analysis
===========================================
[*] Using test waveforms (no network input)
[*] Initializing web server on port 8080...
[OK] Web server listening on port 8080
[OK] Signal buffers allocated
[OK] DSP contexts allocated
[OK] Ready!
```
✅ After 2 seconds, default browser opens automatically
✅ Browser navigates to http://localhost:8080
✅ Web interface loads correctly
✅ Shows 3 mode options: Network Input, Silence, Signal + Noise

### To Stop:
- Close the "FFT Analyzer" command window
- Browser tab can stay open or be closed separately

---

## Test 2: Full Launcher - Help

### Steps:
1. Open Command Prompt
2. Navigate to project folder:
   ```batch
   cd C:\Users\andre\.claude-worktrees\C_project\affectionate-snyder
   ```
3. Run:
   ```batch
   launch_fft_analyzer.bat --help
   ```

### Expected Results:
✅ Shows help text:
```
========================================
  FFT Analyzer Launcher v1.0.0
========================================

Usage: launch_fft_analyzer.bat [OPTIONS]

Options:
  --port PORT         Web server port (default: 8080)
  --source IP:PORT    Network source (e.g., 192.168.1.100:5000)
  --protocol tcp/udp  Network protocol (default: tcp)
  --test              Use test waveforms
  --help              Show this help

Examples:
  launch_fft_analyzer.bat
  launch_fft_analyzer.bat --test
  launch_fft_analyzer.bat --source 192.168.1.100:5000 --protocol tcp
  launch_fft_analyzer.bat --port 9090 --test
```

---

## Test 3: Full Launcher - Test Mode

### Steps:
1. From Command Prompt:
   ```batch
   launch_fft_analyzer.bat --test
   ```

### Expected Results:
✅ Shows launcher banner
✅ Application starts in new window
✅ Browser opens to http://localhost:8080
✅ Web interface works correctly

---

## Test 4: Full Launcher - Custom Port

### Steps:
1. From Command Prompt:
   ```batch
   launch_fft_analyzer.bat --test --port 9090
   ```

### Expected Results:
✅ Application starts
✅ Browser opens to http://localhost:9090 (custom port)
✅ Web interface accessible on port 9090

### Verification:
Check the command window shows:
```
[*] Initializing web server on port 9090...
[OK] Web server listening on port 9090
```

---

## Test 5: Executable Direct Test

### Steps:
1. From Command Prompt:
   ```batch
   fft_analyzer_network.exe --test
   ```

### Expected Results:
✅ Application starts
✅ Shows URL banner:
```
========================================
  WEB INTERFACE READY
========================================
  Open your browser to:
  http://localhost:8080
========================================
```
✅ Browser does NOT auto-open (expected - user must open manually)
✅ Application works when manually opening browser

---

## Test 6: Mode Selection in Web Interface

### Steps:
1. Launch application (any method)
2. Open web interface
3. Find mode dropdown (top of page)
4. Verify mode options

### Expected Results:
✅ Dropdown shows exactly 3 options:
   - 0: Network Input
   - 1: Silence
   - 2: Signal + Noise
✅ Selecting "Silence" - PSD chart shows flat line (no signal)
✅ Selecting "Signal + Noise" - PSD chart shows 1kHz peak with noise floor
✅ No crashes when changing modes

---

## Test 7: Recording Functionality

### Steps:
1. Launch application
2. Open web interface
3. Select "Signal + Noise" mode
4. Click "Record" button
5. Wait 5 seconds
6. Click "Stop Recording"

### Expected Results:
✅ Recording status shows "Recording..."
✅ File path displayed
✅ After stop, file saved in `logs/` folder
✅ File contains data (not empty)
✅ Format can be changed (Binary/CSV/HDF5)

---

## Test 8: Network Reconnection (Optional)

### Steps:
1. Set up network data source (or simulate with test script)
2. Run:
   ```batch
   launch_fft_analyzer.bat --source 192.168.1.100:5000
   ```
3. Simulate network failure (disconnect/stop source)
4. Observe console output
5. Restore network connection

### Expected Results:
✅ Initial connection succeeds or shows retry attempts
✅ On failure, shows:
   - "[ERROR] Network connection lost"
   - "[*] Reconnection attempt 1/5..."
   - Retries every 5 seconds
✅ Uses silence as fallback during disconnection
✅ Successfully reconnects when network restored
✅ Resumes normal operation

---

## Automated Verification Checklist

Run this in PowerShell to verify files exist:

```powershell
# Check launcher files exist
Test-Path "run.bat"
Test-Path "launch_fft_analyzer.bat"
Test-Path "fft_analyzer_network.exe"
Test-Path "LAUNCHER_README.md"

# Check executable runs
.\fft_analyzer_network.exe --help
```

### Expected Output:
```
True
True
True
True
[Shows help text]
```

---

## Known Issues & Limitations

### Issue: Browser doesn't open automatically
**Cause:** Default browser not set, or permission issues
**Solution:** Manually open http://localhost:8080

### Issue: Port already in use
**Cause:** Previous instance still running
**Solution:**
```batch
taskkill /F /IM fft_analyzer_network.exe
```
Then relaunch

### Issue: Multiple browser tabs open
**Cause:** Rapid re-launching
**Solution:** Close old tabs, or they'll just show old data

---

## Performance Verification

### Memory Test:
1. Launch application
2. Let run for 5 minutes
3. Check Task Manager

**Expected:** Memory usage stable ~50-60 MB (no leaks)

### CPU Test:
1. Launch with test signal
2. Check Task Manager

**Expected:** CPU usage ~5% (one core) - should be lower than before optimizations

---

## Success Criteria

All tests should pass with:
- ✅ No crashes
- ✅ Browser auto-opens from batch files
- ✅ Web interface loads correctly
- ✅ Only 3 modes available
- ✅ Mode switching works
- ✅ Recording works
- ✅ Clean shutdown

---

## Test Results Template

Copy and fill out:

```
Date: ___________
Tester: ___________

[ ] Test 1: Quick Launcher - PASS / FAIL
[ ] Test 2: Full Launcher Help - PASS / FAIL
[ ] Test 3: Full Launcher Test Mode - PASS / FAIL
[ ] Test 4: Custom Port - PASS / FAIL
[ ] Test 5: Direct Executable - PASS / FAIL
[ ] Test 6: Mode Selection - PASS / FAIL
[ ] Test 7: Recording - PASS / FAIL
[ ] Test 8: Network Reconnect (Optional) - PASS / FAIL / SKIP

Issues Found:
_________________________________
_________________________________

Overall Result: PASS / FAIL

Notes:
_________________________________
_________________________________
```
