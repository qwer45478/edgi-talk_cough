#ifndef COMMON_KEY_H
#define COMMON_KEY_H

#include <rtthread.h>

typedef enum
{
    COMMON_KEY_EVENT_DOWN = 0,
    COMMON_KEY_EVENT_UP,
    COMMON_KEY_EVENT_CLICK,
    COMMON_KEY_EVENT_LONG_PRESS,
} common_key_event_t;

typedef void (*common_key_callback_t)(common_key_event_t event, void *user_data);

int common_key_init(void);
int common_key_register_callback(common_key_callback_t callback, void *user_data);
rt_base_t common_key_read_level(void);

#endif