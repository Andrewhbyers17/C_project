@echo off
REM Quick overflow diagnostic test

echo ========================================
echo Ring Buffer Overflow Diagnostic Test
echo ========================================
echo.
echo This will test at multiple sample rates to find the overflow threshold.
echo.

set RATES=2000000 5000000 10000000 15000000 20000000

for %%R in (%RATES%) do (
    echo.
    echo ========================================
    echo Testing at %%R Hz ^(%%R / 1000000 MHz^)
    echo ========================================

    REM Start C generator
    start "IQ Gen %%R" cmd /c "iq_generator.exe --rate %%R --port 5000"

    REM Wait for startup
    timeout /t 2 /nobreak >nul

    REM Start analyzer (will run for 10 seconds)
    echo Starting analyzer... watch for overflow warnings
    timeout /t 10 ./fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp

    REM Kill generator
    taskkill /FI "WINDOWTITLE eq IQ Gen %%R*" /F >nul 2>&1

    echo.
    echo Press any key to test next rate...
    pause >nul
)

echo.
echo ========================================
echo Test complete!
echo ========================================
