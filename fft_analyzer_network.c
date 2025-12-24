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

#include "kiss_fft.h"
#include "web_server.h"
#include "data_logger.h"

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

#define FFT_SIZE            512
#define SAMPLE_RATE         8000
#define NUM_BANDS           8
#define UPDATE_RATE_MS      50
#define DEFAULT_LOG_DIR     "logs"

static const float BAND_EDGES[NUM_BANDS + 1] = {
    0, 200, 400, 600, 800, 1200, 1600, 2400, 4000
};

#define LED_THRESHOLD_LOW   0.01f

/*===========================================================================
 * Network Configuration
 *===========================================================================*/

typedef enum {
    NET_PROTOCOL_TCP,
    NET_PROTOCOL_UDP
} network_protocol_t;

typedef struct {
    char host[256];
    int port;
    network_protocol_t protocol;
    int socket_fd;
} network_config_t;

/*===========================================================================
 * Waveform Modes (for testing without network)
 *===========================================================================*/

typedef enum {
    MODE_NETWORK_INPUT = 0,
    MODE_440HZ, MODE_1KHZ, MODE_2KHZ,
    MODE_MIXED, MODE_SWEEP, MODE_NOISE, MODE_IMPULSE, MODE_LFM,
    MODE_SINC, MODE_IQ_LFM, MODE_SIGNAL_NOISE
} waveform_mode_t;

static const char* MODE_NAMES[] = {
    "Network Input", "440 Hz Sine", "1000 Hz Sine", "2000 Hz Sine",
    "Mixed Tones", "Frequency Sweep", "White Noise", "Impulse Train", "LFM Chirp",
    "Sinc Function", "IQ LFM Chirp", "Signal + Noise"
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

/*===========================================================================
 * Signal Handler
 *===========================================================================*/

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\nShutting down...\n");
        g_running = false;
    }
}

/*===========================================================================
 * Network Functions
 *===========================================================================*/

#ifdef _WIN32
int init_winsock(void) {
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        fprintf(stderr, "[ERROR] WSAStartup failed: %d\n", result);
        return -1;
    }
    return 0;
}

void cleanup_winsock(void) {
    WSACleanup();
}
#else
int init_winsock(void) { return 0; }
void cleanup_winsock(void) {}
#endif

int network_connect(network_config_t* config) {
    struct sockaddr_in server_addr;
    int sock_fd;

    // Create socket
    if (config->protocol == NET_PROTOCOL_TCP) {
        sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    } else {
        sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }

    if (sock_fd < 0) {
        perror("[ERROR] Failed to create socket");
        return -1;
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config->port);

    if (inet_pton(AF_INET, config->host, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "[ERROR] Invalid address: %s\n", config->host);
        closesocket(sock_fd);
        return -1;
    }

    // Connect (TCP only)
    if (config->protocol == NET_PROTOCOL_TCP) {
        printf("[*] Connecting to %s:%d (TCP)...\n", config->host, config->port);
        if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("[ERROR] Connection failed");
            closesocket(sock_fd);
            return -1;
        }
        printf("[OK] Connected to %s:%d\n", config->host, config->port);
    } else {
        printf("[OK] UDP socket ready for %s:%d\n", config->host, config->port);
    }

    config->socket_fd = sock_fd;
    return sock_fd;
}

int network_read_samples(network_config_t* config, float* buffer, int num_samples) {
    int bytes_needed = num_samples * sizeof(float);
    int bytes_read = 0;

    if (config->protocol == NET_PROTOCOL_TCP) {
        // TCP: Read until we have enough data
        while (bytes_read < bytes_needed) {
            int n = recv(config->socket_fd,
                        ((char*)buffer) + bytes_read,
                        bytes_needed - bytes_read, 0);
            if (n <= 0) {
                fprintf(stderr, "[ERROR] Connection lost or no data\n");
                return -1;
            }
            bytes_read += n;
        }
    } else {
        // UDP: Read one packet
        bytes_read = recvfrom(config->socket_fd, (char*)buffer, bytes_needed,
                             0, NULL, NULL);
        if (bytes_read < 0) {
            perror("[ERROR] UDP receive failed");
            return -1;
        }
    }

    return bytes_read / sizeof(float);
}

void network_close(network_config_t* config) {
    if (config->socket_fd >= 0) {
        closesocket(config->socket_fd);
        config->socket_fd = -1;
    }
}

/*===========================================================================
 * Waveform Generation (for testing)
 *===========================================================================*/

void generate_sine_wave(float* buffer, int size, float frequency, float amplitude) {
    for (int i = 0; i < size; i++) {
        float t = (float)i / SAMPLE_RATE;
        buffer[i] = amplitude * sinf(2.0f * M_PI * frequency * t);
    }
}

void generate_mixed_wave(float* buffer, int size) {
    for (int i = 0; i < size; i++) {
        float t = (float)i / SAMPLE_RATE;
        buffer[i] = 0.3f * sinf(2.0f * M_PI * 440.0f * t) +
                    0.2f * sinf(2.0f * M_PI * 880.0f * t) +
                    0.15f * sinf(2.0f * M_PI * 1320.0f * t);
    }
}

static float sweep_phase = 0.0f;
static float sweep_freq = 100.0f;

void generate_sweep(float* buffer, int size) {
    for (int i = 0; i < size; i++) {
        buffer[i] = 0.5f * sinf(sweep_phase);
        sweep_phase += 2.0f * M_PI * sweep_freq / SAMPLE_RATE;

        sweep_freq += 2.0f;
        if (sweep_freq > 3000.0f) {
            sweep_freq = 100.0f;
        }
    }
}

void generate_noise(float* buffer, int size) {
    for (int i = 0; i < size; i++) {
        buffer[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
    }
}

void generate_impulse(float* buffer, int size) {
    memset(buffer, 0, size * sizeof(float));
    for (int i = 0; i < size; i += SAMPLE_RATE / 100) {
        if (i < size) {
            buffer[i] = 1.0f;
        }
    }
}

void generate_lfm(float* buffer, int size) {
    // Linear Frequency Modulation (Chirp)
    static float lfm_phase = 0.0f;
    static float lfm_freq = 500.0f;

    float f0 = 500.0f;      // Start frequency (Hz)
    float f1 = 2500.0f;     // End frequency (Hz)
    float sweep_time = 2.0f;  // Sweep duration in seconds
    float freq_step = (f1 - f0) / (sweep_time * SAMPLE_RATE);

    for (int i = 0; i < size; i++) {
        buffer[i] = 0.8f * sinf(lfm_phase);

        lfm_phase += 2.0f * M_PI * lfm_freq / SAMPLE_RATE;
        lfm_freq += freq_step;

        if (lfm_freq > f1) {
            lfm_freq = f0;
        }

        if (lfm_phase > 2.0f * M_PI) {
            lfm_phase -= 2.0f * M_PI;
        }
    }
}

void generate_sinc(float* buffer, int size) {
    // Sinc function: sinc(x) = sin(πx) / (πx)
    float fc = 1000.0f;  // Cutoff frequency (Hz)
    int center = size / 2;

    for (int i = 0; i < size; i++) {
        float t = (float)(i - center) / SAMPLE_RATE;
        float x = 2.0f * M_PI * fc * t;

        if (fabsf(x) < 1e-6f) {
            buffer[i] = 0.8f;
        } else {
            buffer[i] = 0.8f * sinf(x) / x;
        }
    }
}

void generate_iq_lfm(float* buffer, int size) {
    // IQ (complex) LFM chirp with symmetric spectrum
    static float lfm_phase = 0.0f;
    static float lfm_freq = 500.0f;

    float f0 = 500.0f;      // Start frequency (Hz)
    float f1 = 2500.0f;     // End frequency (Hz)
    float fc = 1500.0f;     // Center frequency (Hz)
    float sweep_time = 2.0f;
    float freq_step = (f1 - f0) / (sweep_time * SAMPLE_RATE);

    for (int i = 0; i < size; i++) {
        float phase_upper = 2.0f * M_PI * lfm_freq / SAMPLE_RATE;

        buffer[i] = 0.4f * (sinf(lfm_phase) + sinf(-lfm_phase + 2.0f * M_PI * fc * i / SAMPLE_RATE));

        lfm_phase += phase_upper;
        lfm_freq += freq_step;

        if (lfm_freq > f1) {
            lfm_freq = f0;
        }

        if (lfm_phase > 2.0f * M_PI) {
            lfm_phase -= 2.0f * M_PI;
        }
    }
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

/*===========================================================================
 * DSP Functions (from your existing code)
 *===========================================================================*/

void compute_fft(const float* input, float* magnitude, int size) {
    kiss_fft_cfg cfg = kiss_fft_alloc(size, 0, NULL, NULL);
    kiss_fft_cpx* fft_in = (kiss_fft_cpx*)malloc(size * sizeof(kiss_fft_cpx));
    kiss_fft_cpx* fft_out = (kiss_fft_cpx*)malloc(size * sizeof(kiss_fft_cpx));

    for (int i = 0; i < size; i++) {
        fft_in[i].r = input[i];
        fft_in[i].i = 0.0f;
    }

    kiss_fft(cfg, fft_in, fft_out);

    for (int i = 0; i < size / 2; i++) {
        magnitude[i] = sqrtf(fft_out[i].r * fft_out[i].r +
                            fft_out[i].i * fft_out[i].i);
    }

    free(fft_in);
    free(fft_out);
    kiss_fft_free(cfg);
}

void compute_psd_welch(const float* signal, float* psd, int fft_size, int sample_rate) {
    const int segment_size = 256;
    const int overlap = segment_size / 2;
    const int num_bins = segment_size / 2;

    float* segment = (float*)malloc(segment_size * sizeof(float));
    float* segment_psd = (float*)malloc(num_bins * sizeof(float));
    float* accumulated_psd = (float*)calloc(num_bins, sizeof(float));

    int num_segments = 0;

    for (int start = 0; start <= fft_size - segment_size; start += overlap) {
        memcpy(segment, signal + start, segment_size * sizeof(float));
        compute_fft(segment, segment_psd, segment_size);

        for (int i = 0; i < num_bins; i++) {
            accumulated_psd[i] += segment_psd[i] * segment_psd[i];
        }
        num_segments++;
    }

    // Average, normalize, and convert to dB
    for (int i = 0; i < num_bins; i++) {
        float power = accumulated_psd[i] / num_segments;
        // Normalize by segment size to get proper PSD
        power = power / (segment_size * segment_size);
        // Convert to dB relative to reference (1.0)
        psd[i] = 10.0f * log10f(power + 1e-10f);
    }

    free(segment);
    free(segment_psd);
    free(accumulated_psd);
}

float get_band_energy(const float* magnitude, int size, float freq_low, float freq_high) {
    int bin_low = (int)((freq_low * size) / SAMPLE_RATE);
    int bin_high = (int)((freq_high * size) / SAMPLE_RATE);

    if (bin_high >= size / 2) bin_high = size / 2 - 1;
    if (bin_low < 0) bin_low = 0;

    float energy = 0.0f;
    for (int i = bin_low; i <= bin_high; i++) {
        energy += magnitude[i] * magnitude[i];
    }

    return sqrtf(energy / (bin_high - bin_low + 1));
}

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
    printf("  --port PORT         Web server port (default: 8080)\n");
    printf("  --no-browser        Don't auto-open web browser\n");
    printf("  --help              Show this help\n\n");
    printf("Examples:\n");
    printf("  %s --source 192.168.1.100:5000 --protocol tcp\n", prog_name);
    printf("  %s --test  (use built-in test signals)\n\n", prog_name);
}

int main(int argc, char* argv[]) {
    int ret = 0;
    bool use_network = false;
    int web_port = 8080;
    bool auto_open_browser = true;  // Auto-open browser by default

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
        } else if (strcmp(argv[i], "--no-browser") == 0) {
            auto_open_browser = false;
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
    if (init_winsock() < 0) {
        return 1;
    }

    // Connect to network source if requested
    if (use_network) {
        if (network_connect(&g_network_config) < 0) {
            fprintf(stderr, "[ERROR] Failed to connect to network source\n");
            cleanup_winsock();
            return 1;
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

    // Allocate buffers
    printf("[*] Allocating FFT buffers (%d samples)...\n", FFT_SIZE);
    float* signal_buffer = (float*)malloc(FFT_SIZE * sizeof(float));
    float* magnitude_buffer = (float*)malloc((FFT_SIZE / 2) * sizeof(float));
    float* psd_buffer = (float*)malloc(128 * sizeof(float));
    float* band_energies = (float*)malloc(NUM_BANDS * sizeof(float));

    if (!signal_buffer || !magnitude_buffer || !psd_buffer || !band_energies) {
        fprintf(stderr, "[ERROR] Failed to allocate buffers\n");
        ret = 1;
        goto cleanup;
    }
    printf("[OK] Buffers allocated\n\n");

    printf("Controls:\n");
    printf("  Web GUI  - http://localhost:%d\n", web_port);
    printf("  Ctrl+C   - Exit\n\n");

    if (use_network) {
        printf("[*] Reading signal data from %s:%d (%s)\n",
               g_network_config.host, g_network_config.port,
               g_network_config.protocol == NET_PROTOCOL_TCP ? "TCP" : "UDP");
    }

    printf("[OK] Ready!\n");

    // Auto-open browser (if enabled)
    if (auto_open_browser) {
        printf("[*] Opening browser to http://localhost:%d ...\n", web_port);
        char browser_cmd[256];
#ifdef _WIN32
        snprintf(browser_cmd, sizeof(browser_cmd), "start http://localhost:%d", web_port);
#elif __APPLE__
        snprintf(browser_cmd, sizeof(browser_cmd), "open http://localhost:%d", web_port);
#else
        snprintf(browser_cmd, sizeof(browser_cmd), "xdg-open http://localhost:%d", web_port);
#endif
        system(browser_cmd);
    }
    printf("\n");

    // Main processing loop
    waveform_mode_t current_mode = MODE_NETWORK_INPUT;
    uint8_t led_pattern = 0;

    while (g_running) {
        // Handle mode change requests
        if (g_requested_mode >= 0 && g_requested_mode < 12) {
            waveform_mode_t new_mode = (waveform_mode_t)g_requested_mode;
            if (new_mode != current_mode) {
                current_mode = new_mode;
                printf("[*] Mode changed to: %s\n", MODE_NAMES[current_mode]);
            }
        }

        if (!g_paused) {
            // Get signal data
            if (current_mode == MODE_NETWORK_INPUT && use_network) {
                // Read from network
                int samples_read = network_read_samples(&g_network_config,
                                                       signal_buffer, FFT_SIZE);
                if (samples_read < 0) {
                    fprintf(stderr, "[ERROR] Network read failed, switching to test mode\n");
                    current_mode = MODE_440HZ;
                }
            } else {
                // Generate test waveform when network not available
                switch (current_mode) {
                    case MODE_NETWORK_INPUT:
                        // In test mode, default to 440Hz instead of silence
                        generate_sine_wave(signal_buffer, FFT_SIZE, 440.0f, 0.8f);
                        break;
                    case MODE_440HZ:
                        generate_sine_wave(signal_buffer, FFT_SIZE, 440.0f, 0.8f);
                        break;
                    case MODE_1KHZ:
                        generate_sine_wave(signal_buffer, FFT_SIZE, 1000.0f, 0.8f);
                        break;
                    case MODE_2KHZ:
                        generate_sine_wave(signal_buffer, FFT_SIZE, 2000.0f, 0.8f);
                        break;
                    case MODE_MIXED:
                        generate_mixed_wave(signal_buffer, FFT_SIZE);
                        break;
                    case MODE_SWEEP:
                        generate_sweep(signal_buffer, FFT_SIZE);
                        break;
                    case MODE_NOISE:
                        generate_noise(signal_buffer, FFT_SIZE);
                        break;
                    case MODE_IMPULSE:
                        generate_impulse(signal_buffer, FFT_SIZE);
                        break;
                    case MODE_LFM:
                        generate_lfm(signal_buffer, FFT_SIZE);
                        break;
                    case MODE_SINC:
                        generate_sinc(signal_buffer, FFT_SIZE);
                        break;
                    case MODE_IQ_LFM:
                        generate_iq_lfm(signal_buffer, FFT_SIZE);
                        break;
                    case MODE_SIGNAL_NOISE:
                        generate_signal_noise(signal_buffer, FFT_SIZE);
                        break;
                    default:
                        generate_sine_wave(signal_buffer, FFT_SIZE, 440.0f, 0.8f);
                        break;
                }
            }

            // Compute FFT
            compute_fft(signal_buffer, magnitude_buffer, FFT_SIZE);

            // Compute PSD
            compute_psd_welch(signal_buffer, psd_buffer, FFT_SIZE, SAMPLE_RATE);

            // Calculate band energies
            for (int band = 0; band < NUM_BANDS; band++) {
                band_energies[band] = get_band_energy(magnitude_buffer, FFT_SIZE,
                                                     BAND_EDGES[band],
                                                     BAND_EDGES[band + 1]);
            }
        }

        // Update web interface (ALWAYS, even when paused)
        if (g_web_server_fd >= 0) {
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
                .timestamp = (uint64_t)time(NULL) * 1000
            };

            web_server_update_data(&web_data);
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
                                       web_data.timestamp);
            }
        }

        usleep(UPDATE_RATE_MS * 1000);
    }

cleanup:
    printf("\n[*] Cleaning up...\n");

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

    free(signal_buffer);
    free(magnitude_buffer);
    free(psd_buffer);
    free(band_energies);

    cleanup_winsock();

    printf("[OK] Shutdown complete\n");
    return ret;
}
