FFT ANALYZER v1.0.0 - Quick Start Guide
==========================================

WHAT IS THIS?
-------------
Real-time FFT signal analyzer with web-based visualization.
Supports Binary, CSV, and HDF5 data logging formats.

QUICK START:
-----------
1. Double-click fft_analyzer_network.exe
2. Open your web browser to: http://localhost:8080
3. Select a waveform from the dropdown
4. Click "Record" to log data
5. Data files are saved in the "logs" folder

COMMAND LINE OPTIONS:
--------------------
--test              Use built-in test signals (default)
--source IP:PORT    Connect to network data source
--protocol tcp/udp  Network protocol (default: tcp)
--port PORT         Web server port (default: 8080)
--help              Show help message

EXAMPLES:
--------
  fft_analyzer_network.exe --test
  fft_analyzer_network.exe --source 192.168.1.100:5000

SYSTEM REQUIREMENTS:
-------------------
- Windows 10/11
- Web browser (Chrome, Firefox, Edge)
- No additional software needed!

LOG FILES:
---------
All recordings are saved to the "logs" folder:
- Binary format: Full precision, best for analysis
- CSV format: Easy to import into Excel/Python
- HDF5 format: Compressed, ideal for large datasets

WEB INTERFACE:
-------------
- Real-time PSD (Power Spectral Density) chart
- Spectrogram visualization
- Single-click recording with format selection
- Auto-record on SNR threshold
- Configurable log directory

TROUBLESHOOTING:
---------------
Q: Browser shows "Connection refused"
A: Make sure the executable is running first

Q: No data appearing?
A: Select a waveform mode from the dropdown

Q: HDF5 button doesn't work?
A: HDF5 libraries not installed. Use Binary or CSV instead.

SUPPORT:
-------
For issues or questions, see the documentation in /docs folder

Version: 1.0.0
Build Date: Wed 12/24/2025
