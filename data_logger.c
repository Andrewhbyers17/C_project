/*
 * data_logger.c
 *
 * Implementation of data logging functionality
 */

#include "data_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
    #include <windows.h>
    #define gmtime_r(timer, buf) (gmtime_s(buf, timer) == 0 ? buf : NULL)
    // Create directory on Windows
    #include <direct.h>
    #define mkdir_p(path) _mkdir(path)
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #define mkdir_p(path) mkdir(path, 0755)
#endif

#ifdef USE_HDF5
    #include <hdf5.h>
    #include <hdf5_hl.h>
#endif

// Helper to create directory if it doesn't exist
static void ensure_directory_exists(const char* path) {
    if (path && strlen(path) > 0 && strcmp(path, ".") != 0) {
        mkdir_p(path);
    }
}

#ifdef USE_HDF5
// Forward declaration
static bool hdf5_write_frame(data_logger_t* logger, const float* signal,
                             const float* magnitude, const float* psd);
#endif

void data_logger_init(data_logger_t* logger) {
    memset(logger, 0, sizeof(data_logger_t));
    logger->file = NULL;
    logger->is_logging = false;
    logger->auto_record_enabled = false;
    logger->snr_threshold_db = 10.0f;  // Default 10 dB threshold
    logger->format = LOG_FORMAT_BINARY;
    strcpy(logger->log_directory, ".");  // Default to current directory
#ifdef USE_HDF5
    logger->hdf5_file = -1;
    logger->hdf5_signal_dset = -1;
    logger->hdf5_magnitude_dset = -1;
    logger->hdf5_psd_dset = -1;
#endif
}

static void get_timestamp_filename(char* buffer, size_t size, const char* prefix, const char* ext) {
    time_t now = time(NULL);
    struct tm tm_buf;
    struct tm* tm_info = gmtime_r(&now, &tm_buf);

    if (tm_info) {
        snprintf(buffer, size, "%s_%04d%02d%02d_%02d%02d%02dZ.%s",
                prefix,
                tm_info->tm_year + 1900,
                tm_info->tm_mon + 1,
                tm_info->tm_mday,
                tm_info->tm_hour,
                tm_info->tm_min,
                tm_info->tm_sec,
                ext);
    } else {
        snprintf(buffer, size, "%s_unknown.%s", prefix, ext);
    }
}

bool data_logger_start_binary(data_logger_t* logger, const char* filename,
                              uint32_t fft_size, uint32_t sample_rate) {
    if (logger->is_logging) {
        fprintf(stderr, "[LOGGER] Already logging to %s\n", logger->filepath);
        return false;
    }

    // Generate filename if not provided
    char temp_filename[256];
    if (filename == NULL || strlen(filename) == 0) {
        get_timestamp_filename(temp_filename, sizeof(temp_filename), "fft_data", "bin");
    } else {
        snprintf(temp_filename, sizeof(temp_filename), "%s", filename);
    }

    // Ensure directory exists
    ensure_directory_exists(logger->log_directory);

    // Build full path with directory
    snprintf(logger->filepath, sizeof(logger->filepath), "%s/%s",
             logger->log_directory, temp_filename);

    // Open file for binary writing
    logger->file = fopen(logger->filepath, "wb");
    if (!logger->file) {
        perror("[LOGGER] Failed to open file");
        return false;
    }

    // Write file header
    data_logger_header_t header = {0};
    memcpy(header.magic, DATA_LOGGER_MAGIC, 8);
    header.version = DATA_LOGGER_VERSION;
    header.fft_size = fft_size;
    header.sample_rate = sample_rate;
    header.start_time = (uint64_t)time(NULL);

    if (fwrite(&header, sizeof(header), 1, logger->file) != 1) {
        fprintf(stderr, "[LOGGER] Failed to write header\n");
        fclose(logger->file);
        logger->file = NULL;
        return false;
    }

    logger->fft_size = fft_size;
    logger->sample_rate = sample_rate;
    logger->is_logging = true;
    logger->frame_count = 0;
    logger->start_time = header.start_time;

    printf("[LOGGER] Started binary logging to: %s\n", logger->filepath);
    printf("[LOGGER] FFT Size: %u, Sample Rate: %u Hz\n", fft_size, sample_rate);

    return true;
}

bool data_logger_write_frame(data_logger_t* logger,
                             const float* signal,
                             const float* magnitude,
                             const float* psd,
                             uint64_t timestamp_ms) {
    if (!logger->is_logging) {
        return false;
    }

#ifdef USE_HDF5
    if (logger->format == LOG_FORMAT_HDF5) {
        bool result = hdf5_write_frame(logger, signal, magnitude, psd);
        if (result) {
            logger->frame_count++;
        }
        return result;
    }
#endif

    if (!logger->file) {
        return false;
    }

    if (logger->format == LOG_FORMAT_CSV) {
        // CSV format: calculate summary statistics
        float signal_avg = 0.0f;
        float magnitude_peak = 0.0f;
        float psd_avg = 0.0f;

        if (signal) {
            for (int i = 0; i < (int)logger->fft_size; i++) {
                signal_avg += fabsf(signal[i]);
            }
            signal_avg /= logger->fft_size;
        }

        if (magnitude) {
            for (int i = 0; i < (int)(logger->fft_size / 2); i++) {
                if (magnitude[i] > magnitude_peak) {
                    magnitude_peak = magnitude[i];
                }
            }
        }

        if (psd) {
            for (int i = 0; i < 128; i++) {
                psd_avg += psd[i];
            }
            psd_avg /= 128.0f;
        }

        // Calculate SNR
        float snr_db = magnitude ? data_logger_calculate_snr(magnitude, logger->fft_size, logger->sample_rate) : 0.0f;

        // Write CSV line
        fprintf(logger->file, "%llu,%.6f,%.6f,%.3f,%.2f\n",
                (unsigned long long)timestamp_ms,
                signal_avg,
                magnitude_peak,
                psd_avg,
                snr_db);

    } else {
        // Binary format
        data_frame_header_t frame_header;
        frame_header.timestamp_ms = timestamp_ms;

        if (fwrite(&frame_header, sizeof(frame_header), 1, logger->file) != 1) {
            fprintf(stderr, "[LOGGER] Failed to write frame header\n");
            return false;
        }

        // Write signal data (time domain)
        if (signal && fwrite(signal, sizeof(float), logger->fft_size, logger->file) != logger->fft_size) {
            fprintf(stderr, "[LOGGER] Failed to write signal data\n");
            return false;
        }

        // Write magnitude data (FFT)
        size_t mag_size = logger->fft_size / 2;
        if (magnitude && fwrite(magnitude, sizeof(float), mag_size, logger->file) != mag_size) {
            fprintf(stderr, "[LOGGER] Failed to write magnitude data\n");
            return false;
        }

        // Write PSD data (128 bins)
        if (psd && fwrite(psd, sizeof(float), 128, logger->file) != 128) {
            fprintf(stderr, "[LOGGER] Failed to write PSD data\n");
            return false;
        }
    }

    // Flush every 10 frames to ensure data is written
    logger->frame_count++;
    if (logger->frame_count % 10 == 0) {
        fflush(logger->file);
    }

    return true;
}

void data_logger_stop(data_logger_t* logger) {
    if (!logger->is_logging) {
        return;
    }

#ifdef USE_HDF5
    if (logger->format == LOG_FORMAT_HDF5) {
        // Close HDF5 datasets and file
        if (logger->hdf5_signal_dset >= 0) {
            H5Dclose(logger->hdf5_signal_dset);
            logger->hdf5_signal_dset = -1;
        }
        if (logger->hdf5_magnitude_dset >= 0) {
            H5Dclose(logger->hdf5_magnitude_dset);
            logger->hdf5_magnitude_dset = -1;
        }
        if (logger->hdf5_psd_dset >= 0) {
            H5Dclose(logger->hdf5_psd_dset);
            logger->hdf5_psd_dset = -1;
        }
        if (logger->hdf5_file >= 0) {
            H5Fclose(logger->hdf5_file);
            logger->hdf5_file = -1;
        }
    } else
#endif
    {
        if (logger->file) {
            fflush(logger->file);
            fclose(logger->file);
            logger->file = NULL;
        }
    }

    printf("[LOGGER] Stopped logging. Wrote %llu frames to: %s\n",
           (unsigned long long)logger->frame_count, logger->filepath);

    logger->is_logging = false;
    logger->frame_count = 0;
}

const char* data_logger_get_filepath(const data_logger_t* logger) {
    return logger->filepath;
}

bool data_logger_is_active(const data_logger_t* logger) {
    return logger->is_logging;
}

bool data_logger_start_csv(data_logger_t* logger, const char* filename,
                           uint32_t fft_size, uint32_t sample_rate) {
    if (logger->is_logging) {
        fprintf(stderr, "[LOGGER] Already logging to %s\n", logger->filepath);
        return false;
    }

    // Generate filename if not provided
    char temp_filename[256];
    if (filename == NULL || strlen(filename) == 0) {
        get_timestamp_filename(temp_filename, sizeof(temp_filename), "fft_data", "csv");
    } else {
        snprintf(temp_filename, sizeof(temp_filename), "%s", filename);
    }

    // Ensure directory exists
    ensure_directory_exists(logger->log_directory);

    // Build full path with directory
    snprintf(logger->filepath, sizeof(logger->filepath), "%s/%s",
             logger->log_directory, temp_filename);

    // Open file for CSV writing
    logger->file = fopen(logger->filepath, "w");
    if (!logger->file) {
        perror("[LOGGER] Failed to open CSV file");
        return false;
    }

    // Write CSV header
    fprintf(logger->file, "# FFT Analyzer Data Log\n");
    fprintf(logger->file, "# FFT Size: %u\n", fft_size);
    fprintf(logger->file, "# Sample Rate: %u Hz\n", sample_rate);
    fprintf(logger->file, "# Start Time: %llu\n", (unsigned long long)time(NULL));
    fprintf(logger->file, "# Format: Timestamp(ms), Signal_Avg, Magnitude_Peak, PSD_Avg, SNR(dB)\n");
    fprintf(logger->file, "Timestamp_ms,Signal_Avg,Magnitude_Peak,PSD_Avg,SNR_dB\n");

    logger->fft_size = fft_size;
    logger->sample_rate = sample_rate;
    logger->is_logging = true;
    logger->frame_count = 0;
    logger->start_time = (uint64_t)time(NULL);
    logger->format = LOG_FORMAT_CSV;

    printf("[LOGGER] Started CSV logging to: %s\n", logger->filepath);
    return true;
}

float data_logger_calculate_snr(const float* magnitude, int size, int sample_rate) {
    if (!magnitude || size < 2) return 0.0f;

    // Find peak signal (max magnitude)
    float peak_signal = 0.0f;
    int peak_bin = 0;
    for (int i = 1; i < size / 2; i++) {  // Skip DC bin
        if (magnitude[i] > peak_signal) {
            peak_signal = magnitude[i];
            peak_bin = i;
        }
    }

    // Estimate noise floor (average of bins away from peak)
    float noise_sum = 0.0f;
    int noise_count = 0;
    int exclusion_bins = 5;  // Exclude bins near peak

    for (int i = 1; i < size / 2; i++) {
        if (abs(i - peak_bin) > exclusion_bins) {
            noise_sum += magnitude[i];
            noise_count++;
        }
    }

    float noise_floor = (noise_count > 0) ? (noise_sum / noise_count) : 1e-10f;

    // Calculate SNR in dB
    float snr_linear = peak_signal / (noise_floor + 1e-10f);
    float snr_db = 20.0f * log10f(snr_linear + 1e-10f);

    return snr_db;
}

void data_logger_set_auto_record(data_logger_t* logger, bool enabled, float snr_threshold_db) {
    logger->auto_record_enabled = enabled;
    logger->snr_threshold_db = snr_threshold_db;

    if (enabled) {
        printf("[LOGGER] Auto-record enabled: SNR threshold = %.1f dB\n", snr_threshold_db);
    } else {
        printf("[LOGGER] Auto-record disabled\n");
    }
}

bool data_logger_check_auto_trigger(data_logger_t* logger, float current_snr_db,
                                    uint32_t fft_size, uint32_t sample_rate) {
    if (!logger->auto_record_enabled) {
        return false;
    }

    if (!logger->is_logging && current_snr_db >= logger->snr_threshold_db) {
        // Trigger auto-record
        printf("[LOGGER] Auto-record triggered! SNR: %.1f dB (threshold: %.1f dB)\n",
               current_snr_db, logger->snr_threshold_db);
        return data_logger_start_binary(logger, NULL, fft_size, sample_rate);
    }

    return false;
}

void data_logger_set_directory(data_logger_t* logger, const char* directory) {
    if (logger->is_logging) {
        fprintf(stderr, "[LOGGER] Cannot change directory while logging\n");
        return;
    }

    if (directory == NULL || strlen(directory) == 0) {
        strcpy(logger->log_directory, ".");
    } else {
        snprintf(logger->log_directory, sizeof(logger->log_directory), "%s", directory);
        // Remove trailing slash if present
        size_t len = strlen(logger->log_directory);
        if (len > 0 && (logger->log_directory[len-1] == '/' || logger->log_directory[len-1] == '\\')) {
            logger->log_directory[len-1] = '\0';
        }
    }

    printf("[LOGGER] Log directory set to: %s\n", logger->log_directory);
}

const char* data_logger_get_directory(const data_logger_t* logger) {
    return logger->log_directory;
}

#ifdef USE_HDF5
bool data_logger_start_hdf5(data_logger_t* logger, const char* filename,
                            uint32_t fft_size, uint32_t sample_rate) {
    if (logger->is_logging) {
        fprintf(stderr, "[LOGGER] Already logging to %s\n", logger->filepath);
        return false;
    }

    // Generate filename if not provided
    char temp_filename[256];
    if (filename == NULL || strlen(filename) == 0) {
        get_timestamp_filename(temp_filename, sizeof(temp_filename), "fft_data", "h5");
    } else {
        snprintf(temp_filename, sizeof(temp_filename), "%s", filename);
    }

    // Ensure directory exists
    ensure_directory_exists(logger->log_directory);

    // Build full path with directory
    snprintf(logger->filepath, sizeof(logger->filepath), "%s/%s",
             logger->log_directory, temp_filename);

    // Create HDF5 file
    logger->hdf5_file = H5Fcreate(logger->filepath, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (logger->hdf5_file < 0) {
        fprintf(stderr, "[LOGGER] Failed to create HDF5 file: %s\n", logger->filepath);
        return false;
    }

    // Create metadata group
    hid_t metadata_group = H5Gcreate2(logger->hdf5_file, "/metadata", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (metadata_group >= 0) {
        // Write metadata attributes
        hsize_t dims[1] = {1};
        hid_t dataspace = H5Screate_simple(1, dims, NULL);

        // FFT size
        hid_t attr = H5Acreate2(metadata_group, "fft_size", H5T_NATIVE_UINT32, dataspace, H5P_DEFAULT, H5P_DEFAULT);
        H5Awrite(attr, H5T_NATIVE_UINT32, &fft_size);
        H5Aclose(attr);

        // Sample rate
        attr = H5Acreate2(metadata_group, "sample_rate", H5T_NATIVE_UINT32, dataspace, H5P_DEFAULT, H5P_DEFAULT);
        H5Awrite(attr, H5T_NATIVE_UINT32, &sample_rate);
        H5Aclose(attr);

        // Start time
        uint64_t start_time = (uint64_t)time(NULL);
        attr = H5Acreate2(metadata_group, "start_time", H5T_NATIVE_UINT64, dataspace, H5P_DEFAULT, H5P_DEFAULT);
        H5Awrite(attr, H5T_NATIVE_UINT64, &start_time);
        H5Aclose(attr);

        H5Sclose(dataspace);
        H5Gclose(metadata_group);
    }

    // Create extensible datasets with chunking and compression
    hsize_t init_dims[2] = {0, 0};
    hsize_t max_dims[2] = {H5S_UNLIMITED, 0};
    hsize_t chunk_dims[2] = {10, 0};  // 10 frames per chunk

    // Signal dataset (time domain)
    init_dims[1] = fft_size;
    max_dims[1] = fft_size;
    chunk_dims[1] = fft_size;
    hid_t signal_space = H5Screate_simple(2, init_dims, max_dims);
    hid_t signal_prop = H5Pcreate(H5P_DATASET_CREATE);
    H5Pset_chunk(signal_prop, 2, chunk_dims);
    H5Pset_deflate(signal_prop, 6);  // gzip compression level 6
    logger->hdf5_signal_dset = H5Dcreate2(logger->hdf5_file, "/signal", H5T_NATIVE_FLOAT,
                                          signal_space, H5P_DEFAULT, signal_prop, H5P_DEFAULT);
    H5Pclose(signal_prop);
    H5Sclose(signal_space);

    // Magnitude dataset (FFT)
    init_dims[1] = fft_size / 2;
    max_dims[1] = fft_size / 2;
    chunk_dims[1] = fft_size / 2;
    hid_t mag_space = H5Screate_simple(2, init_dims, max_dims);
    hid_t mag_prop = H5Pcreate(H5P_DATASET_CREATE);
    H5Pset_chunk(mag_prop, 2, chunk_dims);
    H5Pset_deflate(mag_prop, 6);
    logger->hdf5_magnitude_dset = H5Dcreate2(logger->hdf5_file, "/magnitude", H5T_NATIVE_FLOAT,
                                             mag_space, H5P_DEFAULT, mag_prop, H5P_DEFAULT);
    H5Pclose(mag_prop);
    H5Sclose(mag_space);

    // PSD dataset
    init_dims[1] = 128;
    max_dims[1] = 128;
    chunk_dims[1] = 128;
    hid_t psd_space = H5Screate_simple(2, init_dims, max_dims);
    hid_t psd_prop = H5Pcreate(H5P_DATASET_CREATE);
    H5Pset_chunk(psd_prop, 2, chunk_dims);
    H5Pset_deflate(psd_prop, 6);
    logger->hdf5_psd_dset = H5Dcreate2(logger->hdf5_file, "/psd", H5T_NATIVE_FLOAT,
                                       psd_space, H5P_DEFAULT, psd_prop, H5P_DEFAULT);
    H5Pclose(psd_prop);
    H5Sclose(psd_space);

    logger->fft_size = fft_size;
    logger->sample_rate = sample_rate;
    logger->is_logging = true;
    logger->frame_count = 0;
    logger->start_time = (uint64_t)time(NULL);
    logger->format = LOG_FORMAT_HDF5;

    printf("[LOGGER] Started HDF5 logging to: %s\n", logger->filepath);
    printf("[LOGGER] Compression: gzip level 6, Chunking: 10 frames\n");

    return true;
}

// Write frame to HDF5
static bool hdf5_write_frame(data_logger_t* logger, const float* signal,
                             const float* magnitude, const float* psd) {
    hsize_t new_dims[2];
    hsize_t offset[2];
    hsize_t count[2] = {1, 0};

    // Extend datasets
    new_dims[0] = logger->frame_count + 1;

    // Write signal
    if (signal) {
        new_dims[1] = logger->fft_size;
        H5Dset_extent(logger->hdf5_signal_dset, new_dims);
        hid_t filespace = H5Dget_space(logger->hdf5_signal_dset);
        offset[0] = logger->frame_count;
        offset[1] = 0;
        count[1] = logger->fft_size;
        H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, count, NULL);
        hid_t memspace = H5Screate_simple(2, count, NULL);
        H5Dwrite(logger->hdf5_signal_dset, H5T_NATIVE_FLOAT, memspace, filespace, H5P_DEFAULT, signal);
        H5Sclose(memspace);
        H5Sclose(filespace);
    }

    // Write magnitude
    if (magnitude) {
        new_dims[1] = logger->fft_size / 2;
        H5Dset_extent(logger->hdf5_magnitude_dset, new_dims);
        hid_t filespace = H5Dget_space(logger->hdf5_magnitude_dset);
        offset[0] = logger->frame_count;
        offset[1] = 0;
        count[1] = logger->fft_size / 2;
        H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, count, NULL);
        hid_t memspace = H5Screate_simple(2, count, NULL);
        H5Dwrite(logger->hdf5_magnitude_dset, H5T_NATIVE_FLOAT, memspace, filespace, H5P_DEFAULT, magnitude);
        H5Sclose(memspace);
        H5Sclose(filespace);
    }

    // Write PSD
    if (psd) {
        new_dims[1] = 128;
        H5Dset_extent(logger->hdf5_psd_dset, new_dims);
        hid_t filespace = H5Dget_space(logger->hdf5_psd_dset);
        offset[0] = logger->frame_count;
        offset[1] = 0;
        count[1] = 128;
        H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, count, NULL);
        hid_t memspace = H5Screate_simple(2, count, NULL);
        H5Dwrite(logger->hdf5_psd_dset, H5T_NATIVE_FLOAT, memspace, filespace, H5P_DEFAULT, psd);
        H5Sclose(memspace);
        H5Sclose(filespace);
    }

    return true;
}
#endif
