#include "common_audio_capture.h"

#include <rtdevice.h>

#define DBG_TAG "common.cap"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

static common_audio_capture_t s_capture;

int common_audio_capture_init(void)
{
    s_capture.device = rt_device_find(COMMON_AUDIO_CAPTURE_DEVICE_NAME);
    if (s_capture.device == RT_NULL)
    {
        LOG_E("capture device not found: %s", COMMON_AUDIO_CAPTURE_DEVICE_NAME);
        return -RT_ENOSYS;
    }

    LOG_I("capture device ready: %s", COMMON_AUDIO_CAPTURE_DEVICE_NAME);
    return RT_EOK;
}

int common_audio_capture_open(void)
{
    rt_err_t result;

    if (s_capture.device == RT_NULL)
    {
        result = common_audio_capture_init();
        if (result != RT_EOK)
        {
            return result;
        }
    }
    if (s_capture.is_open)
    {
        return RT_EOK;
    }

    result = rt_device_open(s_capture.device, RT_DEVICE_FLAG_RDONLY);
    if (result != RT_EOK)
    {
        LOG_E("open capture device failed: %d", result);
        return result;
    }

    s_capture.is_open = RT_TRUE;
    return RT_EOK;
}

int common_audio_capture_close(void)
{
    if ((s_capture.device != RT_NULL) && s_capture.is_open)
    {
        rt_device_close(s_capture.device);
        s_capture.is_open = RT_FALSE;
    }
    return RT_EOK;
}

rt_size_t common_audio_capture_read(void *buffer, rt_size_t bytes)
{
    if ((s_capture.device == RT_NULL) || !s_capture.is_open)
    {
        return 0;
    }

    return rt_device_read(s_capture.device, 0, buffer, bytes);
}

const common_audio_capture_t *common_audio_capture_get(void)
{
    return &s_capture;
}