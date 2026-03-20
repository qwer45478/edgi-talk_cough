#ifndef COUGH_UI_H
#define COUGH_UI_H

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

void cough_ui_init(void);
rt_err_t cough_ui_wait_ready(rt_int32_t timeout);

void cough_ui_set_state_text(const char *text);
void cough_ui_push_level(rt_uint16_t level);
void cough_ui_push_cough_event(void);
void cough_ui_push_reminder(const char *label);
void cough_ui_update_env(float temp_c, float hum_pct);
void cough_ui_update_stats(rt_uint32_t total, rt_uint32_t day, rt_uint32_t night,
                           rt_uint32_t bursts);

#ifdef __cplusplus
}
#endif

#endif /* COUGH_UI_H */
