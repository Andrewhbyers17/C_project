# Build script for FFT Analyzer Network (Windows)
# Alternative to using make

Write-Host "Building FFT Analyzer Network..." -ForegroundColor Cyan

# Compiler settings
$CC = "gcc"
$CFLAGS = "-O2", "-Wall", "-D_WIN32", "-std=c99"
$LDFLAGS = "-lws2_32", "-lm"
$EXE = "fft_analyzer_network.exe"

# Source files
$SOURCES = @(
    "fft_analyzer_network.c",
    "web_server.c",
    "kiss_fft.c"
)

# Clean old build
if (Test-Path $EXE) {
    Write-Host "Cleaning old build..." -ForegroundColor Yellow
    Remove-Item $EXE -ErrorAction SilentlyContinue
}
Remove-Item *.o -ErrorAction SilentlyContinue

# Compile each source file
Write-Host "`nCompiling source files..." -ForegroundColor Green
foreach ($source in $SOURCES) {
    Write-Host "  Compiling $source..." -ForegroundColor Gray
    $obj = $source -replace '\.c$', '.o'
    & $CC $CFLAGS -c $source -o $obj
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error compiling $source" -ForegroundColor Red
        exit 1
    }
}

# Link
Write-Host "`nLinking..." -ForegroundColor Green
$objects = $SOURCES | ForEach-Object { $_ -replace '\.c$', '.o' }
& $CC $CFLAGS $objects -o $EXE $LDFLAGS

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n========================================" -ForegroundColor Green
    Write-Host "  Build successful!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Run with: .\$EXE --help`n" -ForegroundColor Cyan
} else {
    Write-Host "`nBuild failed!" -ForegroundColor Red
    exit 1
}
