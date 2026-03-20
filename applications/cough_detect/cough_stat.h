/*
 * cough_stat.h — Cough event statistics engine
 *
 * Tracks:
 *   - Hourly and daily cough counts
 *   - Burst (rapid) cough detection (N events in T seconds)
 *   - Day/night distribution
 *   - Environment correlation (temperature/humidity at event time)
 *   - Persistent log to SD card
 */

#ifndef COUGH_STAT_H
#define COUGH_STAT_H

#include <rtthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration ─────────────────────────────────────────────── */
#define COUGH_STAT_HOURS_PER_DAY    24
#define COUGH_STAT_BURST_WINDOW_S   60    /* burst = N coughs in 60s     */
#define COUGH_STAT_BURST_THRESHOLD  5     /* 5 coughs/min = burst        */
#define COUGH_STAT_NIGHT_START_H    22    /* 22:00 = night start         */
#define COUGH_STAT_NIGHT_END_H      6    /* 06:00 = night end           */

/* ── Data structures ───────────────────────────────────────────── */
typedef struct
{
    rt_uint32_t timestamp;          /* unix timestamp of event */
    float       confidence;         /* model confidence score  */
    float       temperature_c;      /* env temp at event time  */
    float       humidity_pct;       /* env humidity at event    */
} cough_event_t;

typedef struct
{
    rt_uint32_t total_today;                        /* total coughs today       */
    rt_uint32_t hourly[COUGH_STAT_HOURS_PER_DAY];  /* per-hour distribution    */
    rt_uint32_t day_count;                          /* daytime (06:00-22:00)    */
    rt_uint32_t night_count;                        /* nighttime (22:00-06:00)  */
    rt_uint32_t burst_count;                        /* number of burst episodes */
    rt_bool_t   burst_active;                       /* currently in a burst     */
    rt_uint32_t last_event_ts;                      /* timestamp of last event  */
    float       last_temperature;
    float       last_humidity;
    rt_uint32_t date_ymd;           /* YYYYMMDD of current day stats */
} cough_stat_daily_t;

/* Ring buffer for burst detection */
#define COUGH_STAT_RECENT_MAX   32
typedef struct
{
    rt_uint32_t timestamps[COUGH_STAT_RECENT_MAX];
    int         head;
    int         count;
} cough_stat_recent_t;

/* ── Public API ────────────────────────────────────────────────── */

/**
 * Initialize the statistics engine. Call once at startup after RTC is ready.
 */
int cough_stat_init(void);

/**
 * Record a new cough event. Called from inference thread when cough detected.
 * @param confidence  Model confidence score (0..1)
 */
void cough_stat_record_event(float confidence);

/**
 * Get current daily statistics (read-only snapshot).
 */
const cough_stat_daily_t *cough_stat_get_daily(void);

/**
 * Check if a burst is currently active.
 */
rt_bool_t cough_stat_is_burst_active(void);

/**
 * Reset daily statistics (called automatically at midnight, or manually).
 */
void cough_stat_reset_daily(void);

/**
 * Flush current statistics to SD card log file.
 * Format: CSV line with timestamp, hourly counts, totals, env data.
 */
int cough_stat_flush_to_storage(void);

/**
 * Build a JSON summary string for cloud upload.
 * @param buf    Output buffer
 * @param size   Buffer size
 * @return Number of bytes written (excluding null terminator)
 */
int cough_stat_to_json(char *buf, rt_size_t size);

#ifdef __cplusplus
}
#endif

#endif /* COUGH_STAT_H */
