/*
 * common_audio_capture.c — 公共音频采集模块实现
 *
 * 功能：PDM麦克风采集，支持资源仲裁
 *
 * @yyc edit
 */

#include "common_audio_capture.h"

#include <rtdevice.h>

#define DBG_TAG "common.cap"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

static common_audio_capture_t s_capture;

int common_audio_capture_init(void)
{
    /* 初始化互斥锁 - @yyc add */
    s_capture.mutex = rt_mutex_create("audio_cap", RT_IPC_FLAG_PRIO);
    if (s_capture.mutex == RT_NULL)
    {
        LOG_E("Failed to create mutex for audio capture");
        return -RT_ENOMEM;
    }

    s_capture.device      = RT_NULL;
    s_capture.is_open      = RT_FALSE;
    s_capture.is_exclusive = RT_FALSE;  /* @yyc add */
    s_capture.exclusive_owner = AUDIO_USER_COUGH_DETECT; /* 默认为咳嗽检测 */

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

/* ===== 资源仲裁实现 - @yyc add ===== */

int common_audio_capture_request_exclusive(audio_user_t user)
{
    rt_err_t ret;

    if (s_capture.mutex == RT_NULL)
    {
        return -RT_ENOSYS;
    }

    ret = rt_mutex_take(s_capture.mutex, RT_WAITING_FOREVER);
    if (ret != RT_EOK)
    {
        LOG_E("Failed to take mutex: %d", ret);
        return ret;
    }

    /* 如果已经是被当前用户独占，直接成功 */
    if (s_capture.is_exclusive && s_capture.exclusive_owner == user)
    {
        rt_mutex_release(s_capture.mutex);
        return RT_EOK;
    }

    /* 如果被其他用户独占，返回忙 */
    if (s_capture.is_exclusive && s_capture.exclusive_owner != user)
    {
        rt_mutex_release(s_capture.mutex);
        LOG_W("Audio capture busy, owner=%d, requester=%d",
              s_capture.exclusive_owner, user);
        return -RT_EBUSY;
    }

    /* 设置为独占模式 */
    s_capture.is_exclusive   = RT_TRUE;
    s_capture.exclusive_owner = user;

    rt_mutex_release(s_capture.mutex);

    LOG_I("Audio capture granted to user=%d", user);
    return RT_EOK;
}

void common_audio_capture_release_exclusive(audio_user_t user)
{
    if (s_capture.mutex == RT_NULL)
    {
        return;
    }

    rt_mutex_take(s_capture.mutex, RT_WAITING_FOREVER);

    /* 只有持有者才能释放 */
    if (s_capture.is_exclusive && s_capture.exclusive_owner == user)
    {
        s_capture.is_exclusive = RT_FALSE;
        LOG_I("Audio capture released by user=%d", user);
    }
    else
    {
        LOG_W("Audio capture release denied: owner=%d, releaser=%d",
              s_capture.is_exclusive ? s_capture.exclusive_owner : -1, user);
    }

    rt_mutex_release(s_capture.mutex);
}

rt_bool_t common_audio_capture_is_available(audio_user_t user)
{
    rt_bool_t available;

    if (s_capture.mutex == RT_NULL)
    {
        return RT_FALSE;
    }

    rt_mutex_take(s_capture.mutex, RT_WAITING_FOREVER);

    if (!s_capture.is_exclusive)
    {
        available = RT_TRUE;
    }
    else if (s_capture.exclusive_owner == user)
    {
        available = RT_TRUE;  /* 已被自己独占，也算是可用的 */
    }
    else
    {
        available = RT_FALSE;
    }

    rt_mutex_release(s_capture.mutex);

    return available;
}

int common_audio_capture_get_exclusive_owner(void)
{
    int owner;

    if (s_capture.mutex == RT_NULL)
    {
        return -1;
    }

    rt_mutex_take(s_capture.mutex, RT_WAITING_FOREVER);
    owner = s_capture.is_exclusive ? (int)s_capture.exclusive_owner : -1;
    rt_mutex_release(s_capture.mutex);

    return owner;
}