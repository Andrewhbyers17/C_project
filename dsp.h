/*
 * dsp.h
 *
 * Digital Signal Processing Module
 * FFT, PSD (Welch's method), and band energy calculations
 */

#ifndef DSP_H
#define DSP_H

#include <stdint.h>
#include <stdbool.h>
#include "kiss_fft.h"

/*===========================================================================
 * DSP Context Structures
 *===========================================================================*/

// FFT context - pre-allocated buffers for FFT operations
typedef struct {
    kiss_fft_cfg cfg;
    kiss_fft_cpx* fft_in;
    kiss_fft_cpx* fft_out;
    int size;
} fft_context_t;

// PSD context - includes FFT context for Welch's method
typedef struct {
    float* segment;
    float* segment_psd;
    float* accumulated_psd;
    int segment_size;
    int num_bins;
    fft_context_t* fft_ctx;
} psd_context_t;

/*===========================================================================
 * Context Management
 *===========================================================================*/

// Create and destroy FFT context
fft_context_t* fft_context_create(int fft_size);
void fft_context_destroy(fft_context_t* ctx);

// Create and destroy PSD context
psd_context_t* psd_context_create(int segment_size);
void psd_context_destroy(psd_context_t* ctx);

/*===========================================================================
 * DSP Operations (Context-based - zero allocation per call)
 *===========================================================================*/

// Compute FFT with pre-allocated context
// input: time-domain signal (size = ctx->size)
// magnitude: output magnitude spectrum (size = ctx->size / 2)
void compute_fft_with_context(fft_context_t* ctx, const float* input, float* magnitude);

// Compute PSD using Welch's method with pre-allocated context
// signal: time-domain signal (size = fft_size)
// psd: output PSD in dB (size = ctx->num_bins, typically 128 bins)
// fft_size: size of input signal
// sample_rate: sampling rate in Hz
void compute_psd_welch_with_context(psd_context_t* ctx, const float* signal,
                                    float* psd, int fft_size, int sample_rate);

// Compute energy in a frequency band
// magnitude: FFT magnitude spectrum
// size: FFT size
// freq_low, freq_high: frequency band in Hz
// sample_rate: sampling rate in Hz (needed for bin calculation)
// Returns: RMS energy in the specified band
float compute_band_energy(const float* magnitude, int size,
                         float freq_low, float freq_high, int sample_rate);

/*===========================================================================
 * Legacy API (allocates on each call - use context-based API instead)
 *===========================================================================*/

// Legacy FFT computation (allocates context internally)
void compute_fft(const float* input, float* magnitude, int size);

// Legacy PSD computation (allocates context internally)
void compute_psd_welch(const float* signal, float* psd, int fft_size,
                       int sample_rate, int segment_size);

#endif // DSP_H
