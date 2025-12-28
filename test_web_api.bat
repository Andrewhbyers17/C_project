@echo off
REM Comprehensive Web API Test Suite
REM Tests all aspects of the web interface and API

echo ========================================
echo   FFT Analyzer - Web API Test Suite
echo ========================================
echo.

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found
    echo Please install Python to run this test
    pause
    exit /b 1
)

REM Check if analyzer is running
echo [1/6] Checking if analyzer is running...
curl -s http://localhost:8080/ >nul 2>&1
if errorlevel 1 (
    echo.
    echo ERROR: FFT Analyzer is not running on port 8080
    echo.
    echo Please start it first:
    echo   fft_analyzer_network.exe --test
    echo   OR
    echo   start_test.bat
    echo.
    pause
    exit /b 1
)
echo [OK] Analyzer is running

REM Run Python test script
echo [2/6] Running API validation tests...
echo.

python test_web_api.py

if errorlevel 1 (
    echo.
    echo ========================================
    echo   TESTS FAILED
    echo ========================================
    pause
    exit /b 1
)

echo.
echo ========================================
echo   ALL TESTS PASSED
echo ========================================
echo.
echo Web interface is working correctly!
echo.
pause
