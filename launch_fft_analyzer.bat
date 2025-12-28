@echo off
REM FFT Analyzer Launcher
REM Starts the FFT analyzer and automatically opens the web interface

echo ========================================
echo   FFT Analyzer Launcher v1.0.0
echo ========================================
echo.

REM Default settings
set WEB_PORT=8080
set NETWORK_SOURCE=
set PROTOCOL=tcp
set EXTRA_ARGS=

REM Parse command line arguments
:parse_args
if "%~1"=="" goto start_app
if /i "%~1"=="--port" (
    set WEB_PORT=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--source" (
    set NETWORK_SOURCE=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--protocol" (
    set PROTOCOL=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--test" (
    set EXTRA_ARGS=%EXTRA_ARGS% --test
    shift
    goto parse_args
)
if /i "%~1"=="--help" (
    echo Usage: launch_fft_analyzer.bat [OPTIONS]
    echo.
    echo Options:
    echo   --port PORT         Web server port (default: 8080)
    echo   --source IP:PORT    Network source (e.g., 192.168.1.100:5000)
    echo   --protocol tcp/udp  Network protocol (default: tcp)
    echo   --test              Use test waveforms
    echo   --help              Show this help
    echo.
    echo Examples:
    echo   launch_fft_analyzer.bat
    echo   launch_fft_analyzer.bat --test
    echo   launch_fft_analyzer.bat --source 192.168.1.100:5000 --protocol tcp
    echo   launch_fft_analyzer.bat --port 9090 --test
    echo.
    exit /b 0
)
shift
goto parse_args

:start_app
REM Build command line
set CMD=fft_analyzer_network.exe
if not "%NETWORK_SOURCE%"=="" (
    set CMD=%CMD% --source %NETWORK_SOURCE% --protocol %PROTOCOL%
)
if not "%EXTRA_ARGS%"=="" (
    set CMD=%CMD% %EXTRA_ARGS%
)
set CMD=%CMD% --port %WEB_PORT%

echo [*] Starting FFT Analyzer...
echo [*] Command: %CMD%
echo.

REM Start the application in a new window
start "FFT Analyzer" %CMD%

REM Wait a moment for the server to start
echo [*] Waiting for web server to start...
timeout /t 2 /nobreak >nul

REM Open the web interface
echo [*] Opening web interface at http://localhost:%WEB_PORT%
start http://localhost:%WEB_PORT%

echo.
echo ========================================
echo   FFT Analyzer is now running!
echo ========================================
echo.
echo Web interface: http://localhost:%WEB_PORT%
echo.
echo To stop the analyzer, close the FFT Analyzer window
echo or press Ctrl+C in that window.
echo.
pause
