/*
 * cough_detect.h — Cough / Snore detection module interface
 *
 * Overall system states:
 *
 *   IDLE ──[key pressed]──► CALIBRATE  (sample ambient noise, build baseline)
 *        ◄─[key released]──            ──► LISTENING
 *
 *   LISTENING ──[energy > threshold]──► RECORDING  (record 5-min chunk to SD)
 *             ◄─[chunk done]─────────────────────── ──► UPLOADING
 *
 *   UPLOADING ──[done]──► LISTENING  (loop)
 */

#ifndef COUGH_DETECT_H
#define COUGH_DETECT_H

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Pin / hardware ──────────────────────────────────────────────── */
#define CD_BUTTON_PIN       GET_PIN(8, 3)   /* same as original project */
#define CD_MIC_DEVICE_NAME  "mic0"          /* PDM microphone */

/* ── Audio parameters ────────────────────────────────────────────── */
#define CD_SAMPLE_RATE          16000       /* Hz */
#define CD_FRAME_SAMPLES        160         /* 10 ms frame @ 16 kHz */
#define CD_FRAME_BYTES          (CD_FRAME_SAMPLES * sizeof(int16_t))

/* ── Detection thresholds ────────────────────────────────────────── */
/* Energy above (baseline * CD_SNR_FACTOR) triggers recording start   */
#define CD_SNR_FACTOR           3.0f
/* Minimum cough confidence to trigger count */
#define CD_COUGH_THRESHOLD  0.35f
/* Calibration duration: how many frames to average for baseline      */
#define CD_CALIB_FRAMES         500         /* ~5 seconds */

/* ── Recording parameters ────────────────────────────────────────── */
#define CD_RECORD_DURATION_S    (5 * 60)    /* 5 minutes per chunk */
#define CD_RECORD_TOTAL_SAMPLES (CD_SAMPLE_RATE * CD_RECORD_DURATION_S)

/* ── Inter-task events ───────────────────────────────────────────── */
#define CD_EVENT_KEY_DOWN       (1 << 0)  /* begin noise calibration   */
#define CD_EVENT_KEY_UP         (1 << 1)  /* end calibration, start listening */
#define CD_EVENT_TRIGGERED      (1 << 2)  /* energy threshold crossed  */
#define CD_EVENT_CHUNK_DONE     (1 << 3)  /* 5-min chunk complete      */
#define CD_EVENT_UPLOAD_TICK    (1 << 4)  /* periodic stats upload     */
#define CD_EVENT_REMIND_FIRE    (1 << 5)  /* reminder alert triggered  */
#define CD_EVENT_STAT_FLUSH     (1 << 6)  /* midnight stats flush      */

/* ── System state ────────────────────────────────────────────────── */
typedef enum {
    CD_STATE_IDLE       = 0,
    CD_STATE_CALIBRATE,
    CD_STATE_LISTENING,
    CD_STATE_RECORDING,
    CD_STATE_UPLOADING,
} cd_state_t;

/* ── Public API ─────────────────────────────────────────────────── */

/**
 * Initialize hardware (button, mic) and start processing threads.
 * Called once from main().
 * Returns 0 on success, negative on error.
 */
int cough_detect_init(void);

/**
 * Return the current system state (thread-safe read).
 */
cd_state_t cough_detect_get_state(void);

/**
 * Return the most recent computed noise baseline energy level.
 * Valid after at least one calibration has completed.
 */
float cough_detect_get_baseline(void);

/**
 * Send an event to the control thread (thread-safe, callable from timer/ISR).
 */
void cough_detect_send_event(rt_uint32_t event_set);

#ifdef __cplusplus
}
#endif

#endif /* COUGH_DETECT_H */
