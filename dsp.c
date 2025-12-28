/*
 * dsp.c
 *
 * Digital Signal Processing Module Implementation
 */

#include "dsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*===========================================================================
 * Context Management
 *===========================================================================*/

fft_context_t* fft_context_create(int fft_size) {
    fft_context_t* ctx = (fft_context_t*)malloc(sizeof(fft_context_t));
    if (!ctx) {
        fprintf(stderr, "[DSP ERROR] Failed to allocate FFT context\n");
        return NULL;
    }

    ctx->size = fft_size;
    ctx->cfg = kiss_fft_alloc(fft_size, 0, NULL, NULL);
    ctx->fft_in = (kiss_fft_cpx*)malloc(fft_size * sizeof(kiss_fft_cpx));
    ctx->fft_out = (kiss_fft_cpx*)malloc(fft_size * sizeof(kiss_fft_cpx));

    if (!ctx->cfg || !ctx->fft_in || !ctx->fft_out) {
        fprintf(stderr, "[DSP ERROR] Failed to allocate FFT buffers\n");
        fft_context_destroy(ctx);
        return NULL;
    }

    return ctx;
}

void fft_context_destroy(fft_context_t* ctx) {
    if (!ctx) return;

    if (ctx->cfg) kiss_fft_free(ctx->cfg);
    if (ctx->fft_in) free(ctx->fft_in);
    if (ctx->fft_out) free(ctx->fft_out);
    free(ctx);
}

psd_context_t* psd_context_create(int segment_size) {
    psd_context_t* ctx = (psd_context_t*)calloc(1, sizeof(psd_context_t));
    if (!ctx) {
        fprintf(stderr, "[DSP ERROR] Failed to allocate PSD context\n");
        return NULL;
    }

    ctx->segment_size = segment_size;
    ctx->num_bins = segment_size / 2;

    ctx->segment = (float*)malloc(segment_size * sizeof(float));
    ctx->segment_psd = (float*)malloc(ctx->num_bins * sizeof(float));
    ctx->accumulated_psd = (float*)calloc(ctx->num_bins, sizeof(float));
    ctx->fft_ctx = fft_context_create(segment_size);

    if (!ctx->segment || !ctx->segment_psd || !ctx->accumulated_psd || !ctx->fft_ctx) {
        fprintf(stderr, "[DSP ERROR] Failed to allocate PSD buffers\n");
        psd_context_destroy(ctx);
        return NULL;
    }

    return ctx;
}

void psd_context_destroy(psd_context_t* ctx) {
    if (!ctx) return;

    if (ctx->segment) free(ctx->segment);
    if (ctx->segment_psd) free(ctx->segment_psd);
    if (ctx->accumulated_psd) free(ctx->accumulated_psd);
    if (ctx->fft_ctx) fft_context_destroy(ctx->fft_ctx);
    free(ctx);
}

/*===========================================================================
 * DSP Operations (Context-based - zero allocation per call)
 *===========================================================================*/

void compute_fft_with_context(fft_context_t* ctx, const float* input, float* magnitude) {
    if (!ctx || ctx->size <= 0) {
        fprintf(stderr, "[DSP ERROR] Invalid FFT context\n");
        return;
    }

    // Copy input to complex array
    for (int i = 0; i < ctx->size; i++) {
        ctx->fft_in[i].r = input[i];
        ctx->fft_in[i].i = 0.0f;
    }

    // Perform FFT
    kiss_fft(ctx->cfg, ctx->fft_in, ctx->fft_out);

    // Compute magnitude
    for (int i = 0; i < ctx->size / 2; i++) {
        magnitude[i] = sqrtf(ctx->fft_out[i].r * ctx->fft_out[i].r +
                            ctx->fft_out[i].i * ctx->fft_out[i].i);
    }
}

void compute_psd_welch_with_context(psd_context_t* ctx, const float* signal,
                                    float* psd, int fft_size, int sample_rate) {
    if (!ctx) {
        fprintf(stderr, "[DSP ERROR] Invalid PSD context\n");
        return;
    }

    const int overlap = ctx->segment_size / 2;
    int num_segments = 0;

    // Clear accumulator
    memset(ctx->accumulated_psd, 0, ctx->num_bins * sizeof(float));

    // Process overlapping segments
    for (int start = 0; start <= fft_size - ctx->segment_size; start += overlap) {
        memcpy(ctx->segment, signal + start, ctx->segment_size * sizeof(float));
        compute_fft_with_context(ctx->fft_ctx, ctx->segment, ctx->segment_psd);

        for (int i = 0; i < ctx->num_bins; i++) {
            ctx->accumulated_psd[i] += ctx->segment_psd[i] * ctx->segment_psd[i];
        }
        num_segments++;
    }

    // Average, normalize, and convert to dB
    for (int i = 0; i < ctx->num_bins; i++) {
        float power = ctx->accumulated_psd[i] / num_segments;
        // Normalize by segment size to get proper PSD
        power = power / (ctx->segment_size * ctx->segment_size);
        // Convert to dB relative to reference (1.0)
        psd[i] = 10.0f * log10f(power + 1e-10f);
    }
}

float compute_band_energy(const float* magnitude, int size,
                         float freq_low, float freq_high, int sample_rate) {
    int bin_low = (int)((freq_low * size) / sample_rate);
    int bin_high = (int)((freq_high * size) / sample_rate);

    // Clamp to valid range
    if (bin_high >= size / 2) bin_high = size / 2 - 1;
    if (bin_low < 0) bin_low = 0;
    if (bin_low > bin_high) return 0.0f;

    // Compute RMS energy
    float energy = 0.0f;
    for (int i = bin_low; i <= bin_high; i++) {
        energy += magnitude[i] * magnitude[i];
    }

    return sqrtf(energy / (bin_high - bin_low + 1));
}

/*===========================================================================
 * Legacy API (allocates on each call - use context-based API instead)
 *===========================================================================*/

void compute_fft(const float* input, float* magnitude, int size) {
    fft_context_t* ctx = fft_context_create(size);
    if (!ctx) return;

    compute_fft_with_context(ctx, input, magnitude);

    fft_context_destroy(ctx);
}

void compute_psd_welch(const float* signal, float* psd, int fft_size,
                       int sample_rate, int segment_size) {
    psd_context_t* ctx = psd_context_create(segment_size);
    if (!ctx) return;

    compute_psd_welch_with_context(ctx, signal, psd, fft_size, sample_rate);

    psd_context_destroy(ctx);
}
