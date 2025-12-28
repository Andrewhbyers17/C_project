/*
 * ring_buffer.h
 *
 * Thread-safe ring buffer for high-speed data streaming
 * Optimized with batched memcpy operations
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
#else
    #include <pthread.h>
#endif

/*===========================================================================
 * Ring Buffer Structure
 *===========================================================================*/

typedef struct {
    float* data;
    volatile int write_pos;
    volatile int read_pos;
    volatile int available_samples;
    volatile bool overflow;
    int capacity;  // Total buffer size in samples
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
} ring_buffer_t;

/*===========================================================================
 * Ring Buffer Functions
 *===========================================================================*/

/**
 * Initialize ring buffer with specified capacity
 * capacity: Number of float samples the buffer can hold
 * Returns: true on success, false on allocation failure
 */
bool ring_buffer_init(ring_buffer_t* rb, int capacity);

/**
 * Destroy ring buffer and free resources
 */
void ring_buffer_destroy(ring_buffer_t* rb);

/**
 * Write samples to ring buffer (producer)
 * Thread-safe with batched memcpy operations
 * Returns: Number of samples actually written
 */
int ring_buffer_write(ring_buffer_t* rb, const float* samples, int count);

/**
 * Read samples from ring buffer (consumer)
 * Thread-safe with batched memcpy operations
 * Returns: Number of samples actually read
 */
int ring_buffer_read(ring_buffer_t* rb, float* samples, int count);

/**
 * Get number of samples available for reading
 */
static inline int ring_buffer_available(const ring_buffer_t* rb) {
    return rb->available_samples;
}

/**
 * Get number of samples that can be written
 */
static inline int ring_buffer_space(const ring_buffer_t* rb) {
    return rb->capacity - rb->available_samples;
}

/**
 * Check and clear overflow flag
 * Returns: true if overflow occurred since last check
 */
static inline bool ring_buffer_check_overflow(ring_buffer_t* rb) {
    bool overflow = rb->overflow;
    rb->overflow = false;
    return overflow;
}

#endif // RING_BUFFER_H
