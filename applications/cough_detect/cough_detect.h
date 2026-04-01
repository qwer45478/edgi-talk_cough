/*
 * cough_detect.h — Cough / Snore detection module interface
 *
 * Overall system states:
 *
 *   (boot) ──► CALIBRATE  (auto-sample ambient noise for ~5 s)
 *          ──► LISTENING   (baseline established, continuous inference)
 *
 *   LISTENING stays active at all times.  When a cough is detected,
 *   a 5-second audio snippet (3 s pre-trigger + 2 s post-trigger) is
 *   captured and written to SD card asynchronously.  The system never
 *   leaves LISTENING state for recording.
 *
 *   CD_EVENT_CALIBRATE can re-trigger calibration at any time.
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

/* ── Snippet recording parameters ────────────────────────────────── */
#define CD_SNIPPET_PRE_SEC      3           /* seconds before trigger   */
#define CD_SNIPPET_POST_SEC     2           /* seconds after trigger    */
#define CD_SNIPPET_MAX_SEC      30          /* absolute max duration    */
#define CD_SNIPPET_PRE_SAMPLES  (CD_SAMPLE_RATE * CD_SNIPPET_PRE_SEC)
#define CD_SNIPPET_POST_SAMPLES (CD_SAMPLE_RATE * CD_SNIPPET_POST_SEC)
#define CD_SNIPPET_MAX_SAMPLES  (CD_SAMPLE_RATE * CD_SNIPPET_MAX_SEC)

/* ── Inter-task events ───────────────────────────────────────────── */
#define CD_EVENT_CALIBRATE      (1 << 0)  /* (re)start noise calibration */
#define CD_EVENT_COUGH          (1 << 1)  /* cough detected → snippet   */
#define CD_EVENT_SNIPPET_DONE   (1 << 3)  /* snippet WAV written to SD  */
#define CD_EVENT_UPLOAD_TICK    (1 << 4)  /* periodic stats upload     */
#define CD_EVENT_REMIND_FIRE    (1 << 5)  /* reminder alert triggered  */
#define CD_EVENT_STAT_FLUSH     (1 << 6)  /* midnight stats flush      */
#define CD_EVENT_UI_REFRESH    (1 << 7)  /* periodic UI data refresh   */

/* ── System state ────────────────────────────────────────────────── */
typedef enum {
    CD_STATE_IDLE       = 0,
    CD_STATE_CALIBRATE,
    CD_STATE_LISTENING,
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
