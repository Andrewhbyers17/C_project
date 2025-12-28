/*
 * network.c
 *
 * Network Connection Management Implementation
 */

#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #define closesocket closesocket
#else
    #include <unistd.h>
    #define closesocket close
#endif

/*===========================================================================
 * Network Initialization
 *===========================================================================*/

int network_init(void) {
#ifdef _WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        fprintf(stderr, "[ERROR] WSAStartup failed: %d\n", result);
        return -1;
    }
#endif
    return 0;
}

void network_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

/*===========================================================================
 * Connection Management
 *===========================================================================*/

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

void network_close(network_config_t* config) {
    if (config->socket_fd >= 0) {
        closesocket(config->socket_fd);
        config->socket_fd = -1;
    }
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

const char* network_get_protocol_name(network_protocol_t protocol) {
    return (protocol == NET_PROTOCOL_TCP) ? "TCP" : "UDP";
}
