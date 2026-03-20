/*
 * cough_record.h — Cough audio recording to SD card
 *
 * Records 5-minute WAV chunks to the SD card when triggered.
 * Supports recording state management and file naming.
 */

#ifndef COUGH_RECORD_H
#define COUGH_RECORD_H

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COUGH_RECORD_SAMPLE_RATE    16000
#define COUGH_RECORD_BITS           16
#define COUGH_RECORD_CHANNELS       1
#define COUGH_RECORD_CHUNK_SEC      (5 * 60)    /* 5 minutes per chunk */

typedef enum
{
    RECORD_STATE_IDLE = 0,
    RECORD_STATE_RECORDING,
    RECORD_STATE_FINALIZING,
} cough_record_state_t;

typedef void (*cough_record_done_cb_t)(const char *wav_path, void *user_data);

int cough_record_init(void);

/**
 * Start recording audio to a new WAV file.
 * Generates filename based on current timestamp.
 * @param done_cb  Callback when recording chunk completes
 * @param user_data  User data passed to callback
 * @return 0 on success
 */
int cough_record_start(cough_record_done_cb_t done_cb, void *user_data);

/**
 * Feed PCM audio data into the recorder.
 * Must be called from the mic thread during recording state.
 * @param pcm_data  Raw 16-bit PCM samples
 * @param samples   Number of samples
 */
void cough_record_feed(const int16_t *pcm_data, rt_uint32_t samples);

/**
 * Stop recording and finalize the WAV file.
 */
int cough_record_stop(void);

cough_record_state_t cough_record_get_state(void);

/**
 * Get the path of the last completed recording.
 */
const char *cough_record_get_last_path(void);

#ifdef __cplusplus
}
#endif

#endif /* COUGH_RECORD_H */
