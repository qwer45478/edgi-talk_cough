/*
 * cough_infer.cpp — TFLite Micro inference + Log-Power-Spectrum feature extraction
 *
 * Preprocessing pipeline (matches DeepCraft model.c training parameters):
 *
 *   PCM int16  →  float [-1, 1]
 *     →  512-pt real FFT  (hop 256, CMSIS-DSP arm_rfft_fast_f32)
 *     →  complex magnitude  (arm_cmplx_mag_f32)
 *     →  ln(mag + 1e-6) × 0.1
 *     →  stack 40 frames  →  (40, 257, 1) float tensor
 *     →  quantize to INT8  (scale / zero_point from model)
 *     →  TFLite Micro Invoke()
 *     →  dequantize output  →  3 float confidence scores
 *
 * All large buffers are static to minimise stack usage.
 */

/* ── RT-Thread logging (DBG_TAG must precede rtdbg.h) ─────────── */
extern "C" {
#include <rtthread.h>
#define DBG_TAG    "cough.infer"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>
}

#include <cstring>
#include <cmath>

/* ── CMSIS-DSP (has its own extern "C" guards) ────────────────── */
#include "arm_math.h"

/* ── TFLite Micro (C++ native) ────────────────────────────────── */
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

/* ── Our headers ──────────────────────────────────────────────── */
extern "C" {
#include "cough_infer.h"
#include "cough_model_data.h"
}

/* ── Preprocessing constants (must match training pipeline) ───── */
#define FFT_SIZE        COUGH_MODEL_FFT_SIZE       /* 512  */
#define FFT_HOP         COUGH_MODEL_FFT_HOP        /* 256  */
#define FFT_FREQ_BINS   (FFT_SIZE / 2 + 1)         /* 257  */
#define NUM_FRAMES      COUGH_MODEL_INPUT_FRAMES    /* 40   */
#define EPSILON         1e-6f
#define LOG_SCALE       0.1f

/* ── TFLite arena ─────────────────────────────────────────────── */
#define ARENA_SIZE      (100 * 1024)   /* 100 KB, tunable */

static uint8_t s_arena[ARENA_SIZE] __attribute__((aligned(16), section(".cy_socmem_bss")));

/* ── TFLite state (filled once by cough_infer_init) ───────────── */
static const tflite::Model      *s_model       = nullptr;
static tflite::MicroInterpreter *s_interpreter  = nullptr;
static TfLiteTensor             *s_input_tensor  = nullptr;
static TfLiteTensor             *s_output_tensor = nullptr;

/* Quantization parameters cached for fast access */
static float s_input_scale      = 0.0f;
static int   s_input_zero_point = 0;
static float s_output_scale     = 0.0f;
static int   s_output_zero_point = 0;

/* ── FFT workspace (static to save stack) ─────────────────────── */
static arm_rfft_fast_instance_f32 s_fft_inst;
static float s_fft_buf[FFT_SIZE]   __attribute__((section(".cy_socmem_bss")));  /* writable copy for arm_rfft   */
static float s_fft_out[FFT_SIZE]   __attribute__((section(".cy_socmem_bss")));  /* packed complex output        */
static float s_magnitude[FFT_FREQ_BINS] __attribute__((section(".cy_socmem_bss"))); /* magnitude per bin */
static float s_frame_f32[FFT_SIZE] __attribute__((section(".cy_socmem_bss"))); /* int16→float conversion buf   */

/* ================================================================
 *  Initialise the inference engine (call once at startup)
 * ================================================================ */
extern "C" int cough_infer_init(void)
{
    /* 1. Load model from const C array (lives in Flash) */
    s_model = tflite::GetModel(cough_model_tflite);
    if (s_model == nullptr) {
        LOG_E("Failed to map TFLite model");
        return -1;
    }
    if (s_model->version() != TFLITE_SCHEMA_VERSION) {
        LOG_E("Model schema v%lu != expected v%d",
              (unsigned long)s_model->version(), TFLITE_SCHEMA_VERSION);
        return -2;
    }

    /* 2. Register only the operators our model needs */
    /*
     * Op list determined by parsing cough_model.tflite:
     *   CONV_2D, MAX_POOL_2D, FULLY_CONNECTED, SOFTMAX
     *   RESHAPE, SHAPE, STRIDED_SLICE, PACK   ← Flatten uses dynamic shape path
     */
    static tflite::MicroMutableOpResolver<8> resolver;
    resolver.AddConv2D();
    resolver.AddFullyConnected();
    resolver.AddMaxPool2D();
    resolver.AddReshape();
    resolver.AddSoftmax();
    resolver.AddShape();
    resolver.AddStridedSlice();
    resolver.AddPack();

    /* 3. Create interpreter (static local — constructed once) */
    static tflite::MicroInterpreter interpreter(
        s_model, resolver, s_arena, ARENA_SIZE);
    s_interpreter = &interpreter;

    /* 4. Allocate tensors */
    if (s_interpreter->AllocateTensors(true) != kTfLiteOk) {
        LOG_E("AllocateTensors() failed (arena %d KB may be too small)",
              ARENA_SIZE / 1024);
        return -3;
    }

    /* 5. Cache tensor pointers and quant params */
    s_input_tensor  = s_interpreter->input(0);
    s_output_tensor = s_interpreter->output(0);
    if (!s_input_tensor || !s_output_tensor) {
        LOG_E("Cannot get input/output tensors");
        return -4;
    }

    s_input_scale      = s_input_tensor->params.scale;
    s_input_zero_point = s_input_tensor->params.zero_point;
    s_output_scale     = s_output_tensor->params.scale;
    s_output_zero_point = s_output_tensor->params.zero_point;

    LOG_I("Model loaded OK");
    LOG_I("  Input:  [%d,%d,%d,%d]  scale=%.6f zp=%d",
          s_input_tensor->dims->data[0], s_input_tensor->dims->data[1],
          s_input_tensor->dims->data[2], s_input_tensor->dims->data[3],
          s_input_scale, s_input_zero_point);
    LOG_I("  Output: [%d,%d]  scale=%.6f zp=%d",
          s_output_tensor->dims->data[0], s_output_tensor->dims->data[1],
          s_output_scale, s_output_zero_point);
    LOG_I("  Arena:  %u / %u bytes used",
          (unsigned)s_interpreter->arena_used_bytes(), (unsigned)ARENA_SIZE);

    /* 6. Initialise CMSIS-DSP real FFT instance (512 pt) */
    if (arm_rfft_fast_init_f32(&s_fft_inst, FFT_SIZE) != ARM_MATH_SUCCESS) {
        LOG_E("arm_rfft_fast_init_f32(%d) failed", FFT_SIZE);
        return -5;
    }

    LOG_I("cough_infer_init complete");
    return 0;
}

/* ================================================================
 *  Compute one 257-bin log-power-spectrum frame
 *
 *  Input  : 512 float samples  (already converted from int16)
 *  Output : 257 float values   (ln(|FFT| + ε) × 0.1)
 * ================================================================ */
static void compute_log_spectrum(const float *samples_512, float *spectrum)
{
    /* arm_rfft_fast_f32 modifies its input — work on a copy */
    std::memcpy(s_fft_buf, samples_512, FFT_SIZE * sizeof(float));

    /* Forward FFT → packed complex output */
    arm_rfft_fast_f32(&s_fft_inst, s_fft_buf, s_fft_out, 0);

    /*
     * Output layout of arm_rfft_fast_f32 for N-point FFT:
     *   s_fft_out[0] = X[0]   (DC,      real only)
     *   s_fft_out[1] = X[N/2] (Nyquist, real only)
     *   s_fft_out[2..3]   = Re(X[1]),  Im(X[1])
     *   s_fft_out[4..5]   = Re(X[2]),  Im(X[2])
     *   ...
     *   s_fft_out[N-2..N-1] = Re(X[N/2-1]), Im(X[N/2-1])
     */

    /* DC bin (index 0) — no imaginary part */
    spectrum[0] = std::fabsf(s_fft_out[0]);

    /* Bins 1 .. N/2-1 (255 complex pairs starting at s_fft_out[2]) */
    arm_cmplx_mag_f32(&s_fft_out[2], &spectrum[1], FFT_SIZE / 2 - 1);

    /* Nyquist bin (index 256) — no imaginary part */
    spectrum[FFT_FREQ_BINS - 1] = std::fabsf(s_fft_out[1]);

    /* ln(magnitude + ε) × 0.1 */
    for (int i = 0; i < FFT_FREQ_BINS; i++) {
        spectrum[i] = std::logf(spectrum[i] + EPSILON) * LOG_SCALE;
    }
}

/* ── Quantize a single float to int8 ─────────────────────────── */
static inline int8_t quantize_f32(float value)
{
    int32_t q = (int32_t)std::roundf(value / s_input_scale) + s_input_zero_point;
    if (q < -128) q = -128;
    if (q >  127) q =  127;
    return (int8_t)q;
}

/* ================================================================
 *  Run inference on a block of raw PCM audio
 * ================================================================ */
extern "C" int cough_infer_classify(const int16_t *audio, uint32_t num_samples,
                                    float scores[COUGH_INFER_NUM_CLASSES])
{
    if (!s_interpreter) {
        LOG_E("Interpreter not initialised — call cough_infer_init() first");
        return -1;
    }
    if (num_samples < (uint32_t)COUGH_INFER_WINDOW_SAMPLES) {
        LOG_E("Audio too short: %lu < %d",
              (unsigned long)num_samples, COUGH_INFER_WINDOW_SAMPLES);
        return -2;
    }

    int8_t *input_data = s_input_tensor->data.int8;

    /* ── Feature extraction: 40 spectral frames ───────────────── */
    /*
     *  Frame 0  : audio[0       .. 511]
     *  Frame 1  : audio[256     .. 767]
     *  ...
     *  Frame 39 : audio[9984    .. 10495]
     *
     *  Total: (NUM_FRAMES - 1) × FFT_HOP + FFT_SIZE = 10496 samples
     */
    for (int f = 0; f < NUM_FRAMES; f++) {
        int offset = f * FFT_HOP;

        /* Convert int16 PCM → float [-1.0, 1.0) */
        for (int i = 0; i < FFT_SIZE; i++) {
            s_frame_f32[i] = (float)audio[offset + i] / 32768.0f;
        }

        /* Compute log-power-spectrum → s_magnitude[257] */
        compute_log_spectrum(s_frame_f32, s_magnitude);

        /* Quantize and write directly into the input tensor.
         * Tensor layout: [batch=1][frame][freq_bin][channel=1]
         * With channel == 1 this simplifies to a flat [frame * 257 + bin]. */
        int t_off = f * FFT_FREQ_BINS;
        for (int b = 0; b < FFT_FREQ_BINS; b++) {
            input_data[t_off + b] = quantize_f32(s_magnitude[b]);
        }
    }

    /* ── Run TFLite Micro inference ───────────────────────────── */
    if (s_interpreter->Invoke() != kTfLiteOk) {
        LOG_E("Invoke() failed");
        return -3;
    }

    /* ── Dequantize output → float scores ─────────────────────── */
    const int8_t *out = s_output_tensor->data.int8;
    for (int i = 0; i < COUGH_INFER_NUM_CLASSES; i++) {
        scores[i] = ((float)out[i] - (float)s_output_zero_point) * s_output_scale;
    }

    return 0;
}
