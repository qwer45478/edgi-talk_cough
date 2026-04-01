/*
 * cough_record.c — Cough snippet recording to SD card
 *
 * Two-phase capture:
 *   Phase 1 (begin_snippet): snapshot 3 s of pre-trigger audio from the
 *           mic ring buffer into a local staging buffer.
 *   Phase 2 (feed):          append live post-trigger frames (~2 s) into
 *           the same staging buffer.  If another cough fires during this
 *           phase, extend() pushes the deadline out (up to 30 s total).
 *   Flush:  control thread writes the staging buffer as a WAV file to SD.
 */

#include "cough_record.h"

#include <rtthread.h>
#include <string.h>
#include <time.h>

#define DBG_TAG "cough.rec"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "../common/common_storage.h"

/*
 * Staging buffer — holds pre-trigger + post-trigger PCM.
 * Max 30 s × 16 kHz = 480 000 samples = 960 KB.
 * Placed in HyperRAM (shared section) to avoid consuming scarce SRAM.
 */
static int16_t s_staging[CD_SNIPPET_MAX_SAMPLES]
    __attribute__((section(".m33_m55_shared_hyperram")));

static cough_record_state_t s_state = RECORD_STATE_IDLE;
static uint32_t s_staging_len  = 0;   /* samples written into staging  */
static uint32_t s_post_remain  = 0;   /* post-trigger samples remaining */
static uint32_t s_max_remain   = 0;   /* absolute max samples remaining */

static char s_current_path[80];
static char s_last_path[80];
static cough_record_done_cb_t s_done_cb = RT_NULL;
static void *s_done_user_data = RT_NULL;

int cough_record_init(void)
{
    s_state = RECORD_STATE_IDLE;
    s_staging_len = 0;
    s_current_path[0] = '\0';
    s_last_path[0] = '\0';

    common_storage_ensure_dirs();
    LOG_I("cough snippet recorder initialized (max %d s)", CD_SNIPPET_MAX_SEC);
    return RT_EOK;
}

int cough_record_begin_snippet(const int16_t *ring_buf, uint32_t ring_size,
                               uint32_t ring_wr, uint32_t pre_samples,
                               cough_record_done_cb_t done_cb, void *user_data)
{
    uint32_t start;

    if (s_state == RECORD_STATE_POST_CAPTURE)
    {
        /* Already capturing — just extend the deadline */
        cough_record_extend();
        return RT_EOK;
    }
    if (s_state != RECORD_STATE_IDLE)
    {
        return -RT_EBUSY;
    }

    /* Clamp pre_samples to available ring data */
    if (pre_samples > ring_size)
        pre_samples = ring_size;
    if (pre_samples > ring_wr)
        pre_samples = ring_wr;

    /* Copy pre-trigger audio from ring buffer */
    start = ring_wr - pre_samples;
    for (uint32_t i = 0; i < pre_samples; i++)
    {
        s_staging[i] = ring_buf[(start + i) % ring_size];
    }
    s_staging_len = pre_samples;

    /* Set post-trigger countdown */
    s_post_remain = CD_SNIPPET_POST_SAMPLES;
    s_max_remain  = CD_SNIPPET_MAX_SAMPLES - s_staging_len;

    s_done_cb = done_cb;
    s_done_user_data = user_data;
    s_state = RECORD_STATE_POST_CAPTURE;

    LOG_I("Snippet capture started: %lu pre-trigger samples, post=%lu",
          (unsigned long)pre_samples, (unsigned long)s_post_remain);
    return RT_EOK;
}

void cough_record_feed(const int16_t *pcm_data, rt_uint32_t samples)
{
    uint32_t to_copy;

    if (s_state != RECORD_STATE_POST_CAPTURE)
        return;

    /* Clamp to remaining capacity */
    to_copy = samples;
    if (to_copy > s_post_remain)
        to_copy = s_post_remain;
    if (to_copy > s_max_remain)
        to_copy = s_max_remain;
    if (s_staging_len + to_copy > CD_SNIPPET_MAX_SAMPLES)
        to_copy = CD_SNIPPET_MAX_SAMPLES - s_staging_len;

    if (to_copy > 0)
    {
        rt_memcpy(&s_staging[s_staging_len], pcm_data, to_copy * sizeof(int16_t));
        s_staging_len += to_copy;
        s_post_remain -= to_copy;
        s_max_remain  -= to_copy;
    }

    /* Post-trigger period exhausted or max reached → ready to flush */
    if (s_post_remain == 0 || s_max_remain == 0)
    {
        s_state = RECORD_STATE_WRITING;
        /* Signal control thread to perform the blocking SD write */
        cough_detect_send_event(CD_EVENT_SNIPPET_DONE);
    }
}

void cough_record_extend(void)
{
    if (s_state != RECORD_STATE_POST_CAPTURE)
        return;

    /* Reset post-trigger countdown, respecting absolute max */
    uint32_t new_post = CD_SNIPPET_POST_SAMPLES;
    if (new_post > s_max_remain)
        new_post = s_max_remain;
    s_post_remain = new_post;

    LOG_D("Snippet extended: post_remain=%lu, total=%lu",
          (unsigned long)s_post_remain, (unsigned long)s_staging_len);
}

int cough_record_flush_to_sd(void)
{
    time_t now;
    struct tm *t;
    int result;
    cough_record_done_cb_t cb;
    void *ud;

    if (s_state != RECORD_STATE_WRITING)
        return -RT_ERROR;

    if (!common_storage_is_mounted())
    {
        LOG_W("SD card not mounted, snippet discarded");
        s_state = RECORD_STATE_IDLE;
        return -RT_ERROR;
    }

    /* Generate filename */
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

    /* Ensure space on SD card before writing */
    uint32_t need_bytes = s_staging_len * sizeof(int16_t) + 44;
    common_storage_ensure_space(need_bytes);

    /* Create WAV file */
    result = common_storage_wav_create(s_current_path,
                                       COUGH_RECORD_SAMPLE_RATE,
                                       COUGH_RECORD_BITS,
                                       COUGH_RECORD_CHANNELS);
    if (result != RT_EOK)
    {
        LOG_E("Failed to create WAV: %s", s_current_path);
        s_state = RECORD_STATE_IDLE;
        return result;
    }

    /* Write all PCM data at once */
    result = common_storage_wav_append(s_current_path, s_staging,
                                       s_staging_len * sizeof(int16_t));
    if (result != RT_EOK)
    {
        LOG_E("Failed to write PCM data");
        s_state = RECORD_STATE_IDLE;
        return result;
    }

    /* Finalize WAV header sizes */
    common_storage_wav_finalize(s_current_path);

    float duration = (float)s_staging_len / COUGH_RECORD_SAMPLE_RATE;
    LOG_I("Snippet saved: %s (%.1f s, %lu bytes)",
          s_current_path, duration,
          (unsigned long)(s_staging_len * sizeof(int16_t) + 44));

    rt_strncpy(s_last_path, s_current_path, sizeof(s_last_path) - 1);

    cb = s_done_cb;
    ud = s_done_user_data;
    s_done_cb = RT_NULL;
    s_done_user_data = RT_NULL;
    s_state = RECORD_STATE_IDLE;

    if (cb != RT_NULL)
        cb(s_last_path, ud);

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
