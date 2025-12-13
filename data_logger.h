/*
 * data_logger.h
 *
 * Data logging functionality for FFT analyzer
 * Supports binary and HDF5 formats
 */

#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/*===========================================================================
 * Binary Format Specification
 *===========================================================================
 *
 * File Header (64 bytes):
 *   - Magic: "FFTLOG01" (8 bytes)
 *   - Version: uint32_t (4 bytes)
 *   - FFT Size: uint32_t (4 bytes)
 *   - Sample Rate: uint32_t (4 bytes)
 *   - Start Time (Unix): uint64_t (8 bytes)
 *   - Reserved: (36 bytes)
 *
 * Data Frame (variable size):
 *   - Timestamp: uint64_t (8 bytes, milliseconds since epoch)
 *   - Signal: float[fft_size] (time domain data)
 *   - Magnitude: float[fft_size/2] (FFT magnitude)
 *   - PSD: float[128] (power spectral density in dB)
 */

#define DATA_LOGGER_MAGIC "FFTLOG01"
#define DATA_LOGGER_VERSION 1

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t fft_size;
    uint32_t sample_rate;
    uint64_t start_time;
    uint8_t reserved[36];
} __attribute__((packed)) data_logger_header_t;

typedef struct {
    uint64_t timestamp_ms;
    // Followed by variable-length data:
    // float signal[fft_size]
    // float magnitude[fft_size/2]
    // float psd[128]
} __attribute__((packed)) data_frame_header_t;

typedef enum {
    LOG_FORMAT_BINARY,
    LOG_FORMAT_CSV,
    LOG_FORMAT_HDF5
} log_format_t;

typedef struct {
    FILE* file;
    bool is_logging;
    bool auto_record_enabled;
    float snr_threshold_db;
    char filepath[512];
    char log_directory[256];
    uint32_t fft_size;
    uint32_t sample_rate;
    uint64_t frame_count;
    uint64_t start_time;
    log_format_t format;
#ifdef USE_HDF5
    int hdf5_file;              // HDF5 file handle
    int hdf5_signal_dset;       // Signal dataset handle
    int hdf5_magnitude_dset;    // Magnitude dataset handle
    int hdf5_psd_dset;          // PSD dataset handle
#endif
} data_logger_t;

/**
 * Initialize data logger
 */
void data_logger_init(data_logger_t* logger);

/**
 * Start logging to binary file
 * Returns: true on success, false on error
 */
bool data_logger_start_binary(data_logger_t* logger, const char* filename,
                              uint32_t fft_size, uint32_t sample_rate);

/**
 * Log a single frame of data
 */
bool data_logger_write_frame(data_logger_t* logger,
                             const float* signal,
                             const float* magnitude,
                             const float* psd,
                             uint64_t timestamp_ms);

/**
 * Stop logging and close file
 */
void data_logger_stop(data_logger_t* logger);

/**
 * Get current log file path
 */
const char* data_logger_get_filepath(const data_logger_t* logger);

/**
 * Check if currently logging
 */
bool data_logger_is_active(const data_logger_t* logger);

/**
 * Start logging to CSV file
 * Returns: true on success, false on error
 */
bool data_logger_start_csv(data_logger_t* logger, const char* filename,
                           uint32_t fft_size, uint32_t sample_rate);

/**
 * Set auto-record mode with SNR threshold
 * When enabled, recording automatically starts when SNR exceeds threshold
 */
void data_logger_set_auto_record(data_logger_t* logger, bool enabled, float snr_threshold_db);

/**
 * Check SNR and trigger auto-record if threshold exceeded
 * Returns: true if auto-record was triggered
 */
bool data_logger_check_auto_trigger(data_logger_t* logger, float current_snr_db,
                                    uint32_t fft_size, uint32_t sample_rate);

/**
 * Calculate SNR from magnitude data
 * Returns: SNR in dB
 */
float data_logger_calculate_snr(const float* magnitude, int size, int sample_rate);

/**
 * Set log directory for saving files
 * If NULL or empty, uses current directory
 */
void data_logger_set_directory(data_logger_t* logger, const char* directory);

/**
 * Get current log directory
 */
const char* data_logger_get_directory(const data_logger_t* logger);

#ifdef USE_HDF5
/**
 * Start logging to HDF5 file
 * Requires HDF5 library to be linked
 */
bool data_logger_start_hdf5(data_logger_t* logger, const char* filename,
                            uint32_t fft_size, uint32_t sample_rate);
#endif

#endif // DATA_LOGGER_H
