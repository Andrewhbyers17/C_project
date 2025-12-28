@echo off
REM Quick launcher for FFT Analyzer with test signals
start "FFT Analyzer" fft_analyzer_network.exe --test
timeout /t 2 /nobreak >nul
start http://localhost:8080
