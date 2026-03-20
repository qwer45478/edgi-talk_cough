#ifndef COMMON_POWER_H
#define COMMON_POWER_H

#include <rtthread.h>

int common_power_init(void);
int common_power_set_wifi(rt_bool_t enabled);
int common_power_set_audio(rt_bool_t enabled);
int common_power_set_speaker(rt_bool_t enabled);
int common_power_set_display(rt_bool_t enabled);
int common_power_set_backlight(rt_bool_t enabled);
int common_power_set_backlight_percent(rt_uint8_t percent);
int common_power_poweroff(void);

#endif