/*
 * cough_remind.c — Medication and observation reminder system
 *
 * Checks RTC every 30 seconds. When current time matches a slot,
 * plays an alert tone and notifies the UI via callback.
 */

#include "cough_remind.h"
#include "cough_detect.h"

#include <rtthread.h>
#include <string.h>
#include <time.h>

#define DBG_TAG "cough.remind"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "../common/common_audio_playback.h"
#include "../common/common_led.h"
#include "../common/common_config.h"

static cough_remind_slot_t s_slots[COUGH_REMIND_MAX_SLOTS];
static cough_remind_callback_t s_callback = RT_NULL;
static void *s_callback_user_data = RT_NULL;
static rt_timer_t s_check_timer = RT_NULL;
static volatile rt_int8_t s_pending_slot = -1;  /* slot index awaiting alert, or -1 */

/* @yyc Friendly alert melody: Ding-Dong style, repeated 3 times
 * Pattern: High tone (Ding) → Low tone (Dong) → short pause
 * Frequencies: G5(784) → E5(659) → G5(784) → C6(1047)
 * This is more noticeable than a simple ascending arpeggio.
 */
static const rt_uint32_t s_alert_notes[]    = { 784, 659, 784, 1047,
                                                784, 659, 784, 1047,
                                                784, 659, 784, 1047 };
static const rt_uint32_t s_alert_durations[] = { 150, 100, 150, 300,
                                                 150, 100, 150, 300,
                                                 150, 100, 150, 300 };
#define ALERT_NOTE_COUNT  12

static void remind_check_callback(void *parameter)
{
    time_t now = time(RT_NULL);
    struct tm *t = localtime(&now);
    RT_UNUSED(parameter);

    if (t == RT_NULL)
    {
        return;
    }

    for (int i = 0; i < COUGH_REMIND_MAX_SLOTS; i++)
    {
        if (!s_slots[i].enabled || s_slots[i].triggered)
        {
            continue;
        }

        if (t->tm_hour == s_slots[i].hour && t->tm_min == s_slots[i].minute)
        {
            s_slots[i].triggered = RT_TRUE;
            s_pending_slot = (rt_int8_t)i;

            /* Signal control thread to do the heavy work (melody, LED, callback) */
            cough_detect_send_event(CD_EVENT_REMIND_FIRE);
            break;  /* one alert per tick */
        }
    }

    /* Reset triggered flags at midnight */
    if (t->tm_hour == 0 && t->tm_min == 0)
    {
        cough_remind_reset_daily();
    }
}

void cough_remind_do_alert(void)
{
    rt_int8_t idx = s_pending_slot;
    if (idx < 0 || idx >= COUGH_REMIND_MAX_SLOTS)
        return;
    s_pending_slot = -1;

    LOG_I("Reminder #%d fired: %s (%02d:%02d)",
          idx, s_slots[idx].label, s_slots[idx].hour, s_slots[idx].minute);

    /* Play alert melody */
    common_audio_playback_melody(s_alert_notes, s_alert_durations,
                                 ALERT_NOTE_COUNT);

    /* Flash LED */
    common_led_set_mode(LED_MODE_BLINK_FAST);

    /* Notify UI */
    if (s_callback != RT_NULL)
    {
        s_callback(idx, &s_slots[idx], s_callback_user_data);
    }
}

int cough_remind_init(void)
{
    rt_memset(s_slots, 0, sizeof(s_slots));

    /* @yyc edit: 禁用默认提醒设置，改为手动从云端或本地配置启用
     * 如需启用默认提醒，请取消注释以下三行：
     * cough_remind_set(0, 8,  0,  "Morning Med");
     * cough_remind_set(1, 12, 0,  "Noon Med");
     * cough_remind_set(2, 20, 0,  "Evening Med");
     */

    /* Check every 30 seconds */
    s_check_timer = rt_timer_create("remind", remind_check_callback, RT_NULL,
                                    rt_tick_from_millisecond(30000),
                                    RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
    if (s_check_timer == RT_NULL)
    {
        LOG_E("create remind timer failed");
        return -RT_ENOMEM;
    }

    rt_timer_start(s_check_timer);
    LOG_I("reminder system initialized (%d slots)", COUGH_REMIND_MAX_SLOTS);
    return RT_EOK;
}

int cough_remind_set(int index, rt_uint8_t hour, rt_uint8_t minute, const char *label)
{
    if (index < 0 || index >= COUGH_REMIND_MAX_SLOTS)
    {
        return -RT_EINVAL;
    }
    if (hour > 23 || minute > 59)
    {
        return -RT_EINVAL;
    }

    s_slots[index].hour     = hour;
    s_slots[index].minute   = minute;
    s_slots[index].enabled  = RT_TRUE;
    s_slots[index].triggered = RT_FALSE;

    if (label != RT_NULL)
    {
        rt_strncpy(s_slots[index].label, label, sizeof(s_slots[index].label) - 1);
    }

    /* Persist to NOR Flash */
    common_config_save_remind(index);

    return RT_EOK;
}

int cough_remind_enable(int index, rt_bool_t enabled)
{
    if (index < 0 || index >= COUGH_REMIND_MAX_SLOTS)
    {
        return -RT_EINVAL;
    }

    s_slots[index].enabled = enabled;

    /* Persist to NOR Flash */
    common_config_save_remind(index);

    return RT_EOK;
}

int cough_remind_register_callback(cough_remind_callback_t cb, void *user_data)
{
    s_callback = cb;
    s_callback_user_data = user_data;
    return RT_EOK;
}

const cough_remind_slot_t *cough_remind_get_slot(int index)
{
    if (index < 0 || index >= COUGH_REMIND_MAX_SLOTS)
    {
        return RT_NULL;
    }
    return &s_slots[index];
}

int cough_remind_get_next(int *out_index)
{
    time_t now = time(RT_NULL);
    struct tm *t = localtime(&now);
    int best_min = -1;
    int best_idx = -1;
    int cur_minutes;

    if (t == RT_NULL)
    {
        return -1;
    }

    cur_minutes = t->tm_hour * 60 + t->tm_min;

    for (int i = 0; i < COUGH_REMIND_MAX_SLOTS; i++)
    {
        int slot_min;
        int diff;

        if (!s_slots[i].enabled || s_slots[i].triggered)
        {
            continue;
        }

        slot_min = s_slots[i].hour * 60 + s_slots[i].minute;
        diff = slot_min - cur_minutes;
        if (diff < 0)
        {
            diff += 24 * 60;  /* wrap to next day */
        }

        if (best_min < 0 || diff < best_min)
        {
            best_min = diff;
            best_idx = i;
        }
    }

    if (out_index != RT_NULL)
    {
        *out_index = best_idx;
    }
    return best_min;
}

void cough_remind_reset_daily(void)
{
    for (int i = 0; i < COUGH_REMIND_MAX_SLOTS; i++)
    {
        s_slots[i].triggered = RT_FALSE;
    }
    LOG_I("reminder daily flags reset");
}

int cough_remind_to_json(char *json_buf, rt_size_t buf_size)
{
    if (json_buf == RT_NULL || buf_size == 0)
    {
        return -RT_EINVAL;
    }

    char *p = json_buf;
    rt_size_t left = buf_size;
    int written;

    written = rt_snprintf(p, left, "{\"slots\":[");
    if (written < 0 || (rt_size_t)written >= left)
    {
        return -RT_ENOMEM;
    }
    p += written;
    left -= written;

    for (int i = 0; i < COUGH_REMIND_MAX_SLOTS; i++)
    {
        written = rt_snprintf(p, left,
            "%s{\"slot_index\":%d,\"hour\":%d,\"minute\":%d,\"enabled\":%s,\"label\":\"%s\"}",
            (i > 0) ? "," : "",
            i,
            s_slots[i].hour,
            s_slots[i].minute,
            s_slots[i].enabled ? "true" : "false",
            s_slots[i].label);
        if (written < 0 || (rt_size_t)written >= left)
        {
            return -RT_ENOMEM;
        }
        p += written;
        left -= written;
    }

    written = rt_snprintf(p, left, "]}");
    if (written < 0 || (rt_size_t)written >= left)
    {
        return -RT_ENOMEM;
    }
    p += written;
    left -= written;

    return (int)(p - json_buf);
}

static const char *_json_skip_space(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
    {
        p++;
    }
    return p;
}

static const char *_json_find_field(const char *json, const char *field)
{
    char pattern[64];
    const char *p;
    rt_snprintf(pattern, sizeof(pattern), "\"%s\"", field);
    p = strstr(json, pattern);
    return p;
}

static int _json_parse_int(const char *json, const char *field, int *out)
{
    const char *p = _json_find_field(json, field);
    if (p == RT_NULL)
    {
        return -1;
    }
    p = strchr(p, ':');
    if (p == RT_NULL)
    {
        return -1;
    }
    p++;
    p = _json_skip_space(p);

    /* Parse number */
    int val = 0;
    int neg = 0;
    if (*p == '-')
    {
        neg = 1;
        p++;
    }
    while (*p >= '0' && *p <= '9')
    {
        val = val * 10 + (*p - '0');
        p++;
    }
    *out = neg ? -val : val;
    return 0;
}

static int _json_parse_bool(const char *json, const char *field)
{
    const char *p = _json_find_field(json, field);
    if (p == RT_NULL)
    {
        return -1;
    }
    p = strchr(p, ':');
    if (p == RT_NULL)
    {
        return -1;
    }
    p++;
    p = _json_skip_space(p);

    if (strncmp(p, "true", 4) == 0)
    {
        return 1;
    }
    return 0;
}

static int _json_parse_string(const char *json, const char *field, char *out, rt_size_t out_size)
{
    const char *p = _json_find_field(json, field);
    if (p == RT_NULL)
    {
        return -1;
    }
    p = strchr(p, ':');
    if (p == RT_NULL)
    {
        return -1;
    }
    p++;
    p = _json_skip_space(p);

    if (*p != '"')
    {
        return -1;
    }
    p++;
    const char *start = p;
    const char *end = strchr(p, '"');
    if (end == RT_NULL)
    {
        return -1;
    }
    rt_size_t len = (rt_size_t)(end - start);
    if (len >= out_size)
    {
        len = out_size - 1;
    }
    strncpy(out, start, len);
    out[len] = '\0';
    return (int)len;
}

int cough_remind_from_json(const char *json)
{
    int slot_index, hour, minute, enabled;
    char label[24];
    int ret;

    if (json == RT_NULL)
    {
        return -RT_EINVAL;
    }

    /* @yyc Merge strategy: Load from flash first to preserve local-only reminders,
     * then apply cloud data on top (cloud wins for slots it provides).
     */
    common_config_load_remind_all(s_slots, COUGH_REMIND_MAX_SLOTS);

    /* Find "slots" array */
    const char *slots_p = strstr(json, "\"slots\"");
    if (slots_p == RT_NULL)
    {
        return -RT_ERROR;
    }
    slots_p = strchr(slots_p, '[');
    if (slots_p == RT_NULL)
    {
        return -RT_ERROR;
    }
    slots_p++;

    for (int i = 0; i < COUGH_REMIND_MAX_SLOTS; i++)
    {
        /* Find next slot object or end */
        const char *obj_p = strchr(slots_p, '{');
        const char *arr_end = strchr(slots_p, ']');
        if (obj_p == RT_NULL || (arr_end != RT_NULL && obj_p > arr_end))
        {
            break;
        }

        ret = _json_parse_int(obj_p, "slot_index", &slot_index);
        if (ret != 0)
        {
            slot_index = i;
        }
        _json_parse_int(obj_p, "hour", &hour);
        _json_parse_int(obj_p, "minute", &minute);
        enabled = _json_parse_bool(obj_p, "enabled");
        _json_parse_string(obj_p, "label", label, sizeof(label));

        if (slot_index >= 0 && slot_index < COUGH_REMIND_MAX_SLOTS)
        {
            cough_remind_set(slot_index, (rt_uint8_t)hour, (rt_uint8_t)minute,
                           enabled > 0 ? label : "");
            if (enabled <= 0)
            {
                cough_remind_enable(slot_index, RT_FALSE);
            }
        }

        /* Move to next slot */
        slots_p = strchr(obj_p, '}');
        if (slots_p == RT_NULL)
        {
            break;
        }
        slots_p++;
    }

    LOG_I("Reminders applied from JSON");
    return RT_EOK;
}

void cough_remind_apply_defaults(void)
{
    /* @yyc Default reminders - safe fallback when cloud is unavailable */
    rt_memset(s_slots, 0, sizeof(s_slots));

    cough_remind_set(0, 8,  0, "Morning Medicine");
    cough_remind_enable(0, RT_FALSE);  /* Disabled by default */

    LOG_I("Default reminder config applied (all disabled)");
}

const cough_remind_slot_t *cough_remind_get_all_slots(void)
{
    return s_slots;
}

