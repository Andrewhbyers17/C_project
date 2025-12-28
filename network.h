/*
 * network.h
 *
 * Network connection management and data streaming
 * Supports TCP/UDP protocols with auto-reconnection
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Platform-specific includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

/*===========================================================================
 * Network Protocol and Configuration
 *===========================================================================*/

#define MAX_HOSTNAME_LENGTH 256

typedef enum {
    NET_PROTOCOL_TCP,
    NET_PROTOCOL_UDP
} network_protocol_t;

typedef struct {
    char host[MAX_HOSTNAME_LENGTH];
    int port;
    network_protocol_t protocol;
    int socket_fd;
} network_config_t;

/*===========================================================================
 * Connection State Management
 *===========================================================================*/

typedef enum {
    CONN_STATE_DISCONNECTED,
    CONN_STATE_CONNECTING,
    CONN_STATE_CONNECTED,
    CONN_STATE_RECONNECTING
} connection_state_t;

typedef struct {
    connection_state_t state;
    int retry_count;
    int max_retries;
    int retry_delay_ms;
    time_t last_retry_time;
    bool auto_reconnect;
} connection_manager_t;

/*===========================================================================
 * Network Initialization
 *===========================================================================*/

/**
 * Initialize Winsock (Windows only)
 * Returns: 0 on success, -1 on error
 */
int network_init(void);

/**
 * Cleanup Winsock (Windows only)
 */
void network_cleanup(void);

/*===========================================================================
 * Connection Management
 *===========================================================================*/

/**
 * Connect to network source
 * config: Network configuration (host, port, protocol)
 * Returns: socket fd on success, -1 on error
 */
int network_connect(network_config_t* config);

/**
 * Close network connection
 */
void network_close(network_config_t* config);

/**
 * Read samples from network
 * buffer: Output buffer for samples
 * num_samples: Number of float samples to read
 * Returns: Number of samples read, or -1 on error
 */
int network_read_samples(network_config_t* config, float* buffer, int num_samples);

/**
 * Get protocol name string
 */
const char* network_get_protocol_name(network_protocol_t protocol);

#endif // NETWORK_H
