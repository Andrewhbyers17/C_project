@echo off
REM Test script for C-based IQ generator

echo ========================================
echo Building C IQ Generator...
echo ========================================
mingw32-make -f Makefile.iq_generator clean
mingw32-make -f Makefile.iq_generator

if not exist iq_generator.exe (
    echo [ERROR] Build failed
    pause
    exit /b 1
)

echo.
echo ========================================
echo Starting IQ Generator + FFT Analyzer
echo ========================================
echo.
echo This will:
echo   1. Start IQ generator on port 5000 (10 MHz)
echo   2. Start FFT analyzer
echo   3. Open web browser
echo.

REM Parse command line arguments or use defaults
set SAMPLE_RATE=10000000
set SIGNAL=noise_peak

if not "%1"=="" (
    if "%1"=="--rate" set SAMPLE_RATE=%2
)

echo Sample rate: %SAMPLE_RATE% Hz
echo.

REM Start IQ generator in new window
start "IQ Generator" cmd /c "iq_generator.exe --rate %SAMPLE_RATE% --port 5000 & pause"

REM Wait for server to start
timeout /t 2 /nobreak >nul

REM Start FFT analyzer
start "FFT Analyzer" cmd /c "fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp & pause"

REM Wait for web server to start
timeout /t 3 /nobreak >nul

REM Open browser
start http://localhost:8080

echo.
echo ========================================
echo All components started!
echo ========================================
echo Press any key to stop all processes...
pause >nul

REM Kill processes
taskkill /FI "WINDOWTITLE eq IQ Generator*" /F >nul 2>&1
taskkill /FI "WINDOWTITLE eq FFT Analyzer*" /F >nul 2>&1

echo Cleanup complete
