@echo off
REM Build Release Package for FFT Analyzer v1.0.0
REM This creates a portable, ready-to-use package

echo ========================================
echo   FFT Analyzer - Build Release Package
echo ========================================
echo.

REM Clean previous build
echo [1/5] Cleaning previous build...
mingw32-make -f Makefile.windows clean >nul 2>&1

REM Build executable
echo [2/5] Building executable with HDF5 support...
mingw32-make -f Makefile.windows
if errorlevel 1 (
    echo ERROR: Build failed!
    pause
    exit /b 1
)

REM Create release directory structure
echo [3/5] Creating release package...
if exist release rd /s /q release
mkdir release
mkdir release\logs
mkdir release\docs

REM Copy executable
echo [4/5] Copying files...
copy fft_analyzer_network.exe release\ >nul

REM Copy HDF5 DLLs if they exist
if exist "C:\msys64\mingw64\bin\libhdf5-*.dll" (
    echo     - Copying HDF5 libraries...
    copy "C:\msys64\mingw64\bin\libhdf5-*.dll" release\ >nul 2>&1
    copy "C:\msys64\mingw64\bin\libsz*.dll" release\ >nul 2>&1
    copy "C:\msys64\mingw64\bin\zlib*.dll" release\ >nul 2>&1
)

REM Create README
echo [5/5] Creating documentation...
(
echo FFT ANALYZER v1.0.0 - Quick Start Guide
echo ==========================================
echo.
echo WHAT IS THIS?
echo -------------
echo Real-time FFT signal analyzer with web-based visualization.
echo Supports Binary, CSV, and HDF5 data logging formats.
echo.
echo QUICK START:
echo -----------
echo 1. Double-click fft_analyzer_network.exe
echo 2. Open your web browser to: http://localhost:8080
echo 3. Select a waveform from the dropdown
echo 4. Click "Record" to log data
echo 5. Data files are saved in the "logs" folder
echo.
echo COMMAND LINE OPTIONS:
echo --------------------
echo --test              Use built-in test signals ^(default^)
echo --source IP:PORT    Connect to network data source
echo --protocol tcp/udp  Network protocol ^(default: tcp^)
echo --port PORT         Web server port ^(default: 8080^)
echo --help              Show help message
echo.
echo EXAMPLES:
echo --------
echo   fft_analyzer_network.exe --test
echo   fft_analyzer_network.exe --source 192.168.1.100:5000
echo.
echo SYSTEM REQUIREMENTS:
echo -------------------
echo - Windows 10/11
echo - Web browser ^(Chrome, Firefox, Edge^)
echo - No additional software needed!
echo.
echo LOG FILES:
echo ---------
echo All recordings are saved to the "logs" folder:
echo - Binary format: Full precision, best for analysis
echo - CSV format: Easy to import into Excel/Python
echo - HDF5 format: Compressed, ideal for large datasets
echo.
echo WEB INTERFACE:
echo -------------
echo - Real-time PSD ^(Power Spectral Density^) chart
echo - Spectrogram visualization
echo - Single-click recording with format selection
echo - Auto-record on SNR threshold
echo - Configurable log directory
echo.
echo TROUBLESHOOTING:
echo ---------------
echo Q: Browser shows "Connection refused"
echo A: Make sure the executable is running first
echo.
echo Q: No data appearing?
echo A: Select a waveform mode from the dropdown
echo.
echo Q: HDF5 button doesn't work?
echo A: HDF5 libraries not installed. Use Binary or CSV instead.
echo.
echo SUPPORT:
echo -------
echo For issues or questions, see the documentation in /docs folder
echo.
echo Version: 1.0.0
echo Build Date: %date%
) > release\README.txt

REM Create technical documentation
(
echo FFT ANALYZER - Technical Documentation
echo ======================================
echo.
echo ARCHITECTURE
echo ------------
echo - FFT Engine: kiss_fft ^(512-point FFT^)
echo - Web Server: Embedded lightweight HTTP server
echo - Sampling: 8000 Hz
echo - Update Rate: 10 Hz
echo - PSD: Welch method with 256-point segments
echo.
echo FILE FORMATS
echo ------------
echo.
echo BINARY FORMAT:
echo - Header: Magic "FFTLOG01", version, FFT size, sample rate
echo - Frames: Timestamp + Signal + Magnitude + PSD arrays
echo - Size: ~2.8 MB per 1000 frames
echo.
echo CSV FORMAT:
echo - Header: Configuration and column names
echo - Data: Timestamp, Signal_Avg, Magnitude_Peak, PSD_Avg, SNR
echo - Size: ~1.5 MB per 1000 frames
echo.
echo HDF5 FORMAT:
echo - Metadata: /metadata ^(fft_size, sample_rate, start_time^)
echo - Datasets: /signal, /magnitude, /psd ^(compressed^)
echo - Size: ~0.5 MB per 1000 frames ^(gzip level 6^)
echo.
echo API ENDPOINTS
echo -------------
echo GET  /api/fft          - Get current FFT data ^(JSON^)
echo POST /api/mode         - Change waveform mode
echo POST /api/pause        - Toggle pause state
echo POST /api/log/start    - Start logging ^(?format=binary/csv/hdf5^)
echo POST /api/log/stop     - Stop logging
echo POST /api/auto-record  - Configure auto-record
echo POST /api/log/directory - Set log directory
echo.
echo NETWORK PROTOCOL
echo ---------------
echo When using --source option:
echo - TCP: Reliable stream of float32 samples
echo - UDP: Fast but may drop packets
echo - Format: Little-endian float32 array
echo.
echo WAVEFORM MODES
echo --------------
echo 0: Silence
echo 1: 440 Hz Sine
echo 2: 1000 Hz Sine
echo 3: 2000 Hz Sine
echo 4: Mixed Tones ^(440Hz + 880Hz^)
echo 5: Frequency Sweep ^(100-4000 Hz^)
echo 6: White Noise
echo 7: Impulse Train
echo 8: Linear FM Chirp
echo 9: Sinc Function
echo 10: IQ LFM Chirp
echo 11: Signal + Noise
echo.
echo PERFORMANCE
echo -----------
echo - CPU: ~5%% single core
echo - Memory: ~50 MB
echo - Disk Write: ~280 KB/s ^(binary, continuous^)
echo.
) > release\docs\TECHNICAL.txt

echo.
echo ========================================
echo   Build Complete!
echo ========================================
echo.
echo Release package created in: release\
echo.
echo Package contents:
dir /b release
echo.
echo To distribute: Zip the "release" folder
echo.
pause
