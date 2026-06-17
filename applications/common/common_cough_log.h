/*
 * common_cough_log.h - Cough event log
 *
 * Stores timestamp + day/night flag for each cough event.
 * Uses a simple in-memory circular buffer (100 entries).
 */

#ifndef COMMON_COUGH_LOG_H
#define COMMON_COUGH_LOG_H

#include <rtthread.h>
#include <time.h>

#define COUGH_LOG_MAX  100  /* Max number of log entries */

typedef struct
{
    time_t timestamp;       /* Unix timestamp */
    rt_bool_t is_day;       /* RT_TRUE = day, RT_FALSE = night */
} cough_log_entry_t;

/**
 * Initialize the cough log module.
 * Called automatically at startup.
 * Returns RT_EOK on success, negative on failure.
 */
int common_cough_log_init(void);

/**
 * Add a new cough event log entry.
 * Should be called when a cough is detected.
 */
void common_cough_log_add(rt_bool_t is_day);

/**
 * Get the number of logged cough events.
 */
int common_cough_log_count(void);

/**
 * Get recent cough log entries.
 * Returns the number of entries actually copied (up to max_cnt).
 * Entries are ordered from newest to oldest.
 *
 * entries:      array to copy entries into
 * max_cnt:      max number of entries to copy
 * start_index:  start index (0 = most recent)
 */
int common_cough_log_get_recent(cough_log_entry_t *entries, int max_cnt, int start_index);

#endif /* COMMON_COUGH_LOG_H */
