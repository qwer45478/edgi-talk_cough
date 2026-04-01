/*
 * page_home.c — Real-time monitoring page (migrated from original cough_ui.c)
 *
 * Layout (inside 512×744 tile):
 *   Waveform chart   (380px)
 *   PCM level bar    (80px)
 *   Three-column info: Cough | Day/Night | Env  (120px)
 *   System info + Reminder    (120px)
 */

#include <rtthread.h>
#include <string.h>
#include <stdio.h>
#include <lvgl.h>

#include "cough_ui_pages.h"

/* ── Widgets ────────────────────────────────────────────────────── */
static lv_obj_t *s_chart_panel     = RT_NULL;
static lv_obj_t *s_chart           = RT_NULL;
static lv_chart_series_t *s_ser       = RT_NULL;
static lv_chart_series_t *s_ser_cough = RT_NULL;

static lv_obj_t *s_bar_panel       = RT_NULL;
static lv_obj_t *s_bar_level       = RT_NULL;
static lv_obj_t *s_label_peak      = RT_NULL;

static lv_obj_t *s_label_cough_cnt = RT_NULL;
static lv_obj_t *s_label_day_cnt   = RT_NULL;
static lv_obj_t *s_label_night_cnt = RT_NULL;
static lv_obj_t *s_label_env       = RT_NULL;
static lv_obj_t *s_label_info      = RT_NULL;
static lv_obj_t *s_label_remind    = RT_NULL;

/* ── State ──────────────────────────────────────────────────────── */
static uint32_t  s_cough_count     = 0;
static rt_uint16_t s_last_peak     = 0;
static rt_bool_t s_cough_flash     = RT_FALSE;
static rt_tick_t s_cough_flash_tick = 0;
static rt_uint32_t s_stat_bursts   = 0;

/* Level history for cough segment marking */
static rt_uint16_t s_level_hist[CHART_POINT_COUNT];
static int         s_hist_pos = 0;

/* Cached state text for info panel */
static char s_state_text[16] = "IDLE";

/* ================================================================
 *  page_home_create
 * ================================================================ */
void page_home_create(lv_obj_t *parent)
{
    /* Content area starts at 10px from top of tile, pad 12 left */
    lv_coord_t y = 10;

    /* ─── 1. Chart card ────────────────────────────────────────── */
    s_chart_panel = ui_create_card(parent, CONTENT_W, 370);
    lv_obj_set_pos(s_chart_panel, 12, y);

    lv_obj_t *lbl_hdr = ui_create_section_label(s_chart_panel, "SIGNAL STRENGTH");
    lv_obj_align(lbl_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Legend: Normal dot */
    lv_obj_t *dot_n = lv_obj_create(s_chart_panel);
    lv_obj_set_size(dot_n, 8, 8);
    lv_obj_set_style_bg_color(dot_n, lv_color_hex(CLR_ACCENT_CYAN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot_n, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(dot_n, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot_n, 0, LV_PART_MAIN);
    lv_obj_clear_flag(dot_n, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(dot_n, LV_ALIGN_TOP_RIGHT, -110, 4);

    lv_obj_t *lbl_n = lv_label_create(s_chart_panel);
    lv_label_set_text(lbl_n, "Normal");
    lv_obj_set_style_text_color(lbl_n, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_n, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align_to(lbl_n, dot_n, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    /* Legend: Cough dot */
    lv_obj_t *dot_c = lv_obj_create(s_chart_panel);
    lv_obj_set_size(dot_c, 8, 8);
    lv_obj_set_style_bg_color(dot_c, lv_color_hex(CLR_ACCENT_RED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot_c, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(dot_c, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot_c, 0, LV_PART_MAIN);
    lv_obj_clear_flag(dot_c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(dot_c, lbl_n, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

    lv_obj_t *lbl_c = lv_label_create(s_chart_panel);
    lv_label_set_text(lbl_c, "Cough");
    lv_obj_set_style_text_color(lbl_c, lv_color_hex(CLR_ACCENT_RED), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_c, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align_to(lbl_c, dot_c, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    /* Chart */
    s_chart = lv_chart_create(s_chart_panel);
    lv_obj_set_size(s_chart, lv_pct(100), 300);
    lv_obj_align(s_chart, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(s_chart, lv_color_hex(CLR_CHART_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_chart, lv_color_hex(CLR_PANEL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(s_chart, 8, LV_PART_MAIN);
    lv_obj_set_style_line_color(s_chart, lv_color_hex(0x1e293b), LV_PART_MAIN);
    lv_obj_set_style_line_opa(s_chart, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_chart, 8, LV_PART_MAIN);

    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, CHART_POINT_COUNT);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, LEVEL_MAX);
    lv_chart_set_div_line_count(s_chart, 4, 0);
    lv_chart_set_update_mode(s_chart, LV_CHART_UPDATE_MODE_SHIFT);

    s_ser = lv_chart_add_series(s_chart, lv_color_hex(CLR_ACCENT_CYAN),
                                LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_size(s_chart, 0, 0, LV_PART_INDICATOR);

    for (int i = 0; i < CHART_POINT_COUNT; i++)
        lv_chart_set_next_value(s_chart, s_ser, 0);

    /* Cough overlay series */
    s_ser_cough = lv_chart_add_series(s_chart, lv_color_hex(CLR_ACCENT_RED),
                                      LV_CHART_AXIS_PRIMARY_Y);
    {
        lv_coord_t *yc = lv_chart_get_y_array(s_chart, s_ser_cough);
        for (int i = 0; i < CHART_POINT_COUNT; i++)
            yc[i] = LV_CHART_POINT_NONE;
    }

    y += 370 + 10;

    /* ─── 2. Level-bar card ────────────────────────────────────── */
    s_bar_panel = ui_create_card(parent, CONTENT_W, 80);
    lv_obj_set_pos(s_bar_panel, 12, y);

    lv_obj_t *lbl_bar = ui_create_section_label(s_bar_panel, "PEAK LEVEL (PCM abs)");
    lv_obj_align(lbl_bar, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_peak = lv_label_create(s_bar_panel);
    lv_label_set_text(s_label_peak, "0");
    lv_obj_set_style_text_color(s_label_peak, lv_color_hex(CLR_ACCENT_CYAN), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_peak, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(s_label_peak, LV_ALIGN_TOP_RIGHT, 0, -2);

    s_bar_level = lv_bar_create(s_bar_panel);
    lv_obj_set_size(s_bar_level, lv_pct(100), 18);
    lv_obj_align(s_bar_level, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_bar_set_range(s_bar_level, 0, LEVEL_MAX);
    lv_bar_set_value(s_bar_level, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_level, lv_color_hex(CLR_BAR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_bar_level, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar_level, 9, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar_level, lv_color_hex(CLR_ACCENT_INDIGO), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_bar_level, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar_level, 9, LV_PART_INDICATOR);

    lv_obj_t *lbl_min = lv_label_create(s_bar_panel);
    lv_label_set_text(lbl_min, "0");
    lv_obj_set_style_text_color(lbl_min, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_min, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(lbl_min, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *lbl_max = lv_label_create(s_bar_panel);
    lv_label_set_text(lbl_max, "20000");
    lv_obj_set_style_text_color(lbl_max, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_max, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(lbl_max, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    y += 80 + 10;

    /* ─── 3. Three-column info: Cough | Day/Night | Env ────────── */
    lv_coord_t col_w = (CONTENT_W - 2 * 8) / 3;

    /* Cough counter */
    lv_obj_t *cnt_card = ui_create_card(parent, col_w, 120);
    lv_obj_set_pos(cnt_card, 12, y);

    lv_obj_t *cnt_hdr = ui_create_section_label(cnt_card, "COUGH");
    lv_obj_align(cnt_hdr, LV_ALIGN_TOP_MID, 0, 0);

    s_label_cough_cnt = lv_label_create(cnt_card);
    lv_label_set_text(s_label_cough_cnt, "0");
    lv_obj_set_style_text_color(s_label_cough_cnt, lv_color_hex(CLR_ACCENT_INDIGO), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_cough_cnt, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(s_label_cough_cnt, LV_ALIGN_CENTER, 0, 10);

    /* Day/Night */
    lv_obj_t *dn_card = ui_create_card(parent, col_w, 120);
    lv_obj_set_pos(dn_card, 12 + col_w + 8, y);

    lv_obj_t *dn_hdr = ui_create_section_label(dn_card, "DAY / NIGHT");
    lv_obj_align(dn_hdr, LV_ALIGN_TOP_MID, 0, 0);

    s_label_day_cnt = lv_label_create(dn_card);
    lv_label_set_text(s_label_day_cnt, LV_SYMBOL_IMAGE " 0");
    lv_obj_set_style_text_color(s_label_day_cnt, lv_color_hex(CLR_ACCENT_AMBER), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_day_cnt, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(s_label_day_cnt, LV_ALIGN_LEFT_MID, 0, 4);

    s_label_night_cnt = lv_label_create(dn_card);
    lv_label_set_text(s_label_night_cnt, LV_SYMBOL_EYE_CLOSE " 0");
    lv_obj_set_style_text_color(s_label_night_cnt, lv_color_hex(CLR_ACCENT_CYAN), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_night_cnt, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(s_label_night_cnt, LV_ALIGN_LEFT_MID, 0, 28);

    /* Environment */
    lv_obj_t *env_card = ui_create_card(parent, col_w, 120);
    lv_obj_set_pos(env_card, 12 + (col_w + 8) * 2, y);

    lv_obj_t *env_hdr = ui_create_section_label(env_card, "ENVIRONMENT");
    lv_obj_align(env_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_env = lv_label_create(env_card);
    lv_label_set_text(s_label_env, "-- \xC2\xB0""C\n-- %RH");
    lv_obj_set_style_text_color(s_label_env, lv_color_hex(CLR_ACCENT_GREEN), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_env, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(s_label_env, 8, LV_PART_MAIN);
    lv_obj_align(s_label_env, LV_ALIGN_LEFT_MID, 0, 8);

    y += 120 + 10;

    /* ─── 4. Info + Reminder row ───────────────────────────────── */
    s_label_info = RT_NULL; /* Will be created below */

    lv_obj_t *info_card = ui_create_card(parent, CONTENT_W - 160 - 12, 120);
    lv_obj_set_pos(info_card, 12, y);

    lv_obj_t *info_hdr = ui_create_section_label(info_card, "SYSTEM INFO");
    lv_obj_align(info_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_info = lv_label_create(info_card);
    lv_label_set_text(s_label_info,
        "State : IDLE\n"
        "Peak  : 0\n"
        "Burst : 0");
    lv_obj_set_style_text_color(s_label_info, lv_color_hex(CLR_ACCENT_GREEN), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_info, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(s_label_info, 6, LV_PART_MAIN);
    lv_obj_align(s_label_info, LV_ALIGN_TOP_LEFT, 0, 20);

    /* Reminder */
    lv_obj_t *rem_card = ui_create_card(parent, 160, 120);
    lv_obj_set_pos(rem_card, 12 + (CONTENT_W - 160 - 12) + 12, y);

    lv_obj_t *rem_hdr = ui_create_section_label(rem_card, "REMINDER");
    lv_obj_align(rem_hdr, LV_ALIGN_TOP_MID, 0, 0);

    s_label_remind = lv_label_create(rem_card);
    lv_label_set_text(s_label_remind, "No reminder");
    lv_obj_set_style_text_color(s_label_remind, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_remind, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(s_label_remind, 4, LV_PART_MAIN);
    lv_obj_set_width(s_label_remind, lv_pct(100));
    lv_obj_align(s_label_remind, LV_ALIGN_CENTER, 0, 8);
}

/* ================================================================
 *  Update handlers
 * ================================================================ */
void page_home_set_state(const char *text)
{
    rt_strncpy(s_state_text, text, sizeof(s_state_text) - 1);
    s_state_text[sizeof(s_state_text) - 1] = '\0';
}

void page_home_push_level(rt_uint16_t level)
{
    s_last_peak = level;

    if (s_label_peak)
        lv_label_set_text_fmt(s_label_peak, "%u", level);

    if (s_bar_level)
    {
        lv_bar_set_value(s_bar_level, level, LV_ANIM_OFF);

        if (level > COUGH_THRESHOLD)
            lv_obj_set_style_bg_color(s_bar_level, lv_color_hex(CLR_ACCENT_RED), LV_PART_INDICATOR);
        else if (level > COUGH_THRESHOLD / 2)
            lv_obj_set_style_bg_color(s_bar_level, lv_color_hex(CLR_ACCENT_AMBER), LV_PART_INDICATOR);
        else
            lv_obj_set_style_bg_color(s_bar_level, lv_color_hex(CLR_ACCENT_INDIGO), LV_PART_INDICATOR);
    }

    if (s_chart && s_ser)
    {
        lv_chart_set_next_value(s_chart, s_ser, level);
        if (s_ser_cough)
            lv_chart_set_next_value(s_chart, s_ser_cough, LV_CHART_POINT_NONE);
        lv_chart_refresh(s_chart);
    }

    s_level_hist[s_hist_pos] = level;
    s_hist_pos = (s_hist_pos + 1) % CHART_POINT_COUNT;

    if (s_label_info)
    {
        lv_label_set_text_fmt(s_label_info,
            "State : %s\n"
            "Peak  : %u\n"
            "Burst : %lu",
            s_state_text, level, (unsigned long)s_stat_bursts);
    }
}

void page_home_push_cough(void)
{
    s_cough_count++;

    if (s_label_cough_cnt)
    {
        lv_label_set_text_fmt(s_label_cough_cnt, "%lu", (unsigned long)s_cough_count);
        lv_obj_set_style_text_color(s_label_cough_cnt,
                                    lv_color_hex(CLR_ACCENT_RED), LV_PART_MAIN);
    }

    /* Mark cough segment on waveform */
    if (s_chart && s_ser && s_ser_cough)
    {
        rt_uint16_t peak = 1;
        for (int i = 0; i < COUGH_WINDOW_POINTS; i++)
        {
            int hi = (s_hist_pos - 1 - i + CHART_POINT_COUNT * 2) % CHART_POINT_COUNT;
            if (s_level_hist[hi] > peak)
                peak = s_level_hist[hi];
        }

        rt_uint16_t seg_thr = peak / 3;
        uint32_t chart_start = lv_chart_get_x_start_point(s_chart, s_ser_cough);
        lv_coord_t *yc = lv_chart_get_y_array(s_chart, s_ser_cough);

        for (int i = 0; i < COUGH_WINDOW_POINTS; i++)
        {
            int hi  = (s_hist_pos - 1 - i + CHART_POINT_COUNT * 2) % CHART_POINT_COUNT;
            int raw = (int)((chart_start - 1 - i + CHART_POINT_COUNT * 2) % CHART_POINT_COUNT);
            if (s_level_hist[hi] >= seg_thr)
                yc[raw] = (lv_coord_t)s_level_hist[hi];
        }
        lv_chart_refresh(s_chart);
    }

    s_cough_flash = RT_TRUE;
    s_cough_flash_tick = rt_tick_get();
}

void page_home_update_env(float temp, float hum)
{
    if (s_label_env)
        lv_label_set_text_fmt(s_label_env, "%.1f \xC2\xB0""C\n%.1f %%RH", temp, hum);
}

void page_home_update_stats(rt_uint32_t total, rt_uint32_t day,
                            rt_uint32_t night, rt_uint32_t bursts)
{
    s_stat_bursts = bursts;
    if (s_label_day_cnt)
        lv_label_set_text_fmt(s_label_day_cnt, LV_SYMBOL_IMAGE " %lu", (unsigned long)day);
    if (s_label_night_cnt)
        lv_label_set_text_fmt(s_label_night_cnt, LV_SYMBOL_EYE_CLOSE " %lu", (unsigned long)night);
}

void page_home_update_reminder(const char *label)
{
    if (s_label_remind)
    {
        lv_label_set_text(s_label_remind, label);
        lv_obj_set_style_text_color(s_label_remind,
                                    lv_color_hex(CLR_ACCENT_AMBER), LV_PART_MAIN);
    }
}

void page_home_restore_flash(void)
{
    if (s_cough_flash)
    {
        rt_tick_t now = rt_tick_get();
        if ((now - s_cough_flash_tick) >= rt_tick_from_millisecond(2000))
        {
            s_cough_flash = RT_FALSE;
            if (s_label_cough_cnt)
            {
                lv_obj_set_style_text_color(s_label_cough_cnt,
                                            lv_color_hex(CLR_ACCENT_INDIGO),
                                            LV_PART_MAIN);
            }
        }
    }
}
