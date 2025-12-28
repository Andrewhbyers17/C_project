/*
 * ring_buffer.c
 *
 * Thread-safe Ring Buffer Implementation
 */

#include "ring_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Conversion constants
#define BYTES_PER_MB (1024.0 * 1024.0)

/*===========================================================================
 * Ring Buffer Implementation
 *===========================================================================*/

bool ring_buffer_init(ring_buffer_t* rb, int capacity) {
    rb->data = (float*)malloc(capacity * sizeof(float));
    if (!rb->data) {
        fprintf(stderr, "[ERROR] Failed to allocate ring buffer (%d samples, %.2f MB)\n",
                capacity, (capacity * sizeof(float)) / BYTES_PER_MB);
        return false;
    }

    rb->write_pos = 0;
    rb->read_pos = 0;
    rb->available_samples = 0;
    rb->overflow = false;
    rb->capacity = capacity;

#ifdef _WIN32
    InitializeCriticalSection(&rb->lock);
#else
    pthread_mutex_init(&rb->lock, NULL);
#endif

    printf("[OK] Ring buffer allocated (%d samples, %.2f MB)\n",
           capacity, (capacity * sizeof(float)) / BYTES_PER_MB);

    return true;
}

void ring_buffer_destroy(ring_buffer_t* rb) {
    if (rb->data) {
        free(rb->data);
        rb->data = NULL;
    }

#ifdef _WIN32
    DeleteCriticalSection(&rb->lock);
#else
    pthread_mutex_destroy(&rb->lock);
#endif
}

int ring_buffer_write(ring_buffer_t* rb, const float* samples, int count) {
#ifdef _WIN32
    EnterCriticalSection(&rb->lock);
#else
    pthread_mutex_lock(&rb->lock);
#endif

    int space_available = rb->capacity - rb->available_samples;
    int to_write = (count < space_available) ? count : space_available;

    if (to_write < count) {
        rb->overflow = true;
        fprintf(stderr, "[WARN] Ring buffer overflow! Dropping %d samples\n", count - to_write);
    }

    // Batch copy: handle wrap-around with at most 2 memcpy calls
    int space_to_end = rb->capacity - rb->write_pos;

    if (to_write <= space_to_end) {
        // No wrap-around: single memcpy
        memcpy(&rb->data[rb->write_pos], samples, to_write * sizeof(float));
    } else {
        // Wrap-around: two memcpy calls
        // First: fill to end of buffer
        memcpy(&rb->data[rb->write_pos], samples, space_to_end * sizeof(float));
        // Second: wrap to beginning
        int remaining = to_write - space_to_end;
        memcpy(&rb->data[0], &samples[space_to_end], remaining * sizeof(float));
    }

    // Update write position (single modulo operation)
    rb->write_pos = (rb->write_pos + to_write) % rb->capacity;
    rb->available_samples += to_write;

#ifdef _WIN32
    LeaveCriticalSection(&rb->lock);
#else
    pthread_mutex_unlock(&rb->lock);
#endif

    return to_write;
}

int ring_buffer_read(ring_buffer_t* rb, float* samples, int count) {
#ifdef _WIN32
    EnterCriticalSection(&rb->lock);
#else
    pthread_mutex_lock(&rb->lock);
#endif

    int to_read = (count < rb->available_samples) ? count : rb->available_samples;

    // Batch copy: handle wrap-around with at most 2 memcpy calls
    int space_to_end = rb->capacity - rb->read_pos;

    if (to_read <= space_to_end) {
        // No wrap-around: single memcpy
        memcpy(samples, &rb->data[rb->read_pos], to_read * sizeof(float));
    } else {
        // Wrap-around: two memcpy calls
        memcpy(samples, &rb->data[rb->read_pos], space_to_end * sizeof(float));
        int remaining = to_read - space_to_end;
        memcpy(&samples[space_to_end], &rb->data[0], remaining * sizeof(float));
    }

    // Update read position (single modulo operation)
    rb->read_pos = (rb->read_pos + to_read) % rb->capacity;
    rb->available_samples -= to_read;

#ifdef _WIN32
    LeaveCriticalSection(&rb->lock);
#else
    pthread_mutex_unlock(&rb->lock);
#endif

    return to_read;
}
