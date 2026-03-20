#ifndef APP_COMMON_H
#define APP_COMMON_H

#include <rtthread.h>

enum
{
    APP_COMMON_INIT_POWER           = (1u << 0),
    APP_COMMON_INIT_KEY             = (1u << 1),
    APP_COMMON_INIT_AUDIO_CAPTURE   = (1u << 2),
    APP_COMMON_INIT_AUDIO_PLAYBACK  = (1u << 3),
    APP_COMMON_INIT_DISPLAY         = (1u << 4),
    APP_COMMON_INIT_NETWORK         = (1u << 5),
    APP_COMMON_INIT_STORAGE         = (1u << 6),
    APP_COMMON_INIT_SCHEDULER       = (1u << 7),
    APP_COMMON_INIT_ENV             = (1u << 8),
    APP_COMMON_INIT_LED             = (1u << 9),
};

#define APP_COMMON_INIT_BASE \
    (APP_COMMON_INIT_POWER | APP_COMMON_INIT_AUDIO_PLAYBACK | \
     APP_COMMON_INIT_DISPLAY | APP_COMMON_INIT_NETWORK | \
     APP_COMMON_INIT_STORAGE | APP_COMMON_INIT_SCHEDULER | \
     APP_COMMON_INIT_ENV | APP_COMMON_INIT_LED)

typedef struct
{
    rt_uint32_t init_mask;
    rt_uint32_t ready_mask;
} app_common_status_t;

int app_common_init(rt_uint32_t init_mask);
void app_common_dump_status(void);
const app_common_status_t *app_common_get_status(void);

#endif