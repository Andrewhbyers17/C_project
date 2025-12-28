@echo off
REM Quick Start Script
REM Starts IQ server, FFT analyzer, and opens web interface

echo ========================================
echo   FFT Analyzer - Quick Start
echo ========================================
echo.

REM Default settings
set SERVER_PORT=5000
set WEB_PORT=8080
set SAMPLE_RATE=2000000
set SIGNAL=sine

REM Parse command line arguments
:parse_args
if "%~1"=="" goto start
if /i "%~1"=="--rate" (
    set SAMPLE_RATE=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--signal" (
    set SIGNAL=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--help" (
    echo Usage: start_test.bat [OPTIONS]
    echo.
    echo Options:
    echo   --rate RATE      Sample rate in Hz (default: 2000000)
    echo   --signal TYPE    Signal type (default: sine)
    echo                    Types: sine, sweep, noise, signal_noise, multi
    echo   --help           Show this help
    echo.
    echo Examples:
    echo   start_test.bat
    echo   start_test.bat --rate 100000 --signal sweep
    echo   start_test.bat --rate 10000000 --signal multi
    echo.
    exit /b 0
)
shift
goto parse_args

:start
REM Create logs directory
if not exist "logs" mkdir logs

echo Configuration:
echo   Sample rate:  %SAMPLE_RATE% Hz
if %SAMPLE_RATE% GEQ 1000000 (
    set /a RATE_MHZ=%SAMPLE_RATE%/1000000
    echo                 ^(%RATE_MHZ% MHz^)
) else if %SAMPLE_RATE% GEQ 1000 (
    set /a RATE_KHZ=%SAMPLE_RATE%/1000
    echo                 ^(%RATE_KHZ% kHz^)
)
echo   Signal type:  %SIGNAL%
echo   Server port:  %SERVER_PORT%
echo   Web port:     %WEB_PORT%
echo.

echo [1/3] Starting IQ data server...
start "IQ Server" cmd /c "python test_iq_server.py --port %SERVER_PORT% --rate %SAMPLE_RATE% --signal %SIGNAL%"

echo [2/3] Waiting for server to start (2 seconds)...
timeout /t 2 /nobreak >nul

echo [3/3] Starting FFT analyzer...
start "FFT Analyzer" cmd /c "fft_analyzer_network.exe --source 127.0.0.1:%SERVER_PORT% --protocol tcp --port %WEB_PORT%"

echo [*] Waiting for web server and network connection (5 seconds)...
timeout /t 5 /nobreak >nul

echo [*] Opening web interface...
start http://localhost:%WEB_PORT%

echo.
echo ========================================
echo   System Running!
echo ========================================
echo.
echo IQ Server:     Running on port %SERVER_PORT%
echo FFT Analyzer:  Running
echo Web Interface: http://localhost:%WEB_PORT%
echo.
echo To record raw IQ data:
echo   1. Open web interface
echo   2. Click "Start Logging"
echo   3. Select "raw_iq" format
echo   4. Check logs/ directory for HDF5 files
echo.
echo To stop:
echo   - Close the "IQ Server" window, OR
echo   - Close the "FFT Analyzer" window, OR
echo   - Press Ctrl+C in either window
echo.
echo Press any key to close this launcher window...
pause >nul
