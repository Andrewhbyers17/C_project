/*
 * web_server.h
 *
 * Lightweight HTTP server for streaming FFT data to web browsers
 * Uses simple socket-based HTTP server (no external dependencies)
 *
 * Features:
 * - Serves static HTML/CSS/JS files
 * - Provides JSON API for FFT data
 * - Real-time spectrum data streaming
 * - CORS support for development
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================
 * Configuration
 *===========================================================================*/

#define WEB_SERVER_PORT         8080
#define WEB_SERVER_MAX_CLIENTS  4
#define WEB_SERVER_TIMEOUT_SEC  5
#define WEB_SERVER_BUFFER_SIZE  4096

/*===========================================================================
 * FFT Data Structure for Web Interface
 *===========================================================================*/

typedef struct {
    int fft_size;           // FFT size (e.g., 512)
    int sample_rate;        // Sample rate in Hz (e.g., 8000)
    int num_bands;          // Number of frequency bands (e.g., 8)
    int psd_size;           // PSD array size (128 for Welch with 256-pt segments)
    float* time_domain;     // Time-domain signal (fft_size values)
    float* magnitude;       // Magnitude spectrum (fft_size/2 values)
    float* psd;             // Power Spectral Density in dB/Hz (psd_size values)
    float* band_energies;   // Energy per band (num_bands values)
    uint8_t led_pattern;    // Current LED pattern
    const char* mode_name;  // Current waveform mode name
    bool paused;            // Pause state
    bool web_control_active; // True when switches=1111 (web control mode enabled)
    uint64_t timestamp;     // Timestamp in milliseconds
} fft_data_t;

/*===========================================================================
 * Web Server API
 *===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize web server on specified port
 * Returns: socket fd on success, -1 on failure
 */
int web_server_init(int port);

/**
 * Update FFT data to be served to clients
 * This should be called after each FFT computation
 */
void web_server_update_data(const fft_data_t* data);

/**
 * Handle incoming web requests (non-blocking)
 * Call this in your main loop to process client requests
 * Returns: number of requests handled
 */
int web_server_handle_requests(int server_fd);

/**
 * Shutdown web server and cleanup
 */
void web_server_cleanup(int server_fd);

/**
 * Get current timestamp in milliseconds
 */
uint64_t get_timestamp_ms(void);

/**
 * Set callback for mode change requests from web interface
 */
void web_server_set_mode_callback(void (*callback)(int mode));

/**
 * Set callback for pause toggle requests from web interface
 */
void web_server_set_pause_callback(void (*callback)(void));

/**
 * Set callback for logging toggle requests from web interface
 * Returns: true if logging started, false if stopped
 */
void web_server_set_log_callback(bool (*callback)(void));

/**
 * Get logging status callback
 * Returns: true if logging, false otherwise, and optionally the filepath
 */
void web_server_set_log_status_callback(bool (*callback)(char* filepath, size_t max_len));

/**
 * Set callback for starting logging with specific format
 * format: "binary", "csv", or "hdf5"
 * Returns: true if logging started successfully
 */
void web_server_set_log_start_callback(bool (*callback)(const char* format));

/**
 * Set callback for stopping logging
 */
void web_server_set_log_stop_callback(void (*callback)(void));

/**
 * Set callback to get current logging format
 * Returns: "binary", "csv", "hdf5", or empty string if not logging
 */
void web_server_set_log_format_callback(const char* (*callback)(void));

/**
 * Set callback for auto-record settings from web interface
 */
void web_server_set_auto_record_callback(void (*callback)(bool enabled, float threshold));

/**
 * Set callback for log directory changes from web interface
 */
void web_server_set_log_directory_callback(void (*callback)(const char* directory));

/**
 * Set callback to get current log directory
 */
void web_server_set_get_log_directory_callback(const char* (*callback)(void));

#ifdef __cplusplus
}
#endif

#endif /* WEB_SERVER_H */
