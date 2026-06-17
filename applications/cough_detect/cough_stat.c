/*
 * cough_stat.c — Cough event statistics engine
 *
 * Maintains hourly/daily counters, detects burst episodes,
 * tracks night/day distribution, and logs to SD card.
 */

#include "cough_stat.h"
#include "cough_detect.h"

#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define DBG_TAG "cough.stat"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "../common/common_env.h"
#include "../common/common_storage.h"
#include "../common/common_led.h"
#include "../common/common_config.h"  // @yyc for common_config_get_device_id()

static cough_stat_daily_t  s_daily;
static cough_stat_recent_t s_recent;
static rt_mutex_t s_stat_mutex = RT_NULL;
static rt_timer_t s_midnight_timer = RT_NULL;

/* ── Helpers ────────────────────────────────────────────────────── */

static rt_uint32_t get_unix_timestamp(void)
{
    time_t now = time(RT_NULL);
    return (rt_uint32_t)now;
}

static int get_current_hour(void)
{
    time_t now = time(RT_NULL);
    struct tm *t = localtime(&now);
    return t ? t->tm_hour : 0;
}

static rt_uint32_t get_current_ymd(void)
{
    time_t now = time(RT_NULL);
    struct tm *t = localtime(&now);
    if (t == RT_NULL)
    {
        return 0;
    }
    return (rt_uint32_t)((t->tm_year + 1900) * 10000 +
                         (t->tm_mon + 1) * 100 +
                         t->tm_mday);
}

static rt_bool_t is_nighttime(int hour)
{
    return (hour >= COUGH_STAT_NIGHT_START_H || hour < COUGH_STAT_NIGHT_END_H)
           ? RT_TRUE : RT_FALSE;
}

/* Check how many events in the recent ring buffer fall within the burst window */
static int count_recent_in_window(rt_uint32_t now_ts)
{
    int cnt = 0;
    for (int i = 0; i < s_recent.count; i++)
    {
        int idx = (s_recent.head - 1 - i + COUGH_STAT_RECENT_MAX) % COUGH_STAT_RECENT_MAX;
        if ((now_ts - s_recent.timestamps[idx]) <= COUGH_STAT_BURST_WINDOW_S)
        {
            cnt++;
        }
        else
        {
            break; /* older entries are even further away */
        }
    }
    return cnt;
}

static void recent_push(rt_uint32_t ts)
{
    s_recent.timestamps[s_recent.head] = ts;
    s_recent.head = (s_recent.head + 1) % COUGH_STAT_RECENT_MAX;
    if (s_recent.count < COUGH_STAT_RECENT_MAX)
    {
        s_recent.count++;
    }
}

/* ── Midnight rollover timer ───────────────────────────────────── */
static void midnight_check_callback(void *parameter)
{
    RT_UNUSED(parameter);

    rt_uint32_t ymd = get_current_ymd();
    if (ymd != s_daily.date_ymd && s_daily.date_ymd != 0)
    {
        LOG_I("Date rollover detected: %lu -> %lu", (unsigned long)s_daily.date_ymd,
              (unsigned long)ymd);
        /* Signal control thread to flush; avoid file I/O in timer context */
        cough_detect_send_event(CD_EVENT_STAT_FLUSH);
        cough_stat_reset_daily();
    }
}

/* ── Public API ────────────────────────────────────────────────── */

int cough_stat_init(void)
{
    rt_memset(&s_daily, 0, sizeof(s_daily));
    rt_memset(&s_recent, 0, sizeof(s_recent));

    s_daily.date_ymd = get_current_ymd();

    s_stat_mutex = rt_mutex_create("cg_stat", RT_IPC_FLAG_PRIO);
    if (s_stat_mutex == RT_NULL)
    {
        LOG_E("create stat mutex failed");
        return -RT_ENOMEM;
    }

    /* Check for day rollover every 60 seconds */
    s_midnight_timer = rt_timer_create("stat_mid", midnight_check_callback, RT_NULL,
                                       rt_tick_from_millisecond(60000),
                                       RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
    if (s_midnight_timer != RT_NULL)
    {
        rt_timer_start(s_midnight_timer);
    }

    cough_event_queue_init();  /* @yyc 初始化事件队列 */

    LOG_I("cough statistics engine initialized (date=%lu)", (unsigned long)s_daily.date_ymd);
    return RT_EOK;
}

void cough_stat_record_event(float confidence)
{
    rt_uint32_t ts = get_unix_timestamp();
    int hour = get_current_hour();
    const common_env_sample_t *env;
    rt_bool_t was_burst;
    int recent_count;

    rt_mutex_take(s_stat_mutex, RT_WAITING_FOREVER);

    /* Check date rollover */
    rt_uint32_t ymd = get_current_ymd();
    if (ymd != s_daily.date_ymd && s_daily.date_ymd != 0)
    {
        cough_stat_flush_to_storage();
        cough_stat_reset_daily();
        s_daily.date_ymd = ymd;
    }

    /* Update counters */
    s_daily.total_today++;
    if (hour >= 0 && hour < COUGH_STAT_HOURS_PER_DAY)
    {
        s_daily.hourly[hour]++;
    }

    if (is_nighttime(hour))
    {
        s_daily.night_count++;
    }
    else
    {
        s_daily.day_count++;
    }

    s_daily.last_event_ts = ts;

    /* Read current environment data */
    env = common_env_get_cached();
    if (env != RT_NULL && env->valid)
    {
        s_daily.last_temperature = env->temperature_c;
        s_daily.last_humidity = env->humidity_pct;
    }

    /* Burst detection */
    recent_push(ts);
    was_burst = s_daily.burst_active;
    recent_count = count_recent_in_window(ts);

    if (recent_count >= COUGH_STAT_BURST_THRESHOLD)
    {
        s_daily.burst_active = RT_TRUE;
        if (!was_burst)
        {
            s_daily.burst_count++;
            LOG_W("*** BURST DETECTED! %d coughs in %ds window (episode #%lu) ***",
                  recent_count, COUGH_STAT_BURST_WINDOW_S,
                  (unsigned long)s_daily.burst_count);

            /* Flash LED on burst */
            common_led_set_mode(LED_MODE_BLINK_FAST);
        }
    }
    else
    {
        if (was_burst)
        {
            LOG_I("Burst episode ended");
            common_led_set_mode(LED_MODE_ON);
        }
        s_daily.burst_active = RT_FALSE;
    }

    /* @yyc 将事件加入队列用于批量上传到云端 */
    cough_event_t ev;
    ev.timestamp = ts;
    ev.confidence = confidence;
    ev.temperature_c = s_daily.last_temperature;
    ev.humidity_pct = s_daily.last_humidity;
    cough_event_queue_push(&ev);

    rt_mutex_release(s_stat_mutex);
}

const cough_stat_daily_t *cough_stat_get_daily(void)
{
    return &s_daily;
}

rt_bool_t cough_stat_is_burst_active(void)
{
    return s_daily.burst_active;
}

void cough_stat_reset_daily(void)
{
    rt_mutex_take(s_stat_mutex, RT_WAITING_FOREVER);

    rt_uint32_t ymd = get_current_ymd();
    rt_memset(&s_daily, 0, sizeof(s_daily));
    s_daily.date_ymd = ymd;

    rt_memset(&s_recent, 0, sizeof(s_recent));

    rt_mutex_release(s_stat_mutex);

    LOG_I("Daily statistics reset (new date=%lu)", (unsigned long)ymd);
}

int cough_stat_flush_to_storage(void)
{
    char line[512];
    int len;

    rt_mutex_take(s_stat_mutex, RT_WAITING_FOREVER);

    len = rt_snprintf(line, sizeof(line),
        "%lu,%lu,%lu,%lu,%lu",
        (unsigned long)s_daily.date_ymd,
        (unsigned long)s_daily.total_today,
        (unsigned long)s_daily.day_count,
        (unsigned long)s_daily.night_count,
        (unsigned long)s_daily.burst_count);

    /* Append hourly data */
    for (int h = 0; h < COUGH_STAT_HOURS_PER_DAY && len < (int)sizeof(line) - 10; h++)
    {
        len += rt_snprintf(line + len, sizeof(line) - len, ",%lu",
                           (unsigned long)s_daily.hourly[h]);
    }

    /* Append env data */
    len += rt_snprintf(line + len, sizeof(line) - len, ",%.1f,%.1f\n",
                       s_daily.last_temperature, s_daily.last_humidity);

    rt_mutex_release(s_stat_mutex);

    /* Write to SD card CSV file */
    char path[64];
    rt_snprintf(path, sizeof(path), "%s/daily_stats.csv", COMMON_STORAGE_LOG_DIR);

    return common_storage_append_text(path, line);
}

int cough_stat_to_json(char *buf, rt_size_t size)
{
    int len;
    rt_uint32_t now_ts;  // @yyc
    const char *device_id;  // @yyc
    rt_uint32_t ymd;
    rt_uint32_t total, day_count, night_count, burst_count;
    float last_temp, last_hum;
    rt_uint32_t last_event_ts;
    rt_uint32_t hourly[COUGH_STAT_HOURS_PER_DAY];  // @yyc

    rt_mutex_take(s_stat_mutex, RT_WAITING_FOREVER);

    // @yyc 复制数据避免长时间持锁，并获取device_id和当前时间戳
    now_ts = time(RT_NULL);
    device_id = common_config_get_device_id();
    ymd = s_daily.date_ymd;
    total = s_daily.total_today;
    day_count = s_daily.day_count;
    night_count = s_daily.night_count;
    burst_count = s_daily.burst_count;
    last_temp = s_daily.last_temperature;
    last_hum = s_daily.last_humidity;
    last_event_ts = s_daily.last_event_ts;
    for (int h = 0; h < COUGH_STAT_HOURS_PER_DAY; h++) {
        hourly[h] = s_daily.hourly[h];
    }

    rt_mutex_release(s_stat_mutex);

    // @yyc 添加 device_id 和 ts 字段以匹配云端接口格式
    len = rt_snprintf(buf, size,
        "{"
        "\"device_id\":\"%s\","
        "\"ts\":%lu,"
        "\"date\":%lu,"
        "\"total\":%lu,"
        "\"day\":%lu,"
        "\"night\":%lu,"
        "\"bursts\":%lu,"
        "\"temp\":%.1f,"
        "\"hum\":%.1f,"
        "\"last_ts\":%lu,"
        "\"hourly\":[",
        device_id,
        (unsigned long)now_ts,
        (unsigned long)ymd,
        (unsigned long)total,
        (unsigned long)day_count,
        (unsigned long)night_count,
        (unsigned long)burst_count,
        last_temp,
        last_hum,
        (unsigned long)last_event_ts);

    for (int h = 0; h < COUGH_STAT_HOURS_PER_DAY && len < (int)size - 20; h++)
    {
        len += rt_snprintf(buf + len, size - len, "%s%lu",
                           (h > 0) ? "," : "",
                           (unsigned long)hourly[h]);
    }

    len += rt_snprintf(buf + len, size - len, "]}");

    return len;
}

/* @yyc 新增：事件队列 - 用于批量上传咳嗽事件到云端 */
/* Ring buffer for individual cough events (batch upload) */
#define COUGH_EVENT_QUEUE_SIZE  64
static cough_event_t s_event_queue[COUGH_EVENT_QUEUE_SIZE];  /* @yyc 事件队列 */
static int s_event_queue_head = 0;  /* @yyc 队列头（写入位置） */
static int s_event_queue_tail = 0;  /* @yyc 队列尾（读取位置） */
static int s_event_queue_count = 0;  /* @yyc 当前事件数量 */
static rt_mutex_t s_event_queue_mutex = RT_NULL;  /* @yyc 队列互斥锁 */

void cough_event_queue_init(void)
{
    if (s_event_queue_mutex == RT_NULL)
    {
        s_event_queue_mutex = rt_mutex_create("cough_eq", RT_IPC_FLAG_FIFO);
    }
    s_event_queue_head = 0;
    s_event_queue_tail = 0;
    s_event_queue_count = 0;
    memset(s_event_queue, 0, sizeof(s_event_queue));
}

void cough_event_queue_push(const cough_event_t *event)
{
    if (event == RT_NULL || s_event_queue_mutex == RT_NULL)
        return;

    rt_mutex_take(s_event_queue_mutex, RT_WAITING_FOREVER);
    if (s_event_queue_count < COUGH_EVENT_QUEUE_SIZE)
    {
        s_event_queue[s_event_queue_head] = *event;
        s_event_queue_head = (s_event_queue_head + 1) % COUGH_EVENT_QUEUE_SIZE;
        s_event_queue_count++;
    }
    else
    {
        LOG_W("Event queue full, dropping oldest event");
        s_event_queue_tail = (s_event_queue_tail + 1) % COUGH_EVENT_QUEUE_SIZE;
        s_event_queue[s_event_queue_head] = *event;
        s_event_queue_head = (s_event_queue_head + 1) % COUGH_EVENT_QUEUE_SIZE;
    }
    rt_mutex_release(s_event_queue_mutex);
}

int cough_event_queue_pop_batch(cough_event_t *buf, int len)
{
    int i, count = 0;

    if (buf == RT_NULL || len <= 0 || s_event_queue_mutex == RT_NULL)
        return 0;

    rt_mutex_take(s_event_queue_mutex, RT_WAITING_FOREVER);
    count = (len < s_event_queue_count) ? len : s_event_queue_count;
    for (i = 0; i < count; i++)
    {
        buf[i] = s_event_queue[s_event_queue_tail];
        s_event_queue_tail = (s_event_queue_tail + 1) % COUGH_EVENT_QUEUE_SIZE;
        s_event_queue_count--;
    }
    rt_mutex_release(s_event_queue_mutex);

    return count;
}

int cough_event_queue_size(void)
{
    int count;
    if (s_event_queue_mutex == RT_NULL)
        return 0;
    rt_mutex_take(s_event_queue_mutex, RT_WAITING_FOREVER);
    count = s_event_queue_count;
    rt_mutex_release(s_event_queue_mutex);
    return count;
}

int cough_event_queue_to_json(char *buf, rt_size_t size, int max_events)
{
    int len = 0;
    int i, count;
    cough_event_t event;

    if (buf == RT_NULL || size == 0)
        return -1;

    rt_mutex_take(s_event_queue_mutex, RT_WAITING_FOREVER);
    count = (max_events < s_event_queue_count) ? max_events : s_event_queue_count;

    len = rt_snprintf(buf, size, "[");
    for (i = 0; i < count && len < (int)size - 50; i++)
    {
        int idx = (s_event_queue_tail + i) % COUGH_EVENT_QUEUE_SIZE;
        event = s_event_queue[idx];
        len += rt_snprintf(buf + len, size - len,
                           "%s{\"event_time\":%lu,\"confidence\":%.2f,\"temperature_c\":%.1f,\"humidity_pct\":%.1f}",
                           (i > 0) ? "," : "",
                           (unsigned long)event.timestamp,
                           event.confidence,
                           event.temperature_c,
                           event.humidity_pct);
    }
    if (len < (int)size)
        len += rt_snprintf(buf + len, size - len, "]");

    rt_mutex_release(s_event_queue_mutex);
    return len;
}
