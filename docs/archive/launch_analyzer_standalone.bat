@echo off
REM Standalone Launcher - Start analyzer first, connect data source later
REM The analyzer will wait indefinitely for the data source to become available

echo ========================================
echo   FFT Analyzer - Standalone Mode
echo ========================================
echo.

REM Default settings
set WEB_PORT=8080
set SOURCE_IP=127.0.0.1
set SOURCE_PORT=5000
set PROTOCOL=tcp

REM Parse command line arguments
:parse_args
if "%~1"=="" goto start_app
if /i "%~1"=="--port" (
    set WEB_PORT=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--source-port" (
    set SOURCE_PORT=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--source-ip" (
    set SOURCE_IP=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--help" (
    echo Usage: launch_analyzer_standalone.bat [OPTIONS]
    echo.
    echo Options:
    echo   --port PORT         Web server port (default: 8080)
    echo   --source-ip IP      Data source IP (default: 127.0.0.1)
    echo   --source-port PORT  Data source port (default: 5000)
    echo   --help              Show this help
    echo.
    echo Examples:
    echo   launch_analyzer_standalone.bat
    echo   launch_analyzer_standalone.bat --source-port 5001
    echo   launch_analyzer_standalone.bat --source-ip 192.168.1.100 --source-port 5000
    echo.
    echo What this does:
    echo   1. Starts FFT analyzer immediately
    echo   2. Opens web browser
    echo   3. Waits for data source to become available
    echo   4. Auto-connects when source starts
    echo.
    echo You can:
    echo   - Start analyzer first, run data source later
    echo   - Stop/restart data source anytime
    echo   - Analyzer automatically reconnects
    echo.
    exit /b 0
)
shift
goto parse_args

:start_app
echo Configuration:
echo   Web port:        %WEB_PORT%
echo   Data source:     %SOURCE_IP%:%SOURCE_PORT%
echo   Protocol:        %PROTOCOL%
echo.

echo [1/2] Starting FFT Analyzer...
echo       (Will wait for data source to become available)
echo.
start "FFT Analyzer" fft_analyzer_network.exe --source %SOURCE_IP%:%SOURCE_PORT% --protocol %PROTOCOL% --port %WEB_PORT%

echo [2/2] Waiting for web server to initialize...
timeout /t 2 /nobreak >nul

echo [*] Opening web interface...
start http://localhost:%WEB_PORT%

echo.
echo ========================================
echo   Analyzer Running!
echo ========================================
echo.
echo Web interface: http://localhost:%WEB_PORT%
echo Waiting for:   %SOURCE_IP%:%SOURCE_PORT%
echo.
echo The analyzer will automatically connect when you start:
echo   python test_iq_server.py --port %SOURCE_PORT% --rate 2000000 --signal sine
echo.
echo You can:
echo   - Start the data source now or later
echo   - Stop and restart the data source anytime
echo   - The analyzer will auto-reconnect every 5 seconds
echo.
echo To stop:
echo   - Close the "FFT Analyzer" window, OR
echo   - Press Ctrl+C in that window
echo.
pause
