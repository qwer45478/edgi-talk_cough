#include "common_display.h"

#include <rtdevice.h>

#include "common_power.h"

#define DBG_TAG "common.disp"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

static common_display_t s_display;

int common_display_init(void)
{
    common_power_set_display(RT_TRUE);
    common_power_set_backlight(RT_TRUE);

    s_display.lcd = rt_device_find("lcd");
    if (s_display.lcd == RT_NULL)
    {
        LOG_E("lcd device not found");
        return -RT_ENOSYS;
    }

    s_display.is_ready = RT_TRUE;
    s_display.brightness = 100;
    LOG_I("display device ready: lcd");
    return RT_EOK;
}

int common_display_set_brightness(rt_uint8_t percent)
{
    s_display.brightness = percent;
    return common_power_set_backlight_percent(percent);
}

int common_display_set_backlight(rt_bool_t enabled)
{
    return common_power_set_backlight(enabled);
}

int common_display_refresh_full(void)
{
    if ((s_display.lcd == RT_NULL) || !s_display.is_ready)
    {
        return -RT_ERROR;
    }

    return rt_device_control(s_display.lcd, RTGRAPHIC_CTRL_RECT_UPDATE, RT_NULL);
}

const common_display_t *common_display_get(void)
{
    return &s_display;
}