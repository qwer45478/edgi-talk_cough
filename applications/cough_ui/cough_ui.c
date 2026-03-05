/*
 * cough_ui.c — LVGL-based UI for Cough Detection Monitor
 *
 * Visual style inspired by the web frontend (resources/cough.html):
 *   - Dark background with accent colors
 *   - Real-time waveform chart (volume vs time)
 *   - Level bar with threshold marker
 *   - Cough event counter
 *   - State indicator
 *
 * Thread-safe: all public APIs post messages to an internal queue;
 * the UI thread consumes them inside the LVGL tick loop.
 */

#include <rtthread.h>
#include <string.h>
#include <stdlib.h>
#include <lvgl.h>

#define DBG_TAG    "cough.ui"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

#include "cough_ui.h"

/* ── Thread parameters ────────────────────────────────────────────── */
#define UI_THREAD_STACK     (1024 * 16)
#define UI_THREAD_PRIORITY  25
#define UI_THREAD_TICK      10

/* ── Message queue ──────────────────────────────────────────────── */
#define UI_MSG_DATA_SIZE    64
#define UI_MSG_POOL_SIZE    16

/* ── Chart parameters ───────────────────────────────────────────── */
#define CHART_POINT_COUNT    100
#define LEVEL_MAX            20000
#define COUGH_THRESHOLD      10000  /* visual threshold line on chart */
/* Number of chart points covered by one inference window (10496/16000≈0.656s,
 * level is pushed every ~50ms → 0.656/0.05 ≈ 13 points) */
#define COUGH_WINDOW_POINTS  13

/* ── Color palette (matching frontend cough.html) ───────────────── */
#define CLR_BG_DARK         0x101418   /* main background              */
#define CLR_PANEL_BG        0x1a1e24   /* card / panel background      */
#define CLR_PANEL_BORDER    0x2d3340   /* subtle border                */
#define CLR_HEADER_BG       0x4f46e5   /* indigo-600 header            */
#define CLR_TEXT_WHITE      0xffffff
#define CLR_TEXT_MUTED      0x94a3b8   /* gray-400                     */
#define CLR_ACCENT_INDIGO   0x6366f1   /* indigo-500  (normal line)    */
#define CLR_ACCENT_RED      0xef4444   /* red-500     (cough line)     */
#define CLR_ACCENT_GREEN    0x4ade80   /* green-400   (status OK)      */
#define CLR_ACCENT_CYAN     0x22d3ee   /* cyan-400    (chart line)     */
#define CLR_ACCENT_AMBER    0xfbbf24   /* amber-400                    */
#define CLR_BAR_BG          0x1e293b   /* bar track                    */
#define CLR_CHART_BG        0x0f172a   /* chart background             */
#define CLR_THRESHOLD       0xf87171   /* red-400 threshold line       */

/* ── Display dimensions ────────────────────────────────────────── */
#define SCREEN_W  512       /* LCD hardware resolution              */
#define SCREEN_H  800
#define SAFE_R    34        /* right inset: 12 margin + 22 bezel    */
#define CONTENT_W (SCREEN_W - 12 - SAFE_R)   /* 466 usable width   */

/* ── Message types ──────────────────────────────────────────────── */
typedef enum
{
    UI_CMD_SET_STATE = 0,
    UI_CMD_PUSH_LEVEL,
    UI_CMD_COUGH_EVENT,
} ui_cmd_t;

typedef struct
{
    ui_cmd_t cmd;
    char data[UI_MSG_DATA_SIZE];
} ui_msg_t;

/* ── Static objects ─────────────────────────────────────────────── */
static struct rt_semaphore  s_ui_ready_sem;
static struct rt_messagequeue s_ui_mq;
static char s_ui_mq_pool[RT_MQ_BUF_SIZE(sizeof(ui_msg_t), UI_MSG_POOL_SIZE)];

/* LVGL widgets */
static lv_obj_t *s_header_panel    = RT_NULL;
static lv_obj_t *s_label_title     = RT_NULL;
static lv_obj_t *s_status_dot      = RT_NULL;
static lv_obj_t *s_label_state     = RT_NULL;

static lv_obj_t *s_chart_panel     = RT_NULL;
static lv_obj_t *s_label_chart_hdr = RT_NULL;
static lv_obj_t *s_chart           = RT_NULL;
static lv_chart_series_t *s_ser       = RT_NULL;  /* normal (cyan) series        */
static lv_chart_series_t *s_ser_cough = RT_NULL;  /* cough highlight (red) overlay */

static lv_obj_t *s_bar_panel       = RT_NULL;
static lv_obj_t *s_bar_level       = RT_NULL;
static lv_obj_t *s_label_peak      = RT_NULL;
static lv_obj_t *s_label_bar_min   = RT_NULL;
static lv_obj_t *s_label_bar_max   = RT_NULL;

static lv_obj_t *s_info_panel      = RT_NULL;
static lv_obj_t *s_label_cough_cnt = RT_NULL;
static lv_obj_t *s_label_cough_hdr = RT_NULL;
static lv_obj_t *s_label_info      = RT_NULL;

static uint32_t  s_cough_count     = 0;
static uint16_t  s_last_peak       = 0;
static rt_bool_t s_cough_flash     = RT_FALSE;   /* counter is in red-flash mode  */
static rt_tick_t s_cough_flash_tick = 0;          /* when the flash started        */

/* Level history — circular buffer mirroring the chart, used to locate
 * the actual cough segment (peak position) at inference time.        */
static rt_uint16_t s_level_hist[CHART_POINT_COUNT];
static int         s_hist_pos = 0;  /* next write slot (0..CHART_POINT_COUNT-1) */

extern void lv_port_disp_init(void);
extern void lv_port_indev_init(void);
extern void lv_port_disp_test_red(void);  /* fill framebuf with red, bypass LVGL */

/* ── Helpers ────────────────────────────────────────────────────── */
static void ui_send(ui_cmd_t cmd, const char *text)
{
    ui_msg_t msg;
    rt_memset(&msg, 0, sizeof(msg));
    msg.cmd = cmd;
    if (text)
    {
        rt_strncpy(msg.data, text, sizeof(msg.data) - 1);
    }
    rt_mq_send(&s_ui_mq, &msg, sizeof(msg));
}

static rt_uint16_t parse_level(const char *text)
{
    long v = strtol(text, RT_NULL, 10);
    if (v < 0) v = 0;
    if (v > LEVEL_MAX) v = LEVEL_MAX;
    return (rt_uint16_t)v;
}

/* ── Helper: create a rounded "card" panel ──────────────────────── */
static lv_obj_t *create_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, lv_color_hex(CLR_PANEL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(CLR_PANEL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

/* ── Helper: section header label ───────────────────────────────── */
static lv_obj_t *create_section_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    return lbl;
}

/* ================================================================
 *  UI BUILD — Creates the full interface
 * ================================================================ */
static void ui_build(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(CLR_BG_DARK), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* ─── 1. Header bar (indigo gradient-like) ─────────────────── */
    s_header_panel = lv_obj_create(scr);
    lv_obj_set_size(s_header_panel, SCREEN_W, 56);
    lv_obj_set_pos(s_header_panel, 0, 0);
    lv_obj_set_style_bg_color(s_header_panel, lv_color_hex(CLR_HEADER_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_header_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_header_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_header_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(s_header_panel, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_right(s_header_panel, SAFE_R, LV_PART_MAIN);
    lv_obj_clear_flag(s_header_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_label_title = lv_label_create(s_header_panel);
    lv_label_set_text(s_label_title, LV_SYMBOL_AUDIO "  Cough Monitor");
    lv_obj_set_style_text_color(s_label_title, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_title, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_align(s_label_title, LV_ALIGN_LEFT_MID, 0, 0);

    /* Status dot + state text (right side of header, inside safe area) */
    s_label_state = lv_label_create(s_header_panel);
    lv_label_set_text(s_label_state, "IDLE");
    lv_obj_set_style_text_color(s_label_state, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_state, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_label_state, LV_ALIGN_RIGHT_MID, 0, 0);

    s_status_dot = lv_obj_create(s_header_panel);
    lv_obj_set_size(s_status_dot, 10, 10);
    lv_obj_set_style_bg_color(s_status_dot, lv_color_hex(CLR_ACCENT_GREEN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_status_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_status_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_status_dot, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_status_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(s_status_dot, s_label_state, LV_ALIGN_OUT_LEFT_MID, -8, 0);

    /* ─── 2. Chart card ────────────────────────────────────────── */
    s_chart_panel = create_card(scr, CONTENT_W, 380);
    lv_obj_set_pos(s_chart_panel, 12, 66);

    s_label_chart_hdr = create_section_label(s_chart_panel, "SIGNAL STRENGTH");
    lv_obj_align(s_label_chart_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Legend dots */
    lv_obj_t *dot_normal = lv_obj_create(s_chart_panel);
    lv_obj_set_size(dot_normal, 8, 8);
    lv_obj_set_style_bg_color(dot_normal, lv_color_hex(CLR_ACCENT_CYAN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot_normal, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(dot_normal, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot_normal, 0, LV_PART_MAIN);
    lv_obj_clear_flag(dot_normal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(dot_normal, LV_ALIGN_TOP_RIGHT, -110, 4);

    lv_obj_t *lbl_n = lv_label_create(s_chart_panel);
    lv_label_set_text(lbl_n, "Normal");
    lv_obj_set_style_text_color(lbl_n, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_n, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align_to(lbl_n, dot_normal, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    lv_obj_t *dot_cough = lv_obj_create(s_chart_panel);
    lv_obj_set_size(dot_cough, 8, 8);
    lv_obj_set_style_bg_color(dot_cough, lv_color_hex(CLR_ACCENT_RED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot_cough, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(dot_cough, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot_cough, 0, LV_PART_MAIN);
    lv_obj_clear_flag(dot_cough, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(dot_cough, lbl_n, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

    lv_obj_t *lbl_c = lv_label_create(s_chart_panel);
    lv_label_set_text(lbl_c, "Cough");
    lv_obj_set_style_text_color(lbl_c, lv_color_hex(CLR_ACCENT_RED), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_c, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align_to(lbl_c, dot_cough, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    /* The chart itself */
    s_chart = lv_chart_create(s_chart_panel);
    lv_obj_set_size(s_chart, lv_pct(100), 310);
    lv_obj_align(s_chart, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(s_chart, lv_color_hex(CLR_CHART_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_chart, lv_color_hex(CLR_PANEL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(s_chart, 8, LV_PART_MAIN);
    lv_obj_set_style_line_color(s_chart, lv_color_hex(0x1e293b), LV_PART_MAIN); /* grid lines */
    lv_obj_set_style_line_opa(s_chart, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_chart, 8, LV_PART_MAIN);

    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, CHART_POINT_COUNT);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, LEVEL_MAX);
    lv_chart_set_div_line_count(s_chart, 4, 0);
    lv_chart_set_update_mode(s_chart, LV_CHART_UPDATE_MODE_SHIFT);

    s_ser = lv_chart_add_series(s_chart, lv_color_hex(CLR_ACCENT_CYAN),
                                LV_CHART_AXIS_PRIMARY_Y);

    /* Hide point markers — show only the line */
    lv_obj_set_style_size(s_chart, 0, 0, LV_PART_INDICATOR);

    /* Initialize all points to 0 */
    for (int i = 0; i < CHART_POINT_COUNT; i++)
    {
        lv_chart_set_next_value(s_chart, s_ser, 0);
    }

    /* Cough overlay series — all points invisible until a cough event */
    s_ser_cough = lv_chart_add_series(s_chart, lv_color_hex(CLR_ACCENT_RED),
                                      LV_CHART_AXIS_PRIMARY_Y);
    {
        lv_coord_t *yc = lv_chart_get_y_array(s_chart, s_ser_cough);
        for (int i = 0; i < CHART_POINT_COUNT; i++)
            yc[i] = LV_CHART_POINT_NONE;
    }

    /* ─── 3. Level-bar card ────────────────────────────────────── */
    s_bar_panel = create_card(scr, CONTENT_W, 80);
    lv_obj_set_pos(s_bar_panel, 12, 456);

    lv_obj_t *lbl_bar_hdr = create_section_label(s_bar_panel, "PEAK LEVEL (PCM abs)");
    lv_obj_align(lbl_bar_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

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

    s_label_bar_min = lv_label_create(s_bar_panel);
    lv_label_set_text(s_label_bar_min, "0");
    lv_obj_set_style_text_color(s_label_bar_min, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_bar_min, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(s_label_bar_min, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    s_label_bar_max = lv_label_create(s_bar_panel);
    lv_label_set_text(s_label_bar_max, "20000");
    lv_obj_set_style_text_color(s_label_bar_max, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_bar_max, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(s_label_bar_max, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    /* ─── 4. Bottom info row: cough counter + info ─────────────── */
    /* Cough counter (left) */
    lv_obj_t *cnt_card = create_card(scr, 160, 230);
    lv_obj_set_pos(cnt_card, 12, 546);

    s_label_cough_hdr = create_section_label(cnt_card, "COUGH COUNT");
    lv_obj_align(s_label_cough_hdr, LV_ALIGN_TOP_MID, 0, 0);

    s_label_cough_cnt = lv_label_create(cnt_card);
    lv_label_set_text(s_label_cough_cnt, "0");
    lv_obj_set_style_text_color(s_label_cough_cnt, lv_color_hex(CLR_ACCENT_INDIGO), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_cough_cnt, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(s_label_cough_cnt, LV_ALIGN_CENTER, 0, 10);

    /* Info panel (right) */
    s_info_panel = create_card(scr, CONTENT_W - 160 - 12, 230);
    lv_obj_set_pos(s_info_panel, 12 + 160 + 12, 546);

    lv_obj_t *info_hdr = create_section_label(s_info_panel, "SYSTEM INFO");
    lv_obj_align(info_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_info = lv_label_create(s_info_panel);
    lv_label_set_text(s_label_info,
        "State : IDLE\n"
        "Peak  : 0\n"
        "Base  : --\n"
        "Cough : 0");
    lv_obj_set_style_text_color(s_label_info, lv_color_hex(CLR_ACCENT_GREEN), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_info, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(s_label_info, 10, LV_PART_MAIN);
    lv_obj_align(s_label_info, LV_ALIGN_TOP_LEFT, 0, 24);
}

/* ================================================================
 *  UI UPDATE HANDLERS
 * ================================================================ */
static void ui_handle_level(rt_uint16_t level)
{
    s_last_peak = level;

    /* Update peak label */
    if (s_label_peak)
    {
        lv_label_set_text_fmt(s_label_peak, "%u", level);
    }

    /* Update bar (color changes based on level) */
    if (s_bar_level)
    {
        lv_bar_set_value(s_bar_level, level, LV_ANIM_OFF);

        if (level > COUGH_THRESHOLD)
        {
            lv_obj_set_style_bg_color(s_bar_level, lv_color_hex(CLR_ACCENT_RED),
                                      LV_PART_INDICATOR);
        }
        else if (level > COUGH_THRESHOLD / 2)
        {
            lv_obj_set_style_bg_color(s_bar_level, lv_color_hex(CLR_ACCENT_AMBER),
                                      LV_PART_INDICATOR);
        }
        else
        {
            lv_obj_set_style_bg_color(s_bar_level, lv_color_hex(CLR_ACCENT_INDIGO),
                                      LV_PART_INDICATOR);
        }
    }

    /* Push new data point into the chart.
     * CRITICAL: s_ser_cough must be advanced with set_next_value every frame
     * so its internal start_point stays in sync with s_ser.  Without this
     * the overlay raw indices never shift and red marks stay frozen in place.
     * We push POINT_NONE (invisible) normally; cough events overwrite the
     * relevant slots afterwards using the now-synced start_point. */
    if (s_chart && s_ser)
    {
        lv_chart_set_next_value(s_chart, s_ser, level);
        if (s_ser_cough)
            lv_chart_set_next_value(s_chart, s_ser_cough, LV_CHART_POINT_NONE);
        lv_chart_refresh(s_chart);
    }

    /* Mirror into our own history buffer (advances after chart push) */
    s_level_hist[s_hist_pos] = level;
    s_hist_pos = (s_hist_pos + 1) % CHART_POINT_COUNT;

    /* Update info panel text */
    if (s_label_info)
    {
        const char *state_str = "IDLE";
        if (s_label_state)
        {
            state_str = lv_label_get_text(s_label_state);
        }
        lv_label_set_text_fmt(s_label_info,
            "State : %s\n"
            "Peak  : %u\n"
            "Base  : --\n"
            "Cough : %lu",
            state_str,
            level,
            (unsigned long)s_cough_count);
    }
}

static void ui_handle_cough_event(void)
{
    s_cough_count++;

    /* Update counter display, flash red */
    if (s_label_cough_cnt)
    {
        lv_label_set_text_fmt(s_label_cough_cnt, "%lu", (unsigned long)s_cough_count);
        lv_obj_set_style_text_color(s_label_cough_cnt,
                                    lv_color_hex(CLR_ACCENT_RED), LV_PART_MAIN);
    }

    /*
     * Mark the actual cough segment on the waveform in red.
     *
     * The model processes a ~650ms window and reports a cough only AFTER
     * the window ends.  By then the microphone signal may already be quiet
     * again.  We therefore scan the level history buffer for the last
     * COUGH_WINDOW_POINTS entries, find the dominant peak, and mark every
     * point within that segment where the level exceeds peak/3 (half-max
     * approach).
     *
     * LVGL 9 SHIFT mode: chart->start_point is the NEXT write slot
     * (= current oldest displayed).  After n calls to set_next_value,
     * the newest raw index = (start_point - 1 + N) & modN.
     * Point i steps back from newest = (start_point - 1 - i + N*2) % N.
     */
    if (s_chart && s_ser && s_ser_cough)
    {
        /* ── 1. Find peak level within the inference window ─────── */
        rt_uint16_t peak = 1;
        for (int i = 0; i < COUGH_WINDOW_POINTS; i++)
        {
            int hi = (s_hist_pos - 1 - i + CHART_POINT_COUNT * 2) % CHART_POINT_COUNT;
            if (s_level_hist[hi] > peak)
                peak = s_level_hist[hi];
        }

        /* ── 2. Mark the elevated segment (level > peak/3) in red ──
         * Both series now have identical start_point (kept in sync via
         * set_next_value every frame), so raw index arithmetic is the same.
         * start_point points to the NEXT slot to be written, so the newest
         * filled slot is (start_point - 1 + N) % N. */
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

/* Restore counter colour after cough flash delay.
 * The chart overlay clears itself naturally: each new level push erases
 * the oldest slot in the overlay, so red segments scroll off leftward
 * without any explicit timer-based cleanup. */
static void ui_restore_chart_color(void)
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

static void ui_handle_state(const char *text)
{
    if (s_label_state)
    {
        lv_label_set_text(s_label_state, text);
    }

    /* Update status dot color based on state */
    if (s_status_dot)
    {
        /* Default: green for active states, amber for idle */
        if (rt_strcmp(text, "IDLE") == 0)
        {
            lv_obj_set_style_bg_color(s_status_dot,
                                      lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
        }
        else if (rt_strcmp(text, "RECORDING") == 0)
        {
            lv_obj_set_style_bg_color(s_status_dot,
                                      lv_color_hex(CLR_ACCENT_RED), LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_bg_color(s_status_dot,
                                      lv_color_hex(CLR_ACCENT_GREEN), LV_PART_MAIN);
        }
    }
}

/* ── Message dispatcher ─────────────────────────────────────────── */
static void ui_process_message(const ui_msg_t *msg)
{
    if (!msg) return;

    switch (msg->cmd)
    {
    case UI_CMD_SET_STATE:
        ui_handle_state(msg->data);
        break;

    case UI_CMD_PUSH_LEVEL:
        ui_handle_level(parse_level(msg->data));
        break;

    case UI_CMD_COUGH_EVENT:
        ui_handle_cough_event();
        break;

    default:
        break;
    }
}

/* ── UI thread entry ────────────────────────────────────────────── */
static void ui_thread_entry(void *args)
{
    ui_msg_t msg;
    rt_uint32_t period_ms;
    (void)args;

    LOG_I("UI thread started");

    /* Initialize LVGL and display */
    LOG_I("lv_init()...");
    lv_init();
    lv_tick_set_cb(&rt_tick_get_millisecond);

    LOG_I("lv_port_disp_init()...");
    lv_port_disp_init();

    /* ---- Direct framebuffer test: bypass LVGL, write red to screen ---- */
    LOG_I("Display HW test: filling red framebuffer...");
    lv_port_disp_test_red();
    LOG_I("Display HW test: red frame sent to DC. If screen is RED, display chain OK.");
    LOG_I("Display HW test: waiting 3 seconds...");
    rt_thread_mdelay(3000);
    LOG_I("Display HW test: done, proceeding to LVGL UI");

    LOG_I("lv_port_indev_init()...");
    lv_port_indev_init();

    /* Build all UI elements */
    LOG_I("ui_build()...");
    ui_build();

    /* Signal that UI is ready */
    LOG_I("UI init complete, entering main loop");
    rt_sem_release(&s_ui_ready_sem);

    while (1)
    {
        /* Drain all pending messages */
        while (rt_mq_recv(&s_ui_mq, &msg, sizeof(msg), RT_WAITING_NO) > 0)
        {
            ui_process_message(&msg);
        }

        /* Periodic maintenance */
        ui_restore_chart_color();

        /* Let LVGL do its rendering */
        period_ms = lv_timer_handler();
        if (period_ms == LV_NO_TIMER_READY || period_ms > 100)
        {
            period_ms = 20;
        }
        rt_thread_mdelay(period_ms);
    }
}

/* ================================================================
 *  Public API (thread-safe, can be called from any context)
 * ================================================================ */
void cough_ui_init(void)
{
    rt_thread_t tid;

    rt_sem_init(&s_ui_ready_sem, "cd_ui_sem", 0, RT_IPC_FLAG_PRIO);
    rt_mq_init(&s_ui_mq, "cd_ui_mq", s_ui_mq_pool, sizeof(ui_msg_t),
               sizeof(s_ui_mq_pool), RT_IPC_FLAG_FIFO);

    tid = rt_thread_create("cd_ui", ui_thread_entry, RT_NULL,
                           UI_THREAD_STACK, UI_THREAD_PRIORITY, UI_THREAD_TICK);
    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
    }
    else
    {
        LOG_E("Create cough ui thread failed");
    }
}

rt_err_t cough_ui_wait_ready(rt_int32_t timeout)
{
    return rt_sem_take(&s_ui_ready_sem, timeout);
}

void cough_ui_set_state_text(const char *text)
{
    ui_send(UI_CMD_SET_STATE, text ? text : "-");
}

void cough_ui_push_level(rt_uint16_t level)
{
    char tmp[16];
    rt_snprintf(tmp, sizeof(tmp), "%u", level);
    ui_send(UI_CMD_PUSH_LEVEL, tmp);
}

void cough_ui_push_cough_event(void)
{
    ui_send(UI_CMD_COUGH_EVENT, RT_NULL);
}
