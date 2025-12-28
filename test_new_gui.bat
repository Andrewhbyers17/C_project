@echo off
echo.
echo ========================================
echo   Testing New Ultra-Responsive GUI
echo ========================================
echo.

echo [1/3] Starting IQ test server...
start "IQ Server" /min python test_iq_server.py --port 5000 --rate 2000000 --signal multi
timeout /t 2 /nobreak >nul

echo [2/3] Starting FFT Analyzer...
start "FFT Analyzer" fft_analyzer_network.exe --source 127.0.0.1:5000 --protocol tcp
timeout /t 3 /nobreak >nul

echo [3/3] Opening browser...
start http://localhost:8080

echo.
echo ========================================
echo   System is running!
echo ========================================
echo.
echo   Web Interface: http://localhost:8080
echo   Press Ctrl+C in each window to stop
echo.
echo   NEW FEATURES:
echo   - Performance indicators (FPS, data rate, latency)
echo   - Peak hold mode (click "Peak" button)
echo   - Averaging mode (click "Avg" button)
echo   - Adaptive polling rate
echo   - Connection quality indicator
echo   - Real-time performance monitoring
echo.
pause
