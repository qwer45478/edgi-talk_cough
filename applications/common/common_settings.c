/*
 * common_settings.c — User settings persistence to SD card
 */

#include "common_settings.h"
#include "common_storage.h"

#include <string.h>

#define DBG_TAG "common.cfg"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

void common_settings_defaults(common_settings_t *out)
{
    rt_memset(out, 0, sizeof(*out));
    out->magic          = COMMON_SETTINGS_MAGIC;
    out->version        = COMMON_SETTINGS_VERSION;
    out->threshold_pct  = 35;   /* 0.35 */
    out->brightness_pct = 90;
    out->auto_upload    = RT_TRUE;
    out->reminder_count = 3;

    /* Default reminder slots */
    out->reminders[0].hour    = 8;
    out->reminders[0].minute  = 0;
    out->reminders[0].enabled = RT_TRUE;
    rt_strncpy(out->reminders[0].label, "Morning Med", sizeof(out->reminders[0].label) - 1);

    out->reminders[1].hour    = 12;
    out->reminders[1].minute  = 0;
    out->reminders[1].enabled = RT_TRUE;
    rt_strncpy(out->reminders[1].label, "Noon Med", sizeof(out->reminders[1].label) - 1);

    out->reminders[2].hour    = 20;
    out->reminders[2].minute  = 0;
    out->reminders[2].enabled = RT_TRUE;
    rt_strncpy(out->reminders[2].label, "Evening Med", sizeof(out->reminders[2].label) - 1);
}

int common_settings_load(common_settings_t *out)
{
    rt_size_t read_sz = 0;
    int ret;

    if (out == RT_NULL)
        return -RT_EINVAL;

    ret = common_storage_read_binary(COMMON_STORAGE_SETTINGS_PATH,
                                     out, sizeof(*out), &read_sz);
    if (ret != RT_EOK || read_sz != sizeof(*out))
    {
        LOG_W("Settings file missing or truncated, using defaults");
        common_settings_defaults(out);
        return -RT_ERROR;
    }

    if (out->magic != COMMON_SETTINGS_MAGIC || out->version != COMMON_SETTINGS_VERSION)
    {
        LOG_W("Settings magic/version mismatch, using defaults");
        common_settings_defaults(out);
        return -RT_ERROR;
    }

    LOG_I("Settings loaded: thr=%d bri=%d upload=%d reminders=%d",
          out->threshold_pct, out->brightness_pct,
          out->auto_upload, out->reminder_count);
    return RT_EOK;
}

int common_settings_save(const common_settings_t *settings)
{
    if (settings == RT_NULL)
        return -RT_EINVAL;

    common_storage_ensure_dirs();

    int ret = common_storage_write_binary(COMMON_STORAGE_SETTINGS_PATH,
                                          settings, sizeof(*settings));
    if (ret == RT_EOK)
    {
        LOG_I("Settings saved to SD card");
    }
    else
    {
        LOG_E("Failed to save settings");
    }
    return ret;
}
