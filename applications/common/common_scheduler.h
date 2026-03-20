#ifndef COMMON_SCHEDULER_H
#define COMMON_SCHEDULER_H

#include <rtthread.h>

typedef void (*common_scheduler_callback_t)(void *parameter);

int common_scheduler_init(void);
rt_timer_t common_scheduler_create_periodic(const char *name,
                                            rt_tick_t period,
                                            common_scheduler_callback_t callback,
                                            void *parameter);
int common_scheduler_start(rt_timer_t timer);
int common_scheduler_stop(rt_timer_t timer);

#endif