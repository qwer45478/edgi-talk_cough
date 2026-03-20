/*
 * cough_record.c — WAV recording to SD card
 *
 * Records audio to /sdcard/cough_audio/ with timestamp-based filenames.
 * WAV header is written first, PCM data is appended in chunks,
 * and the header is finalized when recording stops.
 */

#include "cough_record.h"

#include <rtthread.h>
#include <string.h>
#include <time.h>

#define DBG_TAG "cough.rec"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "../common/common_storage.h"

static cough_record_state_t s_state = RECORD_STATE_IDLE;
static char s_current_path[80];
static char s_last_path[80];
static rt_uint32_t s_samples_written = 0;
static rt_uint32_t s_max_samples = 0;
static cough_record_done_cb_t s_done_cb = RT_NULL;
static void *s_done_user_data = RT_NULL;

int cough_record_init(void)
{
    s_state = RECORD_STATE_IDLE;
    s_current_path[0] = '\0';
    s_last_path[0] = '\0';
    s_samples_written = 0;

    common_storage_ensure_dirs();

    LOG_I("cough recorder initialized");
    return RT_EOK;
}

int cough_record_start(cough_record_done_cb_t done_cb, void *user_data)
{
    time_t now;
    struct tm *t;
    int result;

    if (s_state != RECORD_STATE_IDLE)
    {
        LOG_W("recorder busy (state=%d)", s_state);
        return -RT_EBUSY;
    }

    if (!common_storage_is_mounted())
    {
        LOG_W("SD card not mounted, cannot record");
        return -RT_ERROR;
    }

    /* Generate filename from timestamp: cough_YYYYMMDD_HHMMSS.wav */
    now = time(RT_NULL);
    t = localtime(&now);
    if (t != RT_NULL)
    {
        rt_snprintf(s_current_path, sizeof(s_current_path),
                    "%s/cough_%04d%02d%02d_%02d%02d%02d.wav",
                    COMMON_STORAGE_AUDIO_DIR,
                    t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                    t->tm_hour, t->tm_min, t->tm_sec);
    }
    else
    {
        rt_snprintf(s_current_path, sizeof(s_current_path),
                    "%s/cough_%lu.wav", COMMON_STORAGE_AUDIO_DIR,
                    (unsigned long)time(RT_NULL));
    }

    /* Create WAV file with header */
    result = common_storage_wav_create(s_current_path,
                                       COUGH_RECORD_SAMPLE_RATE,
                                       COUGH_RECORD_BITS,
                                       COUGH_RECORD_CHANNELS);
    if (result != RT_EOK)
    {
        LOG_E("failed to create WAV file: %s", s_current_path);
        return result;
    }

    s_samples_written = 0;
    s_max_samples = COUGH_RECORD_SAMPLE_RATE * COUGH_RECORD_CHUNK_SEC;
    s_done_cb = done_cb;
    s_done_user_data = user_data;
    s_state = RECORD_STATE_RECORDING;

    LOG_I("Recording started: %s (max %lu samples)",
          s_current_path, (unsigned long)s_max_samples);
    return RT_EOK;
}

void cough_record_feed(const int16_t *pcm_data, rt_uint32_t samples)
{
    rt_size_t bytes;

    if (s_state != RECORD_STATE_RECORDING)
    {
        return;
    }

    /* Clamp to remaining capacity */
    if (s_samples_written + samples > s_max_samples)
    {
        samples = s_max_samples - s_samples_written;
    }
    if (samples == 0)
    {
        /* Chunk full, trigger finalization */
        cough_record_stop();
        return;
    }

    bytes = samples * sizeof(int16_t);
    common_storage_wav_append(s_current_path, pcm_data, bytes);
    s_samples_written += samples;

    /* Check if chunk is full */
    if (s_samples_written >= s_max_samples)
    {
        cough_record_stop();
    }
}

int cough_record_stop(void)
{
    cough_record_done_cb_t cb;
    void *ud;

    if (s_state != RECORD_STATE_RECORDING)
    {
        return RT_EOK;
    }

    s_state = RECORD_STATE_FINALIZING;

    /* Finalize WAV header with correct data size */
    common_storage_wav_finalize(s_current_path);

    LOG_I("Recording complete: %s (%lu samples, %.1f sec)",
          s_current_path,
          (unsigned long)s_samples_written,
          (float)s_samples_written / COUGH_RECORD_SAMPLE_RATE);

    rt_strncpy(s_last_path, s_current_path, sizeof(s_last_path) - 1);

    /* Notify completion */
    cb = s_done_cb;
    ud = s_done_user_data;
    s_done_cb = RT_NULL;
    s_done_user_data = RT_NULL;
    s_state = RECORD_STATE_IDLE;

    if (cb != RT_NULL)
    {
        cb(s_last_path, ud);
    }

    return RT_EOK;
}

cough_record_state_t cough_record_get_state(void)
{
    return s_state;
}

const char *cough_record_get_last_path(void)
{
    return s_last_path[0] ? s_last_path : RT_NULL;
}
