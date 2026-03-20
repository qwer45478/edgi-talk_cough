#include "common_audio_playback.h"

#include <rtdevice.h>
#include <string.h>
#include <math.h>

#include "common_power.h"

#define DBG_TAG "common.play"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define PLAYBACK_DEVICE_NAME    "sound0"
#define PLAYBACK_SAMPLE_RATE    16000
#define TONE_AMPLITUDE          8000     /* INT16 amplitude for tone generation */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static common_audio_playback_t s_playback;

int common_audio_playback_init(void)
{
    s_playback.volume = 80;
    s_playback.powered = RT_TRUE;
    s_playback.device = RT_NULL;
    s_playback.is_open = RT_FALSE;

    common_power_set_audio(RT_TRUE);
    common_power_set_speaker(RT_TRUE);

    s_playback.device = rt_device_find(PLAYBACK_DEVICE_NAME);
    if (s_playback.device == RT_NULL)
    {
        LOG_W("playback device '%s' not found; tone generation disabled",
              PLAYBACK_DEVICE_NAME);
    }

    LOG_I("audio playback power ready");
    return RT_EOK;
}

static int playback_open(void)
{
    if (s_playback.device == RT_NULL)
    {
        return -RT_ENOSYS;
    }
    if (s_playback.is_open)
    {
        return RT_EOK;
    }

    rt_err_t result = rt_device_open(s_playback.device, RT_DEVICE_OFLAG_WRONLY);
    if (result != RT_EOK)
    {
        LOG_E("open playback device failed: %d", result);
        return result;
    }

    /* Configure sample rate */
    struct rt_audio_caps caps;
    caps.main_type = AUDIO_TYPE_OUTPUT;
    caps.sub_type  = AUDIO_DSP_SAMPLERATE;
    caps.udata.value = PLAYBACK_SAMPLE_RATE;
    rt_device_control(s_playback.device, AUDIO_CTL_CONFIGURE, &caps);

    /* Set volume */
    caps.main_type = AUDIO_TYPE_MIXER;
    caps.sub_type  = AUDIO_MIXER_VOLUME;
    caps.udata.value = s_playback.volume;
    rt_device_control(s_playback.device, AUDIO_CTL_CONFIGURE, &caps);

    s_playback.is_open = RT_TRUE;
    return RT_EOK;
}

static void playback_close(void)
{
    if (s_playback.device != RT_NULL && s_playback.is_open)
    {
        rt_device_close(s_playback.device);
        s_playback.is_open = RT_FALSE;
    }
}

int common_audio_playback_set_volume(rt_int16_t volume)
{
    if (volume < 0)
    {
        volume = 0;
    }
    if (volume > 100)
    {
        volume = 100;
    }

    s_playback.volume = volume;

    /* Update hardware if device is open */
    if (s_playback.device != RT_NULL && s_playback.is_open)
    {
        struct rt_audio_caps caps;
        caps.main_type   = AUDIO_TYPE_MIXER;
        caps.sub_type    = AUDIO_MIXER_VOLUME;
        caps.udata.value = volume;
        rt_device_control(s_playback.device, AUDIO_CTL_CONFIGURE, &caps);
    }

    return RT_EOK;
}

int common_audio_playback_mute(rt_bool_t mute)
{
    s_playback.powered = !mute;
    return common_power_set_speaker(!mute);
}

int common_audio_playback_play_pcm(const void *data, rt_size_t bytes, rt_uint32_t sample_rate)
{
    if (data == RT_NULL || bytes == 0)
    {
        return -RT_EINVAL;
    }

    int result = playback_open();
    if (result != RT_EOK)
    {
        return result;
    }

    /* Reconfigure sample rate if different */
    if (sample_rate != PLAYBACK_SAMPLE_RATE)
    {
        struct rt_audio_caps caps;
        caps.main_type   = AUDIO_TYPE_OUTPUT;
        caps.sub_type    = AUDIO_DSP_SAMPLERATE;
        caps.udata.value = sample_rate;
        rt_device_control(s_playback.device, AUDIO_CTL_CONFIGURE, &caps);
    }

    rt_device_write(s_playback.device, 0, data, bytes);
    return RT_EOK;
}

int common_audio_playback_beep(rt_uint32_t freq_hz, rt_uint32_t duration_ms)
{
    rt_uint32_t num_samples;
    rt_int16_t *buf;
    rt_size_t buf_bytes;
    int result;

    if (freq_hz == 0 || duration_ms == 0)
    {
        return -RT_EINVAL;
    }

    result = playback_open();
    if (result != RT_EOK)
    {
        return result;
    }

    num_samples = (PLAYBACK_SAMPLE_RATE * duration_ms) / 1000;
    buf_bytes = num_samples * sizeof(rt_int16_t);

    buf = (rt_int16_t *)rt_malloc(buf_bytes);
    if (buf == RT_NULL)
    {
        return -RT_ENOMEM;
    }

    /* Generate sine wave */
    float phase_inc = 2.0f * (float)M_PI * (float)freq_hz / (float)PLAYBACK_SAMPLE_RATE;
    float amplitude = (float)TONE_AMPLITUDE * (float)s_playback.volume / 100.0f;

    for (rt_uint32_t i = 0; i < num_samples; i++)
    {
        float phase = phase_inc * (float)i;
        /* Apply fade-in/out envelope (first/last 5ms) to avoid clicks */
        float env = 1.0f;
        rt_uint32_t fade_samples = (PLAYBACK_SAMPLE_RATE * 5) / 1000;
        if (i < fade_samples)
        {
            env = (float)i / (float)fade_samples;
        }
        else if (i > num_samples - fade_samples)
        {
            env = (float)(num_samples - i) / (float)fade_samples;
        }
        buf[i] = (rt_int16_t)(sinf(phase) * amplitude * env);
    }

    rt_device_write(s_playback.device, 0, buf, buf_bytes);
    rt_free(buf);

    return RT_EOK;
}

int common_audio_playback_melody(const rt_uint32_t *notes, const rt_uint32_t *durations,
                                 rt_size_t count)
{
    if (notes == RT_NULL || durations == RT_NULL || count == 0)
    {
        return -RT_EINVAL;
    }

    for (rt_size_t i = 0; i < count; i++)
    {
        if (notes[i] > 0)
        {
            common_audio_playback_beep(notes[i], durations[i]);
        }
        /* Inter-note silence */
        rt_thread_mdelay(durations[i] + 30);
    }

    return RT_EOK;
}

const common_audio_playback_t *common_audio_playback_get(void)
{
    return &s_playback;
}