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

static cough_remind_slot_t s_slots[COUGH_REMIND_MAX_SLOTS];
static cough_remind_callback_t s_callback = RT_NULL;
static void *s_callback_user_data = RT_NULL;
static rt_timer_t s_check_timer = RT_NULL;
static volatile rt_int8_t s_pending_slot = -1;  /* slot index awaiting alert, or -1 */

/* Alert melody: C5 E5 G5 C6 */
static const rt_uint32_t s_alert_notes[]    = { 523, 659, 784, 1047 };
static const rt_uint32_t s_alert_durations[] = { 200, 200, 200, 400 };
#define ALERT_NOTE_COUNT  4

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

    /* Set default reminders — can be reconfigured via cloud or MSH */
    cough_remind_set(0, 8,  0,  "Morning Med");
    cough_remind_set(1, 12, 0,  "Noon Med");
    cough_remind_set(2, 20, 0,  "Evening Med");

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

    return RT_EOK;
}

int cough_remind_enable(int index, rt_bool_t enabled)
{
    if (index < 0 || index >= COUGH_REMIND_MAX_SLOTS)
    {
        return -RT_EINVAL;
    }

    s_slots[index].enabled = enabled;
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
