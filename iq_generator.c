/*
 * High-Speed IQ Data Generator
 * Generates noise + sine wave peak for testing FFT analyzer
 * Optimized for 20+ MHz sample rates
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define DEFAULT_SAMPLE_RATE 10000000  // 10 MHz
#define BUFFER_SIZE 65536             // 64K samples (256 KB)
#define DEFAULT_PORT 5000
#define SIGNAL_FREQ 1000.0            // 1 kHz peak
#define NOISE_AMPLITUDE 0.1           // Noise level
#define SIGNAL_AMPLITUDE 1.0          // Signal level (10:1 SNR)

// Fast random number generator (xorshift)
static uint32_t xorshift_state = 123456789;

static inline float fast_random(void) {
    xorshift_state ^= xorshift_state << 13;
    xorshift_state ^= xorshift_state >> 17;
    xorshift_state ^= xorshift_state << 5;
    return ((float)xorshift_state / (float)UINT32_MAX) * 2.0f - 1.0f;
}

// Generate I/Q samples: noise + sine wave
void generate_samples(float* buffer, int count, int sample_rate, double* phase) {
    double phase_inc = 2.0 * M_PI * SIGNAL_FREQ / sample_rate;

    for (int i = 0; i < count * 2; i += 2) {
        // Generate noise
        float noise_i = fast_random() * NOISE_AMPLITUDE;
        float noise_q = fast_random() * NOISE_AMPLITUDE;

        // Generate signal (sine wave in I, cosine in Q)
        float signal_i = SIGNAL_AMPLITUDE * cos(*phase);
        float signal_q = SIGNAL_AMPLITUDE * sin(*phase);

        // Combine signal + noise
        buffer[i]     = signal_i + noise_i;  // I
        buffer[i + 1] = signal_q + noise_q;  // Q

        *phase += phase_inc;
        if (*phase >= 2.0 * M_PI) {
            *phase -= 2.0 * M_PI;
        }
    }
}

// High-precision sleep using busy-wait for last millisecond
void precise_sleep_ms(double ms) {
    if (ms <= 0) return;

    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    double target_counts = (ms / 1000.0) * freq.QuadPart;

    // Coarse sleep if > 2ms
    if (ms > 2.0) {
        Sleep((DWORD)(ms - 1.0));
    }

    // Busy-wait for precision
    do {
        QueryPerformanceCounter(&end);
    } while ((end.QuadPart - start.QuadPart) < target_counts);
}

int main(int argc, char* argv[]) {
    int sample_rate = DEFAULT_SAMPLE_RATE;
    int port = DEFAULT_PORT;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rate") == 0 && i + 1 < argc) {
            sample_rate = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("High-Speed IQ Data Generator\n");
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --rate <Hz>   Sample rate (default: 10000000)\n");
            printf("  --port <num>  TCP port (default: 5000)\n");
            printf("  --help        Show this help\n");
            printf("\nSignal: %.0f Hz sine wave + noise (SNR ~20 dB)\n", SIGNAL_FREQ);
            return 0;
        }
    }

    // Initialize Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    // Create TCP socket
    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation failed\n");
        WSACleanup();
        return 1;
    }

    // Set socket options
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    // Set large send buffer (1 MB)
    int send_buffer_size = 1024 * 1024;
    setsockopt(server_sock, SOL_SOCKET, SO_SNDBUF, (char*)&send_buffer_size, sizeof(send_buffer_size));

    // Bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Bind failed on port %d\n", port);
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    // Listen
    if (listen(server_sock, 1) == SOCKET_ERROR) {
        fprintf(stderr, "Listen failed\n");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    printf("=========================================\n");
    printf("High-Speed IQ Data Generator\n");
    printf("=========================================\n");
    printf("Sample rate: %d Hz (%.2f MHz)\n", sample_rate, sample_rate / 1e6);
    printf("Signal: %.0f Hz sine wave\n", SIGNAL_FREQ);
    printf("Noise level: %.2f (SNR ~20 dB)\n", NOISE_AMPLITUDE);
    printf("Buffer size: %d samples (%.1f KB)\n", BUFFER_SIZE, BUFFER_SIZE * 2 * sizeof(float) / 1024.0);
    printf("Data rate: %.2f MB/sec\n", sample_rate * 2 * sizeof(float) / 1e6);
    printf("Listening on port %d...\n", port);
    printf("=========================================\n\n");

    // Accept connection
    SOCKET client_sock = accept(server_sock, NULL, NULL);
    if (client_sock == INVALID_SOCKET) {
        fprintf(stderr, "Accept failed\n");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    printf("[OK] Client connected\n");

    // Disable Nagle's algorithm for low latency
    int nodelay = 1;
    setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));

    // Allocate buffer
    float* buffer = (float*)malloc(BUFFER_SIZE * 2 * sizeof(float));
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        closesocket(client_sock);
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    // Initialize random seed
    xorshift_state = (uint32_t)time(NULL);

    // Calculate timing
    double samples_per_buffer = BUFFER_SIZE;
    double ms_per_buffer = (samples_per_buffer / sample_rate) * 1000.0;

    printf("[OK] Sending data at %.2f MHz (%.3f ms per buffer)\n",
           sample_rate / 1e6, ms_per_buffer);
    printf("[OK] Press Ctrl+C to stop\n\n");

    // Streaming loop
    uint64_t total_samples = 0;
    double phase = 0.0;

    LARGE_INTEGER freq, start_time;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start_time);

    while (1) {
        // Generate samples
        generate_samples(buffer, BUFFER_SIZE, sample_rate, &phase);

        // Send data
        int bytes_to_send = BUFFER_SIZE * 2 * sizeof(float);
        int bytes_sent = 0;

        while (bytes_sent < bytes_to_send) {
            int result = send(client_sock, (char*)buffer + bytes_sent,
                            bytes_to_send - bytes_sent, 0);
            if (result == SOCKET_ERROR) {
                printf("\n[ERROR] Client disconnected\n");
                goto cleanup;
            }
            bytes_sent += result;
        }

        total_samples += BUFFER_SIZE;

        // Print stats every second
        LARGE_INTEGER current_time;
        QueryPerformanceCounter(&current_time);
        double elapsed_sec = (double)(current_time.QuadPart - start_time.QuadPart) / freq.QuadPart;

        if (elapsed_sec >= 1.0) {
            double actual_rate = total_samples / elapsed_sec;
            double mb_sent = (total_samples * 2 * sizeof(float)) / (1024.0 * 1024.0);

            printf("\r[STATS] Sent: %.2f MB | Rate: %.2f MHz | Samples: %llu   ",
                   mb_sent, actual_rate / 1e6, total_samples);
            fflush(stdout);

            // Reset counters
            QueryPerformanceCounter(&start_time);
            total_samples = 0;
        }

        // Precise rate control
        precise_sleep_ms(ms_per_buffer * 0.95);  // Sleep 95% of time, let TCP buffer handle rest
    }

cleanup:
    free(buffer);
    closesocket(client_sock);
    closesocket(server_sock);
    WSACleanup();

    printf("\n[OK] Shutdown complete\n");
    return 0;
}
