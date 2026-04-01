/*
 * cough_record.h — Cough snippet recording to SD card
 *
 * On cough detection, captures a short audio snippet:
 *   - 3 seconds pre-trigger (from ring buffer snapshot)
 *   - 2 seconds post-trigger (live capture)
 * Consecutive coughs within the post-trigger window extend the
 * recording, up to a maximum of 30 seconds total.
 */

#ifndef COUGH_RECORD_H
#define COUGH_RECORD_H

#include <rtthread.h>
#include "cough_detect.h"

#ifdef __cplusplus
extern "C" {
#endif

#define COUGH_RECORD_SAMPLE_RATE    16000
#define COUGH_RECORD_BITS           16
#define COUGH_RECORD_CHANNELS       1

typedef enum
{
    RECORD_STATE_IDLE = 0,
    RECORD_STATE_POST_CAPTURE,  /* capturing post-trigger audio */
    RECORD_STATE_WRITING,       /* async SD card write in progress */
} cough_record_state_t;

typedef void (*cough_record_done_cb_t)(const char *wav_path, void *user_data);

int cough_record_init(void);

/**
 * Begin snippet capture: snapshot pre-trigger audio from the ring buffer,
 * then start collecting post-trigger frames.
 * @param ring_buf       Pointer to circular audio buffer
 * @param ring_size      Total ring buffer size in samples
 * @param ring_wr        Current write position (in samples, monotonic)
 * @param pre_samples    How many pre-trigger samples to capture
 * @param done_cb        Callback when WAV file is written
 * @param user_data      User data passed to callback
 * @return 0 on success
 */
int cough_record_begin_snippet(const int16_t *ring_buf, uint32_t ring_size,
                               uint32_t ring_wr, uint32_t pre_samples,
                               cough_record_done_cb_t done_cb, void *user_data);

/**
 * Feed live PCM frames during post-trigger capture.
 * Called from mic_thread while state == RECORD_STATE_POST_CAPTURE.
 */
void cough_record_feed(const int16_t *pcm_data, rt_uint32_t samples);

/**
 * Extend the post-trigger deadline (called when another cough is
 * detected during an ongoing capture).
 */
void cough_record_extend(void);

/**
 * Flush the captured snippet to SD card.
 * Called from the control thread (blocking I/O).
 * @return 0 on success
 */
int cough_record_flush_to_sd(void);

cough_record_state_t cough_record_get_state(void);
const char *cough_record_get_last_path(void);

#ifdef __cplusplus
}
#endif

#endif /* COUGH_RECORD_H */
