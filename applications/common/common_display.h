#ifndef COMMON_DISPLAY_H
#define COMMON_DISPLAY_H

#include <rtthread.h>

typedef struct
{
    rt_device_t lcd;
    rt_bool_t is_ready;
    rt_uint8_t brightness;
} common_display_t;

int common_display_init(void);
int common_display_set_brightness(rt_uint8_t percent);
int common_display_set_backlight(rt_bool_t enabled);
int common_display_refresh_full(void);
const common_display_t *common_display_get(void);

#endif