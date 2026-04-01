/*
 * page_stats.c — Statistics page
 *
 * Layout (inside 512×744 tile):
 *   24-hour bar chart     (300px)
 *   Summary cards         (120px)
 *   Day/Night comparison  (100px)
 *   Trend text            (remaining)
 */

#include <rtthread.h>
#include <string.h>
#include <stdio.h>
#include <lvgl.h>

#include "cough_ui_pages.h"
#include "../cough_detect/cough_stat.h"

/* ── Widgets ────────────────────────────────────────────────────── */
static lv_obj_t *s_chart_bar       = RT_NULL;
static lv_chart_series_t *s_bar_ser = RT_NULL;

static lv_obj_t *s_label_total     = RT_NULL;
static lv_obj_t *s_label_burst     = RT_NULL;
static lv_obj_t *s_bar_day         = RT_NULL;
static lv_obj_t *s_bar_night       = RT_NULL;
static lv_obj_t *s_label_day_val   = RT_NULL;
static lv_obj_t *s_label_night_val = RT_NULL;
static lv_obj_t *s_label_trend     = RT_NULL;

/* Cached hourly data for trend analysis */
static rt_uint32_t s_hourly_cache[24];

/* ── Helper: find peak hour ─────────────────────────────────────── */
static void generate_trend_text(char *buf, int buf_size)
{
    int peak_hour = 0;
    rt_uint32_t peak_val = 0;

    for (int h = 0; h < 24; h++)
    {
        if (s_hourly_cache[h] > peak_val)
        {
            peak_val = s_hourly_cache[h];
            peak_hour = h;
        }
    }

    if (peak_val == 0)
    {
        rt_snprintf(buf, buf_size, LV_SYMBOL_LIST " No cough events recorded today.");
    }
    else
    {
        int end_hour = (peak_hour + 1) % 24;
        rt_snprintf(buf, buf_size,
            LV_SYMBOL_LIST " Peak at %d:00-%d:00 (%lu events).\n"
            "Stay warm and hydrated during\nthis period.",
            peak_hour, end_hour, (unsigned long)peak_val);
    }
}

/* ================================================================
 *  page_stats_create
 * ================================================================ */
void page_stats_create(lv_obj_t *parent)
{
    lv_coord_t y = 10;

    /* ─── Title bar ────────────────────────────────────────────── */
    lv_obj_t *title_card = ui_create_card(parent, CONTENT_W, 40);
    lv_obj_set_pos(title_card, 12, y);
    lv_obj_set_style_pad_all(title_card, 8, LV_PART_MAIN);

    lv_obj_t *lbl_title = lv_label_create(title_card);
    lv_label_set_text(lbl_title, "Today's Statistics");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 0, 0);

    y += 40 + 10;

    /* ─── 24h Bar chart ────────────────────────────────────────── */
    lv_obj_t *chart_card = ui_create_card(parent, CONTENT_W, 260);
    lv_obj_set_pos(chart_card, 12, y);

    lv_obj_t *chart_hdr = ui_create_section_label(chart_card, "24H COUGH DISTRIBUTION");
    lv_obj_align(chart_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_chart_bar = lv_chart_create(chart_card);
    lv_obj_set_size(s_chart_bar, lv_pct(100), 210);
    lv_obj_align(s_chart_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(s_chart_bar, lv_color_hex(CLR_CHART_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_chart_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_chart_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_chart_bar, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_chart_bar, 8, LV_PART_MAIN);
    lv_obj_set_style_line_color(s_chart_bar, lv_color_hex(CLR_PANEL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_line_opa(s_chart_bar, LV_OPA_30, LV_PART_MAIN);

    lv_chart_set_type(s_chart_bar, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(s_chart_bar, 24);
    lv_chart_set_range(s_chart_bar, LV_CHART_AXIS_PRIMARY_Y, 0, 20);
    lv_chart_set_div_line_count(s_chart_bar, 4, 0);

    s_bar_ser = lv_chart_add_series(s_chart_bar, lv_color_hex(CLR_ACCENT_INDIGO),
                                    LV_CHART_AXIS_PRIMARY_Y);

    /* Initialize all hours to 0 */
    for (int i = 0; i < 24; i++)
        lv_chart_set_value_by_id(s_chart_bar, s_bar_ser, i, 0);

    y += 260 + 10;

    /* ─── Summary cards ────────────────────────────────────────── */
    lv_coord_t card_w = (CONTENT_W - 8) / 2;

    /* Total today */
    lv_obj_t *total_card = ui_create_card(parent, card_w, 100);
    lv_obj_set_pos(total_card, 12, y);

    lv_obj_t *total_hdr = ui_create_section_label(total_card, "TOTAL TODAY");
    lv_obj_align(total_hdr, LV_ALIGN_TOP_MID, 0, 0);

    s_label_total = lv_label_create(total_card);
    lv_label_set_text(s_label_total, "0");
    lv_obj_set_style_text_color(s_label_total, lv_color_hex(CLR_ACCENT_INDIGO), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_total, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(s_label_total, LV_ALIGN_CENTER, 0, 10);

    /* Burst count */
    lv_obj_t *burst_card = ui_create_card(parent, card_w, 100);
    lv_obj_set_pos(burst_card, 12 + card_w + 8, y);

    lv_obj_t *burst_hdr = ui_create_section_label(burst_card, "BURST EPISODES");
    lv_obj_align(burst_hdr, LV_ALIGN_TOP_MID, 0, 0);

    s_label_burst = lv_label_create(burst_card);
    lv_label_set_text(s_label_burst, "0");
    lv_obj_set_style_text_color(s_label_burst, lv_color_hex(CLR_ACCENT_AMBER), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_burst, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(s_label_burst, LV_ALIGN_CENTER, 0, 10);

    y += 100 + 10;

    /* ─── Day/Night comparison ─────────────────────────────────── */
    lv_obj_t *dn_card = ui_create_card(parent, CONTENT_W, 100);
    lv_obj_set_pos(dn_card, 12, y);

    lv_obj_t *dn_hdr = ui_create_section_label(dn_card, "DAY / NIGHT COMPARISON");
    lv_obj_align(dn_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Day bar */
    s_label_day_val = lv_label_create(dn_card);
    lv_label_set_text(s_label_day_val, LV_SYMBOL_IMAGE " 0");
    lv_obj_set_style_text_color(s_label_day_val, lv_color_hex(CLR_ACCENT_AMBER), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_day_val, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_label_day_val, LV_ALIGN_TOP_LEFT, 0, 22);

    s_bar_day = lv_bar_create(dn_card);
    lv_obj_set_size(s_bar_day, lv_pct(65), 14);
    lv_obj_align(s_bar_day, LV_ALIGN_TOP_RIGHT, 0, 24);
    lv_bar_set_range(s_bar_day, 0, 100);
    lv_bar_set_value(s_bar_day, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_day, lv_color_hex(CLR_BAR_BG), LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar_day, 7, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar_day, lv_color_hex(CLR_ACCENT_AMBER), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_bar_day, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar_day, 7, LV_PART_INDICATOR);

    /* Night bar */
    s_label_night_val = lv_label_create(dn_card);
    lv_label_set_text(s_label_night_val, LV_SYMBOL_EYE_CLOSE " 0");
    lv_obj_set_style_text_color(s_label_night_val, lv_color_hex(CLR_ACCENT_CYAN), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_night_val, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_label_night_val, LV_ALIGN_TOP_LEFT, 0, 50);

    s_bar_night = lv_bar_create(dn_card);
    lv_obj_set_size(s_bar_night, lv_pct(65), 14);
    lv_obj_align(s_bar_night, LV_ALIGN_TOP_RIGHT, 0, 52);
    lv_bar_set_range(s_bar_night, 0, 100);
    lv_bar_set_value(s_bar_night, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_night, lv_color_hex(CLR_BAR_BG), LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar_night, 7, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar_night, lv_color_hex(CLR_ACCENT_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_bar_night, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar_night, 7, LV_PART_INDICATOR);

    y += 100 + 10;

    /* ─── Trend tip card ───────────────────────────────────────── */
    lv_obj_t *trend_card = ui_create_card(parent, CONTENT_W, 100);
    lv_obj_set_pos(trend_card, 12, y);

    lv_obj_t *trend_hdr = ui_create_section_label(trend_card, "TREND ANALYSIS");
    lv_obj_align(trend_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_trend = lv_label_create(trend_card);
    lv_label_set_text(s_label_trend, LV_SYMBOL_LIST " No data yet.");
    lv_obj_set_style_text_color(s_label_trend, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_trend, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(s_label_trend, 4, LV_PART_MAIN);
    lv_obj_set_width(s_label_trend, lv_pct(100));
    lv_obj_align(s_label_trend, LV_ALIGN_TOP_LEFT, 0, 18);
}

/* ================================================================
 *  page_stats_refresh — called periodically or on page switch
 * ================================================================ */
void page_stats_refresh(const rt_uint32_t *hourly, rt_uint32_t total,
                        rt_uint32_t day, rt_uint32_t night,
                        rt_uint32_t bursts)
{
    /* Update bar chart */
    if (s_chart_bar && s_bar_ser && hourly)
    {
        rt_uint32_t max_val = 1;
        for (int h = 0; h < 24; h++)
        {
            s_hourly_cache[h] = hourly[h];
            if (hourly[h] > max_val)
                max_val = hourly[h];
        }

        /* Dynamically scale Y axis */
        lv_chart_set_range(s_chart_bar, LV_CHART_AXIS_PRIMARY_Y, 0,
                           (max_val < 5) ? 5 : (int)(max_val + 2));

        for (int h = 0; h < 24; h++)
            lv_chart_set_value_by_id(s_chart_bar, s_bar_ser, h, (int)hourly[h]);

        lv_chart_refresh(s_chart_bar);
    }

    /* Summary */
    if (s_label_total)
        lv_label_set_text_fmt(s_label_total, "%lu", (unsigned long)total);
    if (s_label_burst)
        lv_label_set_text_fmt(s_label_burst, "%lu", (unsigned long)bursts);

    /* Day/Night comparison bars */
    rt_uint32_t dn_total = day + night;
    if (dn_total == 0) dn_total = 1;

    if (s_label_day_val)
        lv_label_set_text_fmt(s_label_day_val, LV_SYMBOL_IMAGE " %lu", (unsigned long)day);
    if (s_label_night_val)
        lv_label_set_text_fmt(s_label_night_val, LV_SYMBOL_EYE_CLOSE " %lu", (unsigned long)night);
    if (s_bar_day)
        lv_bar_set_value(s_bar_day, (int)(day * 100 / dn_total), LV_ANIM_ON);
    if (s_bar_night)
        lv_bar_set_value(s_bar_night, (int)(night * 100 / dn_total), LV_ANIM_ON);

    /* Trend text */
    if (s_label_trend)
    {
        char trend_buf[160];
        generate_trend_text(trend_buf, sizeof(trend_buf));
        lv_label_set_text(s_label_trend, trend_buf);
    }
}
