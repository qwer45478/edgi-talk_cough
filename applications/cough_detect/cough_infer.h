/*
 * cough_infer.h — Cough detection inference interface (C-compatible)
 *
 * Provides TFLite Micro based cough/noise/unlabelled classification.
 * Internally performs log-power-spectrum feature extraction using CMSIS-DSP,
 * then runs the quantised INT8 CNN model.
 */

#ifndef COUGH_INFER_H
#define COUGH_INFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Output class indices ─────────────────────────────────────── */
#define COUGH_INFER_NUM_CLASSES     3
#define COUGH_INFER_IDX_UNLABELLED  0
#define COUGH_INFER_IDX_COUGH       1
#define COUGH_INFER_IDX_NOISE       2

/* ── Audio window required for one inference ──────────────────── */
/*  40 spectral frames × 256 hop + (512 − 256) = 10496 samples    */
#define COUGH_INFER_WINDOW_SAMPLES  10496

/**
 * Initialize TFLite Micro interpreter and load the cough detection model.
 * Must be called once before cough_infer_classify().
 *
 * @return 0 on success, negative on error
 */
int cough_infer_init(void);

/**
 * Run cough detection inference on a block of raw PCM audio.
 *
 * Internally performs:
 *   1. int16 → float conversion
 *   2. 512-pt FFT with hop 256 → 40 frames of 257-bin log-power-spectrum
 *   3. Quantize to INT8 and feed into TFLite Micro
 *   4. Dequantize output to float confidence scores
 *
 * @param audio       Raw 16-bit PCM audio (16 kHz, mono)
 * @param num_samples Number of samples (must be >= COUGH_INFER_WINDOW_SAMPLES)
 * @param scores      Output: confidence per class [0..1], size COUGH_INFER_NUM_CLASSES
 * @return 0 on success, negative on error
 */
int cough_infer_classify(const int16_t *audio, uint32_t num_samples,
                         float scores[COUGH_INFER_NUM_CLASSES]);

#ifdef __cplusplus
}
#endif

#endif /* COUGH_INFER_H */
