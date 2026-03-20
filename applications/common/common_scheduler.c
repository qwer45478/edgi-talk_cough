#include "common_scheduler.h"

#define DBG_TAG "common.sched"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

int common_scheduler_init(void)
{
    LOG_I("scheduler service initialized");
    return RT_EOK;
}

rt_timer_t common_scheduler_create_periodic(const char *name,
                                            rt_tick_t period,
                                            common_scheduler_callback_t callback,
                                            void *parameter)
{
    return rt_timer_create(name,
                           callback,
                           parameter,
                           period,
                           RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
}

int common_scheduler_start(rt_timer_t timer)
{
    if (timer == RT_NULL)
    {
        return -RT_EINVAL;
    }
    return rt_timer_start(timer);
}

int common_scheduler_stop(rt_timer_t timer)
{
    if (timer == RT_NULL)
    {
        return -RT_EINVAL;
    }
    return rt_timer_stop(timer);
}