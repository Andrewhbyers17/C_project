@echo off
REM Quick Start - FFT Analyzer Only
REM Opens FFT analyzer and web browser automatically
REM Connect your own IQ data source to 127.0.0.1:5000 (TCP)

echo ========================================
echo   FFT Analyzer - Quick Start
echo ========================================
echo.

REM Default settings
set SOURCE=127.0.0.1:5000
set WEB_PORT=8080
set PROTOCOL=tcp

REM Parse command line arguments
:parse_args
if "%~1"=="" goto start
if /i "%~1"=="--source" (
    set SOURCE=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--port" (
    set WEB_PORT=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--udp" (
    set PROTOCOL=udp
    shift
    goto parse_args
)
if /i "%~1"=="--help" (
    echo Usage: quick_start.bat [OPTIONS]
    echo.
    echo Options:
    echo   --source HOST:PORT   IQ data source (default: 127.0.0.1:5000)
    echo   --port PORT          Web server port (default: 8080)
    echo   --udp                Use UDP instead of TCP
    echo   --help               Show this help
    echo.
    echo Examples:
    echo   quick_start.bat
    echo   quick_start.bat --source 192.168.1.100:5000
    echo   quick_start.bat --port 8081 --udp
    echo.
    exit /b 0
)
shift
goto parse_args

:start
echo Configuration:
echo   IQ Source:     %SOURCE% (%PROTOCOL%)
echo   Web Interface: http://localhost:%WEB_PORT%
echo.

echo [1/2] Starting FFT analyzer...
start "FFT Analyzer" fft_analyzer_network.exe --source %SOURCE% --protocol %PROTOCOL% --port %WEB_PORT%

echo [2/2] Waiting for web server to start (3 seconds)...
timeout /t 3 /nobreak >nul

echo [*] Opening web interface...
start http://localhost:%WEB_PORT%

echo.
echo ========================================
echo   FFT Analyzer Running!
echo ========================================
echo.
echo Web Interface: http://localhost:%WEB_PORT%
echo IQ Source:     %SOURCE% (%PROTOCOL%)
echo.
echo The analyzer will auto-connect when your IQ source
echo starts sending data to %SOURCE%
echo.
echo To stop: Close the "FFT Analyzer" window or press Ctrl+C
echo.
echo Press any key to close this launcher window...
pause >nul
