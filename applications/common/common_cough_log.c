/*
 * common_cough_log.c - Cough event log implementation
 */

#include "common_cough_log.h"
#include "common_network.h"

#include <rtthread.h>
#include <string.h>

#define DBG_TAG    "cough.log"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

/* Circular buffer */
static cough_log_entry_t s_log[COUGH_LOG_MAX];
static int s_head = 0;       /* Next write position */
static int s_count = 0;     /* Current number of entries */
static struct rt_mutex s_lock;

/* Helper: check if it's daytime (6:00 - 18:00) */
static rt_bool_t is_day_time(void)
{
    time_t now = time((time_t *)RT_NULL);
    if (now <= 0)
        return RT_TRUE;  /* Default to day if time not set */

    struct tm *t = localtime(&now);
    if (t->tm_hour >= 6 && t->tm_hour < 18)
        return RT_TRUE;
    return RT_FALSE;
}

int common_cough_log_init(void)
{
    rt_mutex_init(&s_lock, "cough_log", RT_IPC_FLAG_FIFO);
    LOG_I("Cough log initialized (max %d entries)", COUGH_LOG_MAX);
    return RT_EOK;
}

void common_cough_log_add(rt_bool_t is_day)
{
    rt_bool_t is_actually_day = is_day_time();  // @yyc edit

    rt_mutex_take(&s_lock, RT_WAITING_FOREVER);

    s_log[s_head].timestamp = time((time_t *)RT_NULL);
    s_log[s_head].is_day = is_actually_day;

    s_head = (s_head + 1) % COUGH_LOG_MAX;
    if (s_count < COUGH_LOG_MAX)
        s_count++;

    rt_mutex_release(&s_lock);

    LOG_D("Cough logged: %s", is_actually_day ? "DAY" : "NIGHT");
}

int common_cough_log_count(void)
{
    rt_mutex_take(&s_lock, RT_WAITING_FOREVER);
    int cnt = s_count;
    rt_mutex_release(&s_lock);
    return cnt;
}

int common_cough_log_get_recent(cough_log_entry_t *entries, int max_cnt, int start_index)
{
    if (!entries || max_cnt <= 0)
        return 0;

    rt_mutex_take(&s_lock, RT_WAITING_FOREVER);

    if (start_index >= s_count)
    {
        rt_mutex_release(&s_lock);
        return 0;
    }

    int available = s_count - start_index;
    int copy_cnt = (available < max_cnt) ? available : max_cnt;

    for (int i = 0; i < copy_cnt; i++)
    {
        /* Calculate actual index in buffer */
        int idx = (s_head - 1 - start_index - i + COUGH_LOG_MAX * 2) % COUGH_LOG_MAX;
        entries[i] = s_log[idx];
    }

    rt_mutex_release(&s_lock);

    return copy_cnt;
}
