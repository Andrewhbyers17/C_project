/*
 * fft_analyzer_network.c
 *
 * FFT Analyzer with Network Input - Windows Compatible
 * Reads signal data from TCP/UDP socket instead of FPGA hardware
 *
 * Usage:
 *   ./fft_analyzer_network --source 192.168.1.100:5000 --protocol tcp
 *   Then open browser to: http://localhost:8080
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <time.h>

// Windows-specific includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <direct.h>  // for _mkdir
    #pragma comment(lib, "ws2_32.lib")
    #define usleep(x) Sleep((x)/1000)
    typedef int socklen_t;
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <sys/stat.h>  // for mkdir
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

#include "web_server.h"
#include "data_logger.h"
#include "dsp.h"
#include "network.h"
#include "ring_buffer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*===========================================================================
 * Version Information
 *===========================================================================*/

#define VERSION_MAJOR       1
#define VERSION_MINOR       0
#define VERSION_PATCH       0
#define VERSION_STRING      "1.0.0"

/*===========================================================================
 * Configuration Constants
 *===========================================================================*/

// High-speed configuration for MHz-range signals
#define FFT_SIZE            4096        // Larger FFT for better freq resolution at high rates
#define NETWORK_BUFFER_SIZE (FFT_SIZE * 4)  // 16K floats = 64 KB (optimized for TCP window & fewer syscalls)
#define SAMPLE_RATE         10000000    // Default 10 MHz (can be auto-detected)
#define NUM_BANDS           8
#define UPDATE_RATE_MS      5           // 200 Hz updates (faster drain for high sample rates)
#define DEFAULT_LOG_DIR     "logs"

// Auto sample rate detection
#define AUTO_DETECT_SAMPLE_RATE  true   // Enable auto-detection from network data rate
#define TARGET_DISPLAY_RATE      20000  // Target 20 kHz for FFT display (adjusts decimation)

// Buffer sizes for various operations
#define DISK_WRITE_BUFFER_SIZE   32768  // 32K samples per disk write (128 KB)
#define DRAIN_BUFFER_MULTIPLIER  8      // Drain 8x network buffer when not recording
#define PSD_SEGMENT_SIZE         256    // Welch's method segment size (power of 2)

// Network and path limits
#define MAX_PATH_LENGTH          256    // Maximum file path length
#define MIN_PORT_NUMBER          1024   // Minimum allowed port number
#define MAX_PORT_NUMBER          65535  // Maximum allowed port number

// Conversion constants
#define BYTES_PER_KB             1024.0 // Bytes to KB conversion
#define BYTES_PER_MB             (1024.0 * 1024.0) // Bytes to MB conversion
#define MS_TO_MICROSECONDS       1000   // Milliseconds to microseconds

static const float BAND_EDGES[NUM_BANDS + 1] = {
    0, 200, 400, 600, 800, 1200, 1600, 2400, 4000
};

#define LED_THRESHOLD_LOW   0.01f

// DSP context structures now defined in dsp.h

/*===========================================================================
 * Connection State Management (uses network.h types)
 *===========================================================================*/

// Connection manager instance is defined below with globals

/*===========================================================================
 * Waveform Modes (for testing without network)
 *===========================================================================*/

typedef enum {
    MODE_NETWORK_INPUT = 0,
    MODE_SILENCE,
    MODE_SIGNAL_NOISE
} waveform_mode_t;

static const char* MODE_NAMES[] = {
    "Network Input",
    "Silence",
    "Signal + Noise"
};

/*===========================================================================
 * Global Variables
 *===========================================================================*/

static volatile bool g_running = true;
static volatile bool g_paused = false;
static int g_web_server_fd = -1;
static network_config_t g_network_config = {0};
static data_logger_t g_data_logger = {0};

// Mode control
static volatile int g_requested_mode = 0;  // Default to network input

// Connection manager for auto-reconnect
static connection_manager_t g_conn_manager = {
    .state = CONN_STATE_DISCONNECTED,
    .retry_count = 0,
    .max_retries = INT_MAX,  // Retry indefinitely
    .retry_delay_ms = 5000,  // 5 seconds between retries
    .last_retry_time = 0,
    .auto_reconnect = true
};

/*===========================================================================
 * Ring Buffer for Async Network Reception
 *===========================================================================*/

// Ring buffer size: Very large buffer for high data rates
// At 10 MHz with 4096 FFT: 5000 frames = ~2 seconds buffering = 80 MB
// At 2 MHz: ~10 seconds buffering
#define RING_BUFFER_FRAMES 5000
#define RING_BUFFER_SIZE (FFT_SIZE * RING_BUFFER_FRAMES)

static ring_buffer_t g_ring_buffer = {0};
static HANDLE g_network_thread = NULL;
static volatile bool g_network_thread_running = false;

/*===========================================================================
 * Disk Writer Thread for Raw IQ Recording
 *===========================================================================*/

static HANDLE g_disk_writer_thread = NULL;
static volatile bool g_disk_writer_running = false;
static volatile bool g_recording_raw_iq = false;

// Auto sample rate detection and decimation
static volatile uint32_t g_detected_sample_rate = SAMPLE_RATE;
static volatile uint32_t g_decimation_factor = 1;
static volatile uint32_t g_decim_counter = 0;
static volatile uint64_t g_total_samples_received = 0;  // Updated by network thread

/*===========================================================================
 * Signal Handler
 *===========================================================================*/

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\nShutting down...\n");
        g_running = false;
    }
}

// Network functions now in network.c module
// Ring buffer functions now in ring_buffer.c module

/*===========================================================================
 * Auto Sample Rate Detection
 *===========================================================================*/

void update_sample_rate_detection(void) {
    static time_t last_check = 0;
    static uint64_t last_samples = 0;
    static bool first_run = true;

    if (first_run) {
        last_check = time(NULL);
        first_run = false;
        return;
    }

    time_t now = time(NULL);

    // Update every second
    if (difftime(now, last_check) >= 1.0) {
        // Count samples received (now tracked by network thread)
        uint64_t samples_delta = g_total_samples_received - last_samples;

        if (samples_delta > 0) {
            // Detected sample rate (samples per second)
            uint32_t detected_rate = (uint32_t)samples_delta;

            // Only update if significantly different (>10% change)
            if (g_detected_sample_rate == 0 ||
                abs((int)detected_rate - (int)g_detected_sample_rate) > (int)(g_detected_sample_rate * 0.1)) {

                g_detected_sample_rate = detected_rate;

                // Calculate optimal decimation factor for target display rate
                if (detected_rate > TARGET_DISPLAY_RATE) {
                    g_decimation_factor = detected_rate / TARGET_DISPLAY_RATE;
                    if (g_decimation_factor < 1) g_decimation_factor = 1;
                } else {
                    g_decimation_factor = 1;
                }

                printf("[AUTO] Detected sample rate: %u Hz (%.2f MHz)\n",
                       detected_rate, detected_rate / 1e6);
                printf("[AUTO] Decimation factor: %u (display rate: %u Hz)\n",
                       g_decimation_factor, detected_rate / g_decimation_factor);
            }
        }

        last_check = now;
        last_samples = g_total_samples_received;
    }
    // Note: g_total_samples_received is now updated by network thread
}

bool attempt_reconnect(network_config_t* config) {
    time_t now = time(NULL);

    // Check if enough time has passed since last retry
    if (g_conn_manager.last_retry_time > 0 &&
        difftime(now, g_conn_manager.last_retry_time) <
        (g_conn_manager.retry_delay_ms / 1000.0)) {
        return false;  // Too soon to retry
    }

    // Check retry limit (disabled - retry indefinitely)
    // Note: max_retries set to INT_MAX for unlimited retries

    // Show reconnection message (only show count for first few attempts)
    if (g_conn_manager.retry_count < 3) {
        printf("[*] Attempting to connect to %s:%d...\n",
               config->host, config->port);
    } else if (g_conn_manager.retry_count % 10 == 0) {
        // Show periodic reminder every 10 attempts
        printf("[*] Still waiting for %s:%d to become available...\n",
               config->host, config->port);
    }

    g_conn_manager.state = CONN_STATE_RECONNECTING;
    g_conn_manager.last_retry_time = now;
    g_conn_manager.retry_count++;

    // Close old connection
    network_close(config);

    // Attempt new connection
    int result = network_connect(config);

    if (result >= 0) {
        printf("[OK] Reconnected successfully after %d attempt(s)\n",
               g_conn_manager.retry_count);
        g_conn_manager.state = CONN_STATE_CONNECTED;
        g_conn_manager.retry_count = 0;  // Reset counter on success
        return true;
    } else {
        fprintf(stderr, "[ERROR] Reconnection attempt %d failed\n",
                g_conn_manager.retry_count);
        g_conn_manager.state = CONN_STATE_DISCONNECTED;
        return false;
    }
}

/*===========================================================================
 * Network Receiver Thread (Producer)
 *===========================================================================*/

// Background thread that continuously reads from network and fills ring buffer
DWORD WINAPI network_receiver_thread(LPVOID param) {
    network_config_t* config = (network_config_t*)param;
    float* temp_buffer = (float*)malloc(NETWORK_BUFFER_SIZE * sizeof(float));  // Larger buffer for fewer syscalls

    if (!temp_buffer) {
        fprintf(stderr, "[ERROR] Network thread: Failed to allocate temp buffer\n");
        return 1;
    }

    printf("[OK] Network receiver thread started (buffer: %d samples)\n", NETWORK_BUFFER_SIZE);

    while (g_network_thread_running && g_running) {
        // Check if we're connected
        if (g_conn_manager.state != CONN_STATE_CONNECTED) {
            Sleep(100);  // Wait 100ms before checking again
            continue;
        }

        // Read samples from network (larger buffer = fewer syscalls)
        int samples_read = network_read_samples(config, temp_buffer, NETWORK_BUFFER_SIZE);

        if (samples_read < 0) {
            // Connection lost
            fprintf(stderr, "[ERROR] Network thread: Connection lost\n");
            g_conn_manager.state = CONN_STATE_DISCONNECTED;

            // Try to reconnect
            if (g_conn_manager.auto_reconnect && attempt_reconnect(config)) {
                printf("[OK] Network thread: Reconnected\n");
                continue;
            } else {
                Sleep(1000);  // Wait 1 second before retry
                continue;
            }
        }

        if (samples_read == NETWORK_BUFFER_SIZE) {
            // Write to ring buffer
            int written = ring_buffer_write(&g_ring_buffer, temp_buffer, samples_read);

            // Update sample counter for auto-detection
            g_total_samples_received += samples_read;

            if (written < samples_read) {
                // Ring buffer overflow - main thread is too slow
                static int overflow_count = 0;
                if (++overflow_count % 100 == 0) {
                    fprintf(stderr, "[WARN] Ring buffer overflow (%d times) - increase buffer or reduce sample rate\n",
                            overflow_count);
                }
            }
        } else if (samples_read > 0) {
            // Partial read - shouldn't happen with TCP, but handle it
            fprintf(stderr, "[WARN] Network thread: Partial read (%d/%d samples)\n",
                    samples_read, NETWORK_BUFFER_SIZE);
            // Still count partial reads for auto-detection
            g_total_samples_received += samples_read;
        }

        // No sleep - read as fast as data arrives (this is the key difference from main loop)
    }

    free(temp_buffer);
    printf("[OK] Network receiver thread stopped\n");
    return 0;
}

// Start the network receiver thread
bool start_network_thread(network_config_t* config) {
    if (g_network_thread != NULL) {
        fprintf(stderr, "[WARN] Network thread already running\n");
        return false;
    }

    g_network_thread_running = true;
    g_network_thread = CreateThread(
        NULL,                           // Default security attributes
        0,                              // Default stack size
        network_receiver_thread,        // Thread function
        config,                         // Thread parameter
        0,                              // Default creation flags
        NULL                            // Don't need thread ID
    );

    if (g_network_thread == NULL) {
        fprintf(stderr, "[ERROR] Failed to create network receiver thread\n");
        g_network_thread_running = false;
        return false;
    }

    printf("[OK] Network receiver thread created\n");
    return true;
}

// Stop the network receiver thread
void stop_network_thread(void) {
    if (g_network_thread == NULL) {
        return;
    }

    printf("[*] Stopping network receiver thread...\n");
    g_network_thread_running = false;

    // Wait for thread to finish (max 5 seconds)
    DWORD result = WaitForSingleObject(g_network_thread, 5000);
    if (result == WAIT_TIMEOUT) {
        fprintf(stderr, "[WARN] Network thread did not stop gracefully, terminating\n");
        TerminateThread(g_network_thread, 1);
    }

    CloseHandle(g_network_thread);
    g_network_thread = NULL;
    printf("[OK] Network receiver thread stopped\n");
}

/*===========================================================================
 * Disk Writer Thread Functions
 *===========================================================================*/

// Disk writer thread - continuously writes from ring buffer to HDF5
DWORD WINAPI disk_writer_thread(LPVOID param) {
    // Large write buffer for high-speed streaming (32k samples = 128 KB)
    const int WRITE_BUFFER_SIZE = DISK_WRITE_BUFFER_SIZE;
    float* write_buffer = (float*)malloc(WRITE_BUFFER_SIZE * sizeof(float));

    if (!write_buffer) {
        fprintf(stderr, "[ERROR] Failed to allocate disk writer buffer\n");
        return 1;
    }

    printf("[OK] Disk writer thread started\n");

    while (g_disk_writer_running && g_running) {
        // Aggressively drain ring buffer - read as much as possible
        int n = ring_buffer_read(&g_ring_buffer, write_buffer, WRITE_BUFFER_SIZE);

        if (n > 0) {
            // Write to HDF5 file
            if (!data_logger_write_raw_iq(&g_data_logger, write_buffer, n)) {
                fprintf(stderr, "[ERROR] Failed to write IQ data to disk\n");
                // Continue anyway - don't stop on write errors
            }
            // No sleep - keep draining buffer as fast as possible
        } else {
            // Buffer empty, very brief sleep to avoid spinning
            Sleep(1);
        }
    }

    free(write_buffer);
    printf("[OK] Disk writer thread stopped\n");
    return 0;
}

// Start the disk writer thread
bool start_disk_writer_thread(void) {
    if (g_disk_writer_thread != NULL) {
        fprintf(stderr, "[WARN] Disk writer thread already running\n");
        return false;
    }

    g_disk_writer_running = true;
    g_disk_writer_thread = CreateThread(
        NULL,                   // default security attributes
        0,                      // default stack size
        disk_writer_thread,     // thread function
        NULL,                   // argument to thread function
        0,                      // default creation flags
        NULL                    // returns thread identifier
    );

    if (g_disk_writer_thread == NULL) {
        fprintf(stderr, "[ERROR] Failed to create disk writer thread\n");
        g_disk_writer_running = false;
        return false;
    }

    printf("[OK] Disk writer thread created\n");
    return true;
}

// Stop the disk writer thread
void stop_disk_writer_thread(void) {
    if (g_disk_writer_thread == NULL) {
        return;
    }

    printf("[*] Stopping disk writer thread...\n");
    g_disk_writer_running = false;

    // Wait for thread to finish (max 5 seconds)
    DWORD result = WaitForSingleObject(g_disk_writer_thread, 5000);
    if (result == WAIT_TIMEOUT) {
        fprintf(stderr, "[WARN] Disk writer thread did not stop gracefully, terminating\n");
        TerminateThread(g_disk_writer_thread, 1);
    }

    CloseHandle(g_disk_writer_thread);
    g_disk_writer_thread = NULL;
    printf("[OK] Disk writer thread stopped\n");
}

// DSP functions now in dsp.c module

/*===========================================================================
 * Waveform Generation (for testing)
 *===========================================================================*/

void generate_silence(float* buffer, int size) {
    memset(buffer, 0, size * sizeof(float));
}

void generate_signal_noise(float* buffer, int size) {
    float signal_freq = 1000.0f;
    float signal_amp = 0.5f;
    float noise_amp = 0.3f;

    for (int i = 0; i < size; i++) {
        float t = (float)i / SAMPLE_RATE;
        float signal = signal_amp * sinf(2.0f * M_PI * signal_freq * t);

        float u1 = (float)rand() / RAND_MAX;
        float u2 = (float)rand() / RAND_MAX;
        float noise = noise_amp * sqrtf(-2.0f * logf(u1 + 1e-10f)) * cosf(2.0f * M_PI * u2);

        buffer[i] = signal + noise;
    }
}

// DSP functions now in dsp.c module

/*===========================================================================
 * Web Callbacks
 *===========================================================================*/

void web_mode_change_callback(int mode) {
    g_requested_mode = mode;
    printf("[WEB] Mode change requested: %d (%s)\n", mode, MODE_NAMES[mode]);
}

void web_pause_toggle_callback(void) {
    g_paused = !g_paused;
    printf("[WEB] Pause toggled: %s\n", g_paused ? "PAUSED" : "RESUMED");
}

bool web_log_toggle_callback(void) {
    if (data_logger_is_active(&g_data_logger)) {
        // Stop logging
        data_logger_stop(&g_data_logger);
        return false;
    } else {
        // Start logging with auto-generated filename
        bool success = data_logger_start_binary(&g_data_logger, NULL, FFT_SIZE, SAMPLE_RATE);
        return success;
    }
}

bool web_log_status_callback(char* filepath, size_t max_len) {
    bool is_logging = data_logger_is_active(&g_data_logger);
    if (is_logging && filepath && max_len > 0) {
        const char* path = data_logger_get_filepath(&g_data_logger);
        snprintf(filepath, max_len, "%s", path);
    }
    return is_logging;
}

bool web_log_start_callback(const char* format) {
    if (data_logger_is_active(&g_data_logger)) {
        // Already logging, stop first
        data_logger_stop(&g_data_logger);

        // Stop disk writer if raw IQ was recording
        if (g_recording_raw_iq) {
            stop_disk_writer_thread();
            g_recording_raw_iq = false;
        }
    }

    bool success = false;
    if (strcmp(format, "binary") == 0) {
        success = data_logger_start_binary(&g_data_logger, NULL, FFT_SIZE, SAMPLE_RATE);
        printf("[WEB] Starting BINARY logging\n");
    } else if (strcmp(format, "csv") == 0) {
        success = data_logger_start_csv(&g_data_logger, NULL, FFT_SIZE, SAMPLE_RATE);
        printf("[WEB] Starting CSV logging\n");
    }
#ifdef USE_HDF5
    else if (strcmp(format, "hdf5") == 0) {
        success = data_logger_start_hdf5(&g_data_logger, NULL, FFT_SIZE, SAMPLE_RATE);
        printf("[WEB] Starting HDF5 logging\n");
    } else if (strcmp(format, "raw_iq") == 0) {
        // Start raw IQ streaming with detected sample rate (or default if not detected)
        uint32_t record_rate = (g_detected_sample_rate > 0) ? g_detected_sample_rate : SAMPLE_RATE;
        success = data_logger_start_raw_iq(&g_data_logger, NULL, record_rate);
        if (success) {
            // Start disk writer thread
            if (start_disk_writer_thread()) {
                g_recording_raw_iq = true;
                printf("[WEB] Starting RAW IQ streaming at %u Hz (%.2f MHz)\n",
                       record_rate, record_rate / 1e6);
            } else {
                data_logger_stop(&g_data_logger);
                success = false;
            }
        }
    }
#endif
    else {
        fprintf(stderr, "[WEB] Unknown logging format: %s\n", format);
        // Default to binary
        success = data_logger_start_binary(&g_data_logger, NULL, FFT_SIZE, SAMPLE_RATE);
    }

    return success;
}

void web_log_stop_callback(void) {
    if (data_logger_is_active(&g_data_logger)) {
        // Stop disk writer if raw IQ recording
        if (g_recording_raw_iq) {
            stop_disk_writer_thread();
            g_recording_raw_iq = false;
        }

        data_logger_stop(&g_data_logger);
        printf("[WEB] Logging stopped\n");
    }
}

const char* web_log_format_callback(void) {
    if (!data_logger_is_active(&g_data_logger)) {
        return "";
    }

    // Get format from data_logger
    switch (g_data_logger.format) {
        case LOG_FORMAT_BINARY: return "binary";
        case LOG_FORMAT_CSV: return "csv";
#ifdef USE_HDF5
        case LOG_FORMAT_HDF5: return "hdf5";
        case LOG_FORMAT_RAW_IQ: return "raw_iq";
#endif
        default: return "";
    }
}

void web_auto_record_callback(bool enabled, float threshold) {
    data_logger_set_auto_record(&g_data_logger, enabled, threshold);
    printf("[WEB] Auto-record %s (threshold: %.1f dB)\n",
           enabled ? "enabled" : "disabled", threshold);
}

void web_set_log_directory_callback(const char* directory) {
    data_logger_set_directory(&g_data_logger, directory);
}

const char* web_get_log_directory_callback(void) {
    return data_logger_get_directory(&g_data_logger);
}

/*===========================================================================
 * Main Application
 *===========================================================================*/

void print_usage(const char* prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Options:\n");
    printf("  --source IP:PORT    Network source (e.g., 192.168.1.100:5000)\n");
    printf("  --protocol tcp|udp  Network protocol (default: tcp)\n");
    printf("  --test              Use test waveforms instead of network\n");
    printf("  --port PORT         Web server port (default: 8080, range: %d-%d)\n", MIN_PORT_NUMBER, MAX_PORT_NUMBER);
    printf("  --help              Show this help\n\n");
    printf("Examples:\n");
    printf("  %s --source 192.168.1.100:5000 --protocol tcp\n", prog_name);
    printf("  %s --test  (use built-in test signals)\n\n", prog_name);
}

int main(int argc, char* argv[]) {
    int ret = 0;
    bool use_network = false;
    int web_port = 8080;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--source") == 0 && i + 1 < argc) {
            char* colon = strchr(argv[++i], ':');
            if (colon) {
                *colon = '\0';
                strncpy(g_network_config.host, argv[i], sizeof(g_network_config.host) - 1);
                g_network_config.port = atoi(colon + 1);
                use_network = true;
            }
        } else if (strcmp(argv[i], "--protocol") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "udp") == 0) {
                g_network_config.protocol = NET_PROTOCOL_UDP;
            } else {
                g_network_config.protocol = NET_PROTOCOL_TCP;
            }
        } else if (strcmp(argv[i], "--test") == 0) {
            use_network = false;
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            web_port = atoi(argv[++i]);
            // Validate port range
            if (web_port < MIN_PORT_NUMBER || web_port > MAX_PORT_NUMBER) {
                fprintf(stderr, "[ERROR] Invalid port %d. Must be %d-%d\n", web_port, MIN_PORT_NUMBER, MAX_PORT_NUMBER);
                print_usage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    printf("===========================================\n");
    printf("  FFT Analyzer v%s\n", VERSION_STRING);
    printf("  Real-Time Spectrum Analysis\n");
    printf("===========================================\n\n");

    // Signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize Windows sockets
    if (network_init() < 0) {
        return 1;
    }

    // Connect to network source if requested
    if (use_network) {
        printf("[*] Attempting to connect to %s:%d (%s)...\n",
               g_network_config.host, g_network_config.port,
               network_get_protocol_name(g_network_config.protocol));

        g_conn_manager.state = CONN_STATE_CONNECTING;
        if (network_connect(&g_network_config) < 0) {
            printf("[*] Waiting for network source to become available...\n");
            printf("[*] Auto-reconnect enabled. Will retry every %d seconds.\n",
                    g_conn_manager.retry_delay_ms / MS_TO_MICROSECONDS);
            printf("[*] You can start the data source anytime - analyzer will connect automatically.\n");
            printf("[*] Using test waveforms until network connection established.\n\n");
            g_conn_manager.state = CONN_STATE_DISCONNECTED;
        } else {
            g_conn_manager.state = CONN_STATE_CONNECTED;
            printf("[OK] Connected to %s:%d\n\n", g_network_config.host, g_network_config.port);
        }
    } else {
        printf("[*] Using test waveforms (no network input)\n");
        printf("    Use --source IP:PORT to connect to network source\n\n");
    }

    // Initialize web server
    printf("[*] Initializing web server on port %d...\n", web_port);
    g_web_server_fd = web_server_init(web_port);
    if (g_web_server_fd < 0) {
        fprintf(stderr, "[ERROR] Failed to initialize web server\n");
        ret = 1;
        goto cleanup;
    }

    // Register web callbacks
    web_server_set_mode_callback(web_mode_change_callback);
    web_server_set_pause_callback(web_pause_toggle_callback);
    web_server_set_log_callback(web_log_toggle_callback);
    web_server_set_log_status_callback(web_log_status_callback);
    web_server_set_log_start_callback(web_log_start_callback);
    web_server_set_log_stop_callback(web_log_stop_callback);
    web_server_set_log_format_callback(web_log_format_callback);
    web_server_set_auto_record_callback(web_auto_record_callback);
    web_server_set_log_directory_callback(web_set_log_directory_callback);
    web_server_set_get_log_directory_callback(web_get_log_directory_callback);
    printf("[OK] Web callbacks registered\n");

    // Initialize data logger
    data_logger_init(&g_data_logger);

    // Auto-create logs directory
    #ifdef _WIN32
        _mkdir(DEFAULT_LOG_DIR);
    #else
        mkdir(DEFAULT_LOG_DIR, 0755);
    #endif
    data_logger_set_directory(&g_data_logger, DEFAULT_LOG_DIR);
    printf("[*] Log directory set to: %s\n", DEFAULT_LOG_DIR);

    // Allocate signal buffers
    printf("[*] Allocating signal buffers (%d samples)...\n", FFT_SIZE);
    float* signal_buffer = (float*)malloc(FFT_SIZE * sizeof(float));
    float* magnitude_buffer = (float*)malloc((FFT_SIZE / 2) * sizeof(float));
    float* psd_buffer = (float*)malloc(128 * sizeof(float));
    float* band_energies = (float*)malloc(NUM_BANDS * sizeof(float));

    if (!signal_buffer || !magnitude_buffer || !psd_buffer || !band_energies) {
        fprintf(stderr, "[ERROR] Failed to allocate signal buffers\n");
        ret = 1;
        goto cleanup;
    }
    printf("[OK] Signal buffers allocated\n");

    // Allocate DSP contexts (pre-allocated, reused every frame)
    printf("[*] Allocating DSP contexts...\n");
    fft_context_t* fft_ctx = fft_context_create(FFT_SIZE);
    psd_context_t* psd_ctx = psd_context_create(PSD_SEGMENT_SIZE);  // Welch segment size

    if (!fft_ctx || !psd_ctx) {
        fprintf(stderr, "[ERROR] Failed to create DSP contexts\n");
        ret = 1;
        goto cleanup;
    }
    printf("[OK] DSP contexts allocated (eliminates per-frame allocation)\n\n");

    // Initialize ring buffer and start network thread if using network
    if (use_network) {
        printf("[*] Initializing ring buffer for async network reception...\n");
        if (!ring_buffer_init(&g_ring_buffer, RING_BUFFER_SIZE)) {
            fprintf(stderr, "[ERROR] Failed to initialize ring buffer\n");
            ret = 1;
            goto cleanup;
        }

        // Start background network receiver thread
        if (g_conn_manager.state == CONN_STATE_CONNECTED) {
            if (!start_network_thread(&g_network_config)) {
                fprintf(stderr, "[ERROR] Failed to start network receiver thread\n");
                ret = 1;
                goto cleanup;
            }
        }
    }

    printf("Controls:\n");
    printf("  Web GUI  - http://localhost:%d\n", web_port);
    printf("  Ctrl+C   - Exit\n\n");

    if (use_network) {
        printf("[*] Reading signal data from %s:%d (%s)\n",
               g_network_config.host, g_network_config.port,
               network_get_protocol_name(g_network_config.protocol));
    }

    printf("[OK] Ready!\n");

    // Display access information
    printf("\n");
    printf("========================================\n");
    printf("  WEB INTERFACE READY\n");
    printf("========================================\n");
    printf("  Open your browser to:\n");
    printf("  http://localhost:%d\n", web_port);
    printf("========================================\n");
    printf("\n");

    // Main processing loop
    waveform_mode_t current_mode = MODE_NETWORK_INPUT;
    uint8_t led_pattern = 0;

    while (g_running) {
        // Handle mode change requests
        if (g_requested_mode >= 0 && g_requested_mode <= MODE_SIGNAL_NOISE) {
            waveform_mode_t new_mode = (waveform_mode_t)g_requested_mode;
            if (new_mode != current_mode) {
                current_mode = new_mode;
                printf("[*] Mode changed to: %s\n", MODE_NAMES[current_mode]);
            }
        }

        if (!g_paused) {
            // Get signal data
            if (current_mode == MODE_NETWORK_INPUT && use_network) {
                // Check connection state
                if (g_conn_manager.state == CONN_STATE_CONNECTED) {
                    // Auto sample rate detection (updates decimation factor)
                    if (AUTO_DETECT_SAMPLE_RATE) {
                        update_sample_rate_detection();
                    }

                    // When NOT recording, aggressively drain buffer to prevent overflow
                    // When recording, disk writer drains it
                    if (!g_recording_raw_iq) {
                        // Drain multiple frames worth to keep buffer empty
                        // At 10 MHz with 100 Hz updates: 100K samples arrive per cycle
                        // Need drain buffer >= 100K to prevent overflow
                        static float drain_buffer[NETWORK_BUFFER_SIZE * 8];  // 8x network buffer (128K samples)
                        int drained = ring_buffer_read(&g_ring_buffer, drain_buffer, NETWORK_BUFFER_SIZE * DRAIN_BUFFER_MULTIPLIER);

                        // Use first FFT_SIZE for display
                        if (drained >= FFT_SIZE) {
                            memcpy(signal_buffer, drain_buffer, FFT_SIZE * sizeof(float));
                        } else {
                            // Not enough data
                            static int underrun_count = 0;
                            if (drained == 0 && ++underrun_count % 100 == 0) {
                                fprintf(stderr, "[WARN] Ring buffer underrun (%d times) - network may be slow\n",
                                        underrun_count);
                            }
                            generate_silence(signal_buffer, FFT_SIZE);
                        }
                    } else {
                        // When recording, just read one frame for display
                        // Disk writer handles draining aggressively
                        int samples_read = ring_buffer_read(&g_ring_buffer, signal_buffer, FFT_SIZE);

                        if (samples_read < FFT_SIZE) {
                            static int underrun_count = 0;
                            if (samples_read == 0 && ++underrun_count % 100 == 0) {
                                fprintf(stderr, "[WARN] Ring buffer underrun (%d times) - network may be slow\n",
                                        underrun_count);
                            }
                            generate_silence(signal_buffer, FFT_SIZE);
                        }
                    }

                    // Apply decimation for display
                    bool should_update_display = (g_decim_counter == 0);
                    g_decim_counter = (g_decim_counter + 1) % g_decimation_factor;

                    if (!should_update_display) {
                        // Skip FFT computation and display update
                        usleep(UPDATE_RATE_MS * MS_TO_MICROSECONDS);
                        continue;
                    }
                    // If should_update_display is true, fall through to compute FFT
                } else if (g_conn_manager.state == CONN_STATE_DISCONNECTED) {
                    // Not connected, try to reconnect periodically
                    if (g_conn_manager.auto_reconnect && attempt_reconnect(&g_network_config)) {
                        // Successfully reconnected, start network thread
                        if (!start_network_thread(&g_network_config)) {
                            fprintf(stderr, "[ERROR] Failed to start network receiver thread\n");
                        }
                        // Use silence this frame while thread starts up
                        generate_silence(signal_buffer, FFT_SIZE);
                    } else {
                        // Still disconnected, use silence
                        generate_silence(signal_buffer, FFT_SIZE);
                    }
                }
            } else {
                // Generate test waveform when network not requested or in other modes
                switch (current_mode) {
                    case MODE_NETWORK_INPUT:
                        // In test mode (not using network), use silence
                        generate_silence(signal_buffer, FFT_SIZE);
                        break;
                    case MODE_SILENCE:
                        generate_silence(signal_buffer, FFT_SIZE);
                        break;
                    case MODE_SIGNAL_NOISE:
                        generate_signal_noise(signal_buffer, FFT_SIZE);
                        break;
                    default:
                        generate_silence(signal_buffer, FFT_SIZE);
                        break;
                }
            }

            // Compute FFT (using pre-allocated context - no malloc/free)
            compute_fft_with_context(fft_ctx, signal_buffer, magnitude_buffer);

            // Compute PSD (using pre-allocated context - no malloc/free)
            // Use detected sample rate if available, otherwise use default
            uint32_t effective_rate = (g_detected_sample_rate > 0) ?
                                      (g_detected_sample_rate / g_decimation_factor) :
                                      SAMPLE_RATE;
            compute_psd_welch_with_context(psd_ctx, signal_buffer, psd_buffer, FFT_SIZE, effective_rate);

            // Calculate band energies
            for (int band = 0; band < NUM_BANDS; band++) {
                band_energies[band] = compute_band_energy(magnitude_buffer, FFT_SIZE,
                                                     BAND_EDGES[band],
                                                     BAND_EDGES[band + 1],
                                                     effective_rate);
            }
        }

        // Update web interface (throttled to 10 Hz for smooth browser performance)
        // Main loop runs at 200 Hz for fast ring buffer draining, but web only needs 10 Hz
        static int web_update_counter = 0;
        const int WEB_UPDATE_DIVISOR = 20;  // 200/20 = 10 Hz web updates
        uint64_t current_timestamp = (uint64_t)time(NULL) * 1000;

        if (g_web_server_fd >= 0 && (++web_update_counter % WEB_UPDATE_DIVISOR == 0)) {
            fft_data_t web_data = {
                .fft_size = FFT_SIZE,
                .sample_rate = SAMPLE_RATE,
                .num_bands = NUM_BANDS,
                .psd_size = 128,
                .time_domain = signal_buffer,
                .magnitude = magnitude_buffer,
                .psd = psd_buffer,
                .band_energies = band_energies,
                .led_pattern = led_pattern,
                .mode_name = MODE_NAMES[current_mode],
                .paused = g_paused,
                .web_control_active = true,  // Always true (no hardware switches)
                .timestamp = current_timestamp
            };

            web_server_update_data(&web_data);
        }

        // Handle web requests every cycle (but data only updates at 30 Hz)
        if (g_web_server_fd >= 0) {
            web_server_handle_requests(g_web_server_fd);

            // Calculate SNR for auto-record triggering
            float current_snr = data_logger_calculate_snr(magnitude_buffer, FFT_SIZE, SAMPLE_RATE);

            // Check auto-record trigger
            data_logger_check_auto_trigger(&g_data_logger, current_snr, FFT_SIZE, SAMPLE_RATE);

            // Log data if logging is active
            if (data_logger_is_active(&g_data_logger)) {
                data_logger_write_frame(&g_data_logger,
                                       signal_buffer,
                                       magnitude_buffer,
                                       psd_buffer,
                                       current_timestamp);
            }
        }

        usleep(UPDATE_RATE_MS * MS_TO_MICROSECONDS);
    }

cleanup:
    printf("\n[*] Cleaning up...\n");

    // Stop disk writer thread first (if raw IQ recording)
    if (g_recording_raw_iq) {
        stop_disk_writer_thread();
        g_recording_raw_iq = false;
    }

    // Stop network receiver thread
    if (use_network) {
        stop_network_thread();
        ring_buffer_destroy(&g_ring_buffer);
    }

    // Stop logging if active
    if (data_logger_is_active(&g_data_logger)) {
        data_logger_stop(&g_data_logger);
    }

    if (use_network) {
        network_close(&g_network_config);
    }

    if (g_web_server_fd >= 0) {
        web_server_cleanup(g_web_server_fd);
    }

    // Free DSP contexts
    fft_context_destroy(fft_ctx);
    psd_context_destroy(psd_ctx);

    // Free signal buffers
    free(signal_buffer);
    free(magnitude_buffer);
    free(psd_buffer);
    free(band_energies);

    network_cleanup();

    printf("[OK] Shutdown complete\n");
    return ret;
}
