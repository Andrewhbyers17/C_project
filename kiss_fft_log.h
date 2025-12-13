/*
 * kiss_fft_log.h - Minimal stub for kiss_fft logging
 * This file is intentionally empty as logging is not used in this build
 */

#ifndef KISS_FFT_LOG_H
#define KISS_FFT_LOG_H

#include <stdio.h>
#include <stdlib.h>

/* Define error macro */
#define KISS_FFT_ERROR(msg) do { fprintf(stderr, "KISS_FFT_ERROR: %s\n", msg); exit(1); } while(0)

#endif /* KISS_FFT_LOG_H */
