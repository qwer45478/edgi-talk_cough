#ifndef COMMON_AUDIO_CAPTURE_H
#define COMMON_AUDIO_CAPTURE_H

#include <rtthread.h>

#define COMMON_AUDIO_CAPTURE_DEVICE_NAME   "mic0"

typedef struct
{
    rt_device_t device;
    rt_bool_t is_open;
} common_audio_capture_t;

int common_audio_capture_init(void);
int common_audio_capture_open(void);
int common_audio_capture_close(void);
rt_size_t common_audio_capture_read(void *buffer, rt_size_t bytes);
const common_audio_capture_t *common_audio_capture_get(void);

#endif