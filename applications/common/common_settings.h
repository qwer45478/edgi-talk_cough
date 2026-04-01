/*
 * common_settings.h — User settings persistence to SD card
 *
 * Stores user-configurable parameters (threshold, brightness,
 * reminder slots, upload toggle) in a binary file on SD card.
 */

#ifndef COMMON_SETTINGS_H
#define COMMON_SETTINGS_H

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COMMON_SETTINGS_MAGIC   0x43535430  /* "CST0" */
#define COMMON_SETTINGS_VERSION 1

#define COMMON_SETTINGS_MAX_REMINDERS 8

typedef struct
{
    rt_uint8_t  hour;
    rt_uint8_t  minute;
    rt_bool_t   enabled;
    char        label[24];
} settings_reminder_t;

typedef struct
{
    rt_uint32_t magic;
    rt_uint32_t version;
    /* Detection */
    rt_uint8_t  threshold_pct;      /* 20..80 → represents 0.20..0.80 */
    /* Display */
    rt_uint8_t  brightness_pct;     /* 10..100 */
    /* Cloud */
    rt_bool_t   auto_upload;
    /* Reminders */
    rt_uint8_t  reminder_count;
    settings_reminder_t reminders[COMMON_SETTINGS_MAX_REMINDERS];
} common_settings_t;

/**
 * Load settings from SD card.  If file is missing or corrupt,
 * fills `out` with defaults and returns -RT_ERROR.
 */
int common_settings_load(common_settings_t *out);

/**
 * Save settings to SD card.
 */
int common_settings_save(const common_settings_t *settings);

/**
 * Fill a settings struct with factory defaults.
 */
void common_settings_defaults(common_settings_t *out);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_SETTINGS_H */
