# FFT Analyzer - Launcher Scripts

## Quick Start

For the fastest way to get started, just **double-click `run.bat`**. This will:
1. Start the FFT Analyzer with test signals
2. Wait 2 seconds for the server to start
3. Automatically open your default browser to http://localhost:8080

## Launcher Scripts

### `run.bat` - Quick Test Launcher
**Best for:** Quick testing and demos

**What it does:**
- Starts FFT Analyzer in test mode
- Auto-opens web interface in your default browser
- No configuration needed

**Usage:**
```batch
run.bat
```

### `launch_fft_analyzer.bat` - Full Launcher
**Best for:** Custom configurations and network sources

**What it does:**
- Starts FFT Analyzer with custom settings
- Auto-opens web interface
- Supports all command-line options

**Usage:**
```batch
# Test mode (default)
launch_fft_analyzer.bat --test

# Custom port
launch_fft_analyzer.bat --test --port 9090

# Network source
launch_fft_analyzer.bat --source 192.168.1.100:5000 --protocol tcp

# Network source with custom port
launch_fft_analyzer.bat --source 192.168.1.100:5000 --port 9090
```

**Options:**
- `--port PORT` - Web server port (default: 8080)
- `--source IP:PORT` - Network data source
- `--protocol tcp|udp` - Network protocol (default: tcp)
- `--test` - Use built-in test signals
- `--help` - Show help message

## Manual Start (No Auto-Open)

If you prefer to start without auto-opening the browser:

```batch
# Direct execution
fft_analyzer_network.exe --test

# Then manually open: http://localhost:8080
```

## Examples

### Example 1: Quick Test
```batch
run.bat
```
Opens FFT Analyzer with test signals on port 8080.

### Example 2: Custom Port
```batch
launch_fft_analyzer.bat --test --port 9090
```
Opens FFT Analyzer on port 9090.

### Example 3: Network Source
```batch
launch_fft_analyzer.bat --source 192.168.1.100:5000 --protocol tcp
```
Connects to network source at 192.168.1.100:5000 using TCP.

### Example 4: Network with Auto-Reconnect
```batch
launch_fft_analyzer.bat --source 192.168.1.100:5000
```
Connects to network source with automatic reconnection if connection drops.

## Stopping the Application

To stop the FFT Analyzer:
1. Close the "FFT Analyzer" command window
2. Or press `Ctrl+C` in the command window
3. The browser tab can be closed separately

## Troubleshooting

### "Port already in use" Error
- Another application is using port 8080
- Solution: Use `--port` to specify a different port
  ```batch
  launch_fft_analyzer.bat --test --port 9090
  ```

### Browser Doesn't Open Automatically
- Default browser may be blocked or not set
- Solution: Manually open http://localhost:8080 (or your custom port)

### Application Doesn't Start
- Check that `fft_analyzer_network.exe` is in the same directory as the batch files
- Try running directly: `fft_analyzer_network.exe --test`

### Network Connection Fails
- Check IP address and port
- Verify network source is running and accessible
- Check firewall settings
- The application will auto-retry connection 5 times with 5-second delays

## Advanced Usage

### Running in Background
The launchers start the application in a new window. To keep your command prompt available:
- The application runs in its own "FFT Analyzer" window
- You can minimize this window
- Close it when you want to stop the analyzer

### Multiple Instances
To run multiple instances on different ports:
```batch
# Instance 1
start "FFT 1" fft_analyzer_network.exe --test --port 8080

# Instance 2
start "FFT 2" fft_analyzer_network.exe --test --port 8081
```

Then open:
- http://localhost:8080
- http://localhost:8081

## Benefits of Batch Script Approach

✅ **Safer:** No `system()` calls in C code
✅ **Cleaner:** Separation of concerns
✅ **Flexible:** Easy to modify without recompiling
✅ **User-Friendly:** Double-click to launch
✅ **Transparent:** Users can see exactly what's being run
✅ **Portable:** Works on any Windows machine

## For Developers

If you modify the batch scripts:
- Keep the 2-second delay (`timeout /t 2`) to ensure server starts before browser opens
- Use `>nul` to suppress command output for cleaner user experience
- Use `start "Title"` to give the window a clear name
- Always validate parameters before passing to executable
