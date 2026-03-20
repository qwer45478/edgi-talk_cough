#include "common_power.h"

#include <board.h>

#define DBG_TAG "common.pwr"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

int common_power_init(void)
{
    int result = edgi_board_power_init();
    if (result == RT_EOK)
    {
        LOG_I("power domains initialized");
    }
    return result;
}

int common_power_set_wifi(rt_bool_t enabled)
{
    return edgi_board_set_power_domain(EDGI_BOARD_POWER_WIFI, enabled);
}

int common_power_set_audio(rt_bool_t enabled)
{
    return edgi_board_set_power_domain(EDGI_BOARD_POWER_AUDIO_CODEC, enabled);
}

int common_power_set_speaker(rt_bool_t enabled)
{
    return edgi_board_set_power_domain(EDGI_BOARD_POWER_SPEAKER, enabled);
}

int common_power_set_display(rt_bool_t enabled)
{
    return edgi_board_set_power_domain(EDGI_BOARD_POWER_DISPLAY, enabled);
}

int common_power_set_backlight(rt_bool_t enabled)
{
    return edgi_board_set_power_domain(EDGI_BOARD_POWER_DISPLAY_BACKLIGHT, enabled);
}

int common_power_set_backlight_percent(rt_uint8_t percent)
{
    return edgi_board_set_display_brightness(percent);
}

int common_power_poweroff(void)
{
    return edgi_board_poweroff_system();
}