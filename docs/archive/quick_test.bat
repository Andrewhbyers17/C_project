@echo off
REM Quick Test - Automated Raw IQ Recording Test
REM
REM This script runs a quick automated test:
REM   1. Starts the analyzer in background
REM   2. Waits for it to initialize
REM   3. Sends 10 seconds of test data
REM   4. Stops everything

echo ========================================
echo  Quick Raw IQ Recording Test
echo ========================================
echo.
echo This will automatically:
echo   1. Start FFT analyzer on port 5000
echo   2. Send 10 seconds of test data
echo   3. Create HDF5 file in logs/ directory
echo.

REM Create logs directory if it doesn't exist
if not exist "logs" mkdir logs

echo [1/4] Starting IQ data server...
start /min "IQ Server" cmd /c "python test_iq_server.py --port 5000 --rate 2000000 --signal sine --duration 15"

echo [2/4] Waiting for server to start (2 seconds)...
timeout /t 2 /nobreak >nul

echo [3/4] Starting FFT analyzer (will connect to server)...
start /min "FFT Analyzer" cmd /c "fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp"

echo [4/4] Running test (15 seconds)...
timeout /t 15 /nobreak >nul

echo [4/4] Test complete!
echo.
echo Stopping analyzer...
taskkill /FI "WINDOWTITLE eq FFT Analyzer*" /F >nul 2>&1

echo.
echo ========================================
echo  Test Results
echo ========================================
echo.
echo Check the logs/ directory for output:
dir /B /O-D logs\iq_data_*.h5 2>nul | findstr . && (
    echo.
    echo Most recent file:
    for /F "delims=" %%f in ('dir /B /O-D logs\iq_data_*.h5') do (
        echo   logs\%%f
        goto :found
    )
    :found
    echo.
    echo Verify the file with:
    echo   python read_iq_file.py logs\iq_data_*.h5
) || (
    echo No IQ files found - check for errors above
)

echo.
pause
