# HDF5 Support Setup

## What is HDF5?

HDF5 (Hierarchical Data Format 5) is a high-performance data format ideal for storing large datasets. Benefits for FFT data:
- **Compression**: Automatic compression reduces file size by 50-90%
- **Metadata**: Store configuration, timestamps, and parameters alongside data
- **Fast access**: Random access to specific frames without reading entire file
- **Cross-platform**: Works on Windows, Linux, macOS
- **Tools**: Many analysis tools support HDF5 (Python, MATLAB, R, etc.)

## Installation Options

### Option 1: Using Pre-built Binaries (Recommended)

1. **Download HDF5 for Windows:**
   - Visit: https://www.hdfgroup.org/downloads/hdf5/
   - Download: "HDF5 1.14.x Windows (ZIP)" - choose the version with MinGW support
   - Or direct link: https://support.hdfgroup.org/ftp/HDF5/releases/

2. **Extract and Install:**
   ```bash
   # Extract to a known location, e.g.:
   # C:\hdf5
   ```

3. **Update Makefile.windows:**
   ```makefile
   # Add HDF5 paths
   HDF5_DIR = C:/hdf5
   CFLAGS += -I$(HDF5_DIR)/include -DUSE_HDF5
   LDFLAGS += -L$(HDF5_DIR)/lib -lhdf5 -lhdf5_hl
   ```

### Option 2: Using MSYS2 Package Manager

1. **Install via MSYS2:**
   ```bash
   # Open MSYS2 MINGW64 terminal
   pacman -S mingw-w64-x86_64-hdf5
   ```

2. **Update Makefile.windows:**
   ```makefile
   # Add HDF5 support (MSYS2)
   CFLAGS += -DUSE_HDF5
   LDFLAGS += -lhdf5 -lhdf5_hl
   ```

### Option 3: Build from Source

1. **Download source:**
   ```bash
   # Get latest source from HDF Group
   wget https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.14/hdf5-1.14.3/src/hdf5-1.14.3.tar.gz
   tar -xzf hdf5-1.14.3.tar.gz
   cd hdf5-1.14.3
   ```

2. **Build with MinGW:**
   ```bash
   ./configure --prefix=/c/hdf5 --enable-build-mode=production
   make
   make install
   ```

3. **Update Makefile** (same as Option 1)

## Building with HDF5 Support

After installation:

```bash
# Clean previous build
mingw32-make -f Makefile.windows clean

# Build with HDF5
mingw32-make -f Makefile.windows CC=gcc
```

## HDF5 File Format Structure

The FFT analyzer will create HDF5 files with this structure:

```
fft_data_20251213_120000Z.h5
├── /metadata
│   ├── fft_size: 512
│   ├── sample_rate: 8000
│   ├── start_time: "2025-12-13T12:00:00Z"
│   └── version: "1.0"
├── /signal (dataset: [frames, 512])
│   └── Time-domain signal data
├── /magnitude (dataset: [frames, 256])
│   └── FFT magnitude spectrum
└── /psd (dataset: [frames, 128])
    └── Power spectral density
```

## Using HDF5 Files

### Python Example
```python
import h5py
import numpy as np
import matplotlib.pyplot as plt

# Open file
with h5py.File('fft_data_20251213_120000Z.h5', 'r') as f:
    # Read metadata
    fft_size = f['metadata']['fft_size'][()]
    sample_rate = f['metadata']['sample_rate'][()]

    # Read datasets
    magnitude = f['magnitude'][:]  # All frames
    psd = f['psd'][:]

    # Plot specific frame
    frame = 100
    plt.plot(magnitude[frame])
    plt.title(f'FFT Magnitude - Frame {frame}')
    plt.show()
```

### MATLAB Example
```matlab
% Open file
filename = 'fft_data_20251213_120000Z.h5';

% Read metadata
fft_size = h5read(filename, '/metadata/fft_size');
sample_rate = h5read(filename, '/metadata/sample_rate');

% Read magnitude data
magnitude = h5read(filename, '/magnitude');

% Plot frame 100
plot(magnitude(:, 100));
title('FFT Magnitude - Frame 100');
```

## File Size Comparison

| Format | 1000 Frames | 10000 Frames | Compression |
|--------|-------------|--------------|-------------|
| Binary | ~2.8 MB     | ~28 MB       | None        |
| CSV    | ~1.5 MB     | ~15 MB       | Text-based  |
| HDF5   | ~0.5 MB     | ~5 MB        | gzip level 6|

## Troubleshooting

### Error: "hdf5.dll not found"
- Add HDF5 bin directory to PATH: `C:\hdf5\bin`
- Or copy HDF5 DLLs to project directory

### Error: "undefined reference to H5..."
- Check LDFLAGS includes `-lhdf5 -lhdf5_hl`
- Verify HDF5_DIR path is correct

### Error: "Cannot open include file 'hdf5.h'"
- Check CFLAGS includes `-I$(HDF5_DIR)/include`
- Verify header files exist in include directory

## Alternative: Use CSV for Now

If HDF5 setup is too complex, CSV format works well for:
- Small to medium datasets (< 10,000 frames)
- Easy import to Excel, Python pandas, R
- Human-readable for debugging

Binary format is best for:
- Large datasets (> 10,000 frames)
- Maximum fidelity (full floating-point precision)
- Fast writing during real-time capture
