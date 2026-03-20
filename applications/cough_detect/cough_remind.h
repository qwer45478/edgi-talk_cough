/*
 * cough_remind.h — Medication and observation reminder system
 *
 * Supports up to 8 daily reminder slots with configurable times.
 * Uses RTC for scheduling and speaker for audio alerts.
 */

#ifndef COUGH_REMIND_H
#define COUGH_REMIND_H

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COUGH_REMIND_MAX_SLOTS  8

typedef struct
{
    rt_uint8_t  hour;       /* 0-23 */
    rt_uint8_t  minute;     /* 0-59 */
    rt_bool_t   enabled;
    rt_bool_t   triggered;  /* already triggered today */
    char        label[24];  /* e.g. "Morning Medicine" */
} cough_remind_slot_t;

typedef void (*cough_remind_callback_t)(int slot_index, const cough_remind_slot_t *slot,
                                        void *user_data);

int cough_remind_init(void);

/**
 * Set a reminder slot.
 * @param index   Slot index (0 .. COUGH_REMIND_MAX_SLOTS-1)
 * @param hour    Hour (0-23)
 * @param minute  Minute (0-59)
 * @param label   Human-readable label (or NULL)
 */
int cough_remind_set(int index, rt_uint8_t hour, rt_uint8_t minute, const char *label);

/**
 * Enable or disable a reminder slot.
 */
int cough_remind_enable(int index, rt_bool_t enabled);

/**
 * Register callback for when a reminder fires.
 */
int cough_remind_register_callback(cough_remind_callback_t cb, void *user_data);

/**
 * Get a reminder slot configuration.
 */
const cough_remind_slot_t *cough_remind_get_slot(int index);

/**
 * Get the next upcoming reminder (hours and minutes until).
 * @param out_index  Output: slot index of next reminder
 * @return minutes until next reminder, or -1 if none
 */
int cough_remind_get_next(int *out_index);

/**
 * Reset all "triggered" flags (called at midnight).
 */
void cough_remind_reset_daily(void);

/**
 * Execute pending reminder alerts (melody, LED, callback).
 * Called from a worker thread context, NOT from timer interrupt.
 */
void cough_remind_do_alert(void);

#ifdef __cplusplus
}
#endif

#endif /* COUGH_REMIND_H */
