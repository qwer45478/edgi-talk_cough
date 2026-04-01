/*
 * cough_ui_pages.h — Internal header shared between UI page modules
 */
#ifndef COUGH_UI_PAGES_H
#define COUGH_UI_PAGES_H

#include <lvgl.h>
#include <rtthread.h>
#include "cough_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Display dimensions ────────────────────────────────────────── */
#define SCREEN_W  512
#define SCREEN_H  800
#define SAFE_R    34        /* right bezel inset                     */
#define CONTENT_W (SCREEN_W - 12 - SAFE_R)   /* 466 usable width   */
#define HEADER_H  56
#define NAVBAR_H  60
#define PAGE_H    (SCREEN_H - HEADER_H)      /* 744 content height  */

/* ── Color palette ──────────────────────────────────────────────── */
#define CLR_BG_DARK         0x1A1A2E
#define CLR_PANEL_BG        0x16213E
#define CLR_PANEL_BORDER    0x2A2A4A
#define CLR_HEADER_BG       0x4f46e5
#define CLR_TEXT_WHITE      0xffffff
#define CLR_TEXT_MUTED      0xA0A0B8
#define CLR_ACCENT_INDIGO   0x4A6CF7
#define CLR_ACCENT_RED      0xFF4757
#define CLR_ACCENT_GREEN    0x2ED573
#define CLR_ACCENT_CYAN     0x22d3ee
#define CLR_ACCENT_AMBER    0xFFA502
#define CLR_BAR_BG          0x1e293b
#define CLR_CHART_BG        0x0f172a
#define CLR_THRESHOLD       0xf87171

/* ── Chart parameters ───────────────────────────────────────────── */
#define CHART_POINT_COUNT    100
#define LEVEL_MAX            20000
#define COUGH_THRESHOLD      10000
#define COUGH_WINDOW_POINTS  13

/* ── Shared helper functions (implemented in cough_ui.c) ────────── */
lv_obj_t *ui_create_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);
lv_obj_t *ui_create_section_label(lv_obj_t *parent, const char *text);

/* ── Page builders ──────────────────────────────────────────────── */
void page_home_create(lv_obj_t *parent);
void page_stats_create(lv_obj_t *parent);
void page_remind_create(lv_obj_t *parent);
void page_settings_create(lv_obj_t *parent);
void page_about_create(lv_obj_t *parent);

/* ── Page update handlers (called from message loop) ────────────── */
void page_home_set_state(const char *text);
void page_home_push_level(rt_uint16_t level);
void page_home_push_cough(void);
void page_home_update_env(float temp, float hum);
void page_home_update_stats(rt_uint32_t total, rt_uint32_t day,
                            rt_uint32_t night, rt_uint32_t bursts);
void page_home_update_reminder(const char *label);
void page_home_restore_flash(void);

void page_stats_refresh(const rt_uint32_t *hourly, rt_uint32_t total,
                        rt_uint32_t day, rt_uint32_t night,
                        rt_uint32_t bursts);

void page_remind_refresh(void);

void page_settings_update_network(void);

#ifdef __cplusplus
}
#endif

#endif /* COUGH_UI_PAGES_H */
