# Building FFT Analyzer on Windows

## Quick Start

### Option 1: Using MinGW (Recommended)

```bash
mingw32-make -f Makefile.windows CC=gcc
```

### Option 2: Using PowerShell Build Script

```powershell
.\build.ps1
```

### Option 3: Using Visual Studio

Open "Developer Command Prompt for VS" and run:
```cmd
nmake /F Makefile.windows CC=cl
```

---

## Common Issues

### Error: 'make' is not recognized

**Problem:** You're using `make` instead of `mingw32-make`

**Solution:** Use the correct command:
```bash
mingw32-make -f Makefile.windows CC=gcc
```

---

### Error: 'mingw32-make' is not recognized

**Problem:** MinGW is not installed or not in PATH

**Solution:** Install MinGW via MSYS2:

1. Download MSYS2 from https://www.msys2.org/
2. Open MSYS2 MINGW64 terminal
3. Run:
   ```bash
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make
   ```
4. Add `C:\msys64\mingw64\bin` to your Windows PATH
5. Restart your terminal

---

### Error: 'gcc' is not recognized

**Problem:** GCC is installed but not in PATH

**Solution:** Add MinGW bin directory to PATH:
1. Press Win+X, select "System"
2. Click "Advanced system settings"
3. Click "Environment Variables"
4. Edit "Path" variable
5. Add: `C:\msys64\mingw64\bin`
6. Click OK and restart terminal

---

## Running the Application

After successful build:

```bash
# Test mode (built-in waveforms)
./fft_analyzer_network.exe --test

# With network input
./fft_analyzer_network.exe --source 192.168.1.100:5000 --protocol tcp
```

Then open your browser to: **http://localhost:8080**

---

## Build Output

**Successful build:**
```
gcc -O2 -Wall -D_WIN32 -std=c99 -c fft_analyzer_network.c -o fft_analyzer_network.o
gcc -O2 -Wall -D_WIN32 -std=c99 -c web_server.c -o web_server.o
gcc -O2 -Wall -D_WIN32 -std=c99 -c kiss_fft.c -o kiss_fft.o
gcc -O2 -Wall -D_WIN32 -std=c99 -c data_logger.c -o data_logger.o
gcc -O2 -Wall -D_WIN32 -std=c99 fft_analyzer_network.o web_server.o kiss_fft.o data_logger.o -offt_analyzer_network.exe -lws2_32 -lm
```

**Output file:** `fft_analyzer_network.exe` (~518 KB)

---

## Clean Build

To remove build artifacts:

```bash
mingw32-make -f Makefile.windows clean
```

Or manually:
```bash
rm -f *.o *.exe
```

---

## File Structure

After build, you'll have:
```
C_project/
├── fft_analyzer_network.exe    (Main executable)
├── fft_analyzer_network.o       (Object files)
├── web_server.o
├── kiss_fft.o
├── data_logger.o
├── *.c                          (Source files)
├── *.h                          (Header files)
├── Makefile.windows             (Windows makefile)
└── build.ps1                    (PowerShell build script)
```

---

## Compiler Warnings (Safe to Ignore)

You may see these warnings - they are harmless:
```
warning: ignoring '#pragma comment' [-Wunknown-pragmas]
warning: 'signal_buffer' may be used uninitialized [-Wmaybe-uninitialized]
```

These don't affect functionality.

---

## Notes

- The `nul` file may be created accidentally by shell redirects - safe to delete
- Add `nul`, `*.o`, and `*.exe` to `.gitignore` if using Git
- Build time: ~5-10 seconds on modern hardware
- Requires C99 standard support
