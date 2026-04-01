#ifndef COMMON_STORAGE_H
#define COMMON_STORAGE_H

#include <rtthread.h>

#define COMMON_STORAGE_BASE_PATH    "/sdcard"
#define COMMON_STORAGE_LOG_DIR      "/sdcard/cough_log"
#define COMMON_STORAGE_AUDIO_DIR    "/sdcard/cough_audio"
#define COMMON_STORAGE_CACHE_DIR    "/sdcard/cough_cache"
#define COMMON_STORAGE_SETTINGS_PATH "/sdcard/cough_cache/settings.dat"

/* Minimum free space to keep on SD card (2 MB) */
#define COMMON_STORAGE_MIN_FREE     (2 * 1024 * 1024)
/* Max event log file size (1 MB) — older daily files beyond 7 days are deleted */
#define COMMON_STORAGE_LOG_MAX_DAYS 7

int common_storage_init(void);

/* Text file operations */
int common_storage_append_text(const char *path, const char *text);
int common_storage_write_text(const char *path, const char *text);

/* Binary file operations */
int common_storage_write_binary(const char *path, const void *data, rt_size_t size);
int common_storage_append_binary(const char *path, const void *data, rt_size_t size);
int common_storage_read_binary(const char *path, void *buf, rt_size_t size, rt_size_t *read_size);

/* WAV file helpers */
int common_storage_wav_create(const char *path, rt_uint32_t sample_rate,
                              rt_uint16_t bits_per_sample, rt_uint16_t channels);
int common_storage_wav_append(const char *path, const void *pcm_data, rt_size_t bytes);
int common_storage_wav_finalize(const char *path);

/* Directory management */
int common_storage_ensure_dirs(void);
rt_bool_t common_storage_is_mounted(void);

/**
 * Ensure at least `need_bytes` + COMMON_STORAGE_MIN_FREE bytes are free.
 * Deletes oldest files in cough_audio/ if necessary.
 */
void common_storage_ensure_space(uint32_t need_bytes);

/**
 * Append a timestamped event line to today's log file.
 * File: /sdcard/cough_log/event_YYYYMMDD.txt
 * Old log files (>7 days) are cleaned up automatically.
 */
void common_storage_log_event(const char *fmt, ...);

/**
 * Clean up old daily event log files (keep latest N days).
 */
void common_storage_log_rotate(void);

#endif