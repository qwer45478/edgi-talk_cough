#include "app_common.h"

#include "common_audio_capture.h"
#include "common_audio_playback.h"
#include "common_display.h"
#include "common_env.h"
#include "common_key.h"
#include "common_led.h"
#include "common_network.h"
#include "common_power.h"
#include "common_scheduler.h"
#include "common_storage.h"

#define DBG_TAG "common"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

static app_common_status_t s_common_status;

static void mark_module(rt_uint32_t mask, int result)
{
    if (result == RT_EOK)
    {
        s_common_status.ready_mask |= mask;
    }
    else
    {
        LOG_W("module init failed: mask=0x%08x, result=%d", mask, result);
    }
}

int app_common_init(rt_uint32_t init_mask)
{
    s_common_status.init_mask = init_mask;
    s_common_status.ready_mask = 0;

    if (init_mask & APP_COMMON_INIT_POWER)
    {
        mark_module(APP_COMMON_INIT_POWER, common_power_init());
    }
    if (init_mask & APP_COMMON_INIT_KEY)
    {
        mark_module(APP_COMMON_INIT_KEY, common_key_init());
    }
    if (init_mask & APP_COMMON_INIT_AUDIO_CAPTURE)
    {
        mark_module(APP_COMMON_INIT_AUDIO_CAPTURE, common_audio_capture_init());
    }
    if (init_mask & APP_COMMON_INIT_AUDIO_PLAYBACK)
    {
        mark_module(APP_COMMON_INIT_AUDIO_PLAYBACK, common_audio_playback_init());
    }
    if (init_mask & APP_COMMON_INIT_DISPLAY)
    {
        mark_module(APP_COMMON_INIT_DISPLAY, common_display_init());
    }
    if (init_mask & APP_COMMON_INIT_NETWORK)
    {
        mark_module(APP_COMMON_INIT_NETWORK, common_network_init());
    }
    if (init_mask & APP_COMMON_INIT_STORAGE)
    {
        mark_module(APP_COMMON_INIT_STORAGE, common_storage_init());
    }
    if (init_mask & APP_COMMON_INIT_SCHEDULER)
    {
        mark_module(APP_COMMON_INIT_SCHEDULER, common_scheduler_init());
    }
    if (init_mask & APP_COMMON_INIT_ENV)
    {
        mark_module(APP_COMMON_INIT_ENV, common_env_init());
    }
    if (init_mask & APP_COMMON_INIT_LED)
    {
        mark_module(APP_COMMON_INIT_LED, common_led_init());
    }

    return RT_EOK;
}

void app_common_dump_status(void)
{
    LOG_I("common init mask:  0x%08x", s_common_status.init_mask);
    LOG_I("common ready mask: 0x%08x", s_common_status.ready_mask);
}

const app_common_status_t *app_common_get_status(void)
{
    return &s_common_status;
}