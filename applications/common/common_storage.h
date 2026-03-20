#ifndef COMMON_STORAGE_H
#define COMMON_STORAGE_H

#include <rtthread.h>

#define COMMON_STORAGE_BASE_PATH    "/sdcard"
#define COMMON_STORAGE_LOG_DIR      "/sdcard/cough_log"
#define COMMON_STORAGE_AUDIO_DIR    "/sdcard/cough_audio"
#define COMMON_STORAGE_CACHE_DIR    "/sdcard/cough_cache"

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

#endif