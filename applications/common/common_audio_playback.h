#ifndef COMMON_AUDIO_PLAYBACK_H
#define COMMON_AUDIO_PLAYBACK_H

#include <rtthread.h>

typedef struct
{
    rt_int16_t volume;
    rt_bool_t powered;
    rt_device_t device;
    rt_bool_t is_open;
} common_audio_playback_t;

int common_audio_playback_init(void);
int common_audio_playback_set_volume(rt_int16_t volume);
int common_audio_playback_mute(rt_bool_t mute);
int common_audio_playback_play_pcm(const void *data, rt_size_t bytes, rt_uint32_t sample_rate);
const common_audio_playback_t *common_audio_playback_get(void);

/* Tone generation for alerts/reminders */
int common_audio_playback_beep(rt_uint32_t freq_hz, rt_uint32_t duration_ms);
int common_audio_playback_melody(const rt_uint32_t *notes, const rt_uint32_t *durations,
                                 rt_size_t count);

#endif