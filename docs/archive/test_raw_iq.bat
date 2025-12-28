@echo off
REM Test Raw IQ Recording
REM
REM This script demonstrates raw IQ recording at different sample rates.
REM
REM Usage:
REM   1. Run this script - it will start the FFT analyzer in server mode
REM   2. In another terminal, run the Python test sender:
REM      python test_iq_sender.py --port 5000 --rate 8000 --signal sine
REM   3. Open browser to http://localhost:8080
REM   4. Start raw IQ recording from web interface (select "raw_iq" format)
REM   5. Check logs/ directory for HDF5 files

echo ========================================
echo  Raw IQ Recording Test - Interactive
echo ========================================
echo.
echo This will start an IQ server for testing.
echo.
echo Sample rates to try:
echo   --rate 8000      (8 kHz - audio)
echo   --rate 100000    (100 kHz - ultrasonic)
echo   --rate 1000000   (1 MHz - RF)
echo   --rate 2000000   (2 MHz - high-speed RF)
echo   --rate 10000000  (10 MHz - maximum)
echo.
echo Signal types:
echo   --signal sine          (1 kHz pure tone)
echo   --signal sweep         (100 Hz to 4 kHz chirp)
echo   --signal noise         (white noise)
echo   --signal signal_noise  (1 kHz + noise, 10 dB SNR)
echo   --signal multi         (500 Hz + 1 kHz + 2 kHz)
echo.
echo Press any key to start...
pause >nul

echo.
echo [1/2] Starting IQ server (listening on port 5000)...
start "IQ Server" cmd /c "python test_iq_server.py --port 5000 --rate 2000000 --signal sine"

echo [2/2] Waiting for server to start (2 seconds)...
timeout /t 2 /nobreak >nul

echo.
echo Starting FFT analyzer (will connect to server)...
echo.
echo To change test signal, edit this file and modify:
echo   python test_iq_server.py --port 5000 --rate XXXXXX --signal XXXXXX
echo.
fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp

echo.
echo Analyzer stopped.
echo Stopping IQ server...
taskkill /FI "WINDOWTITLE eq IQ Server*" /F >nul 2>&1
echo.
pause
