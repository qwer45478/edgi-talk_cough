/*
 * cough_ui.c — LVGL-based multi-page UI framework for Cough Detection Monitor
 *
 * Architecture:
 *   - 5 horizontal tiles via lv_tileview (Home/Stats/Remind/Settings/About)
 *   - Fixed top status bar with dynamic title
 *   - Floating bottom navigation bar (toggle via physical button)
 *   - Thread-safe message queue for cross-thread updates
 */

#include <rtthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <lvgl.h>

#define DBG_TAG    "cough.ui"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

#include "cough_ui.h"
#include "cough_ui_pages.h"
#include "../cough_detect/cough_stat.h"

/* ── Thread parameters ────────────────────────────────────────────── */
#define UI_THREAD_STACK     (1024 * 16)
#define UI_THREAD_PRIORITY  25
#define UI_THREAD_TICK      10

/* ── Message queue ──────────────────────────────────────────────── */
#define UI_MSG_DATA_SIZE    64
#define UI_MSG_POOL_SIZE    16

/* ── Navbar auto-hide ──────────────────────────────────────────── */
#define NAVBAR_AUTO_HIDE_MS  8000
#define NAVBAR_ANIM_MS       200

/* ── Message types ──────────────────────────────────────────────── */
typedef enum
{
    UI_CMD_SET_STATE = 0,
    UI_CMD_PUSH_LEVEL,
    UI_CMD_COUGH_EVENT,
    UI_CMD_REMINDER,
    UI_CMD_UPDATE_ENV,
    UI_CMD_UPDATE_STATS,
    UI_CMD_NAV_TOGGLE,
    UI_CMD_NAV_GOTO,
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

/* Main layout */
static lv_obj_t *s_header_panel   = RT_NULL;
static lv_obj_t *s_label_title    = RT_NULL;
static lv_obj_t *s_status_dot     = RT_NULL;
static lv_obj_t *s_label_state    = RT_NULL;
static lv_obj_t *s_tileview       = RT_NULL;
static lv_obj_t *s_tiles[UI_PAGE_COUNT];

/* Navbar */
static lv_obj_t *s_navbar         = RT_NULL;
static lv_obj_t *s_nav_btns[UI_PAGE_COUNT];
static lv_obj_t *s_nav_labels[UI_PAGE_COUNT];
static rt_bool_t s_navbar_visible = RT_FALSE;
static rt_tick_t s_navbar_last_touch = 0;
static int       s_current_page   = UI_PAGE_HOME;

extern void lv_port_disp_init(void);
extern void lv_port_indev_init(void);
extern void lv_port_disp_test_red(void);

/* ── Page titles & icons ────────────────────────────────────────── */
static const char *s_page_titles[UI_PAGE_COUNT] = {
    LV_SYMBOL_AUDIO "  Cough Monitor",
    LV_SYMBOL_LIST  "  Statistics",
    LV_SYMBOL_BELL  "  Reminders",
    LV_SYMBOL_SETTINGS "  Settings",
    LV_SYMBOL_DUMMY "  About",
};

static const char *s_nav_icons[UI_PAGE_COUNT] = {
    LV_SYMBOL_AUDIO,
    LV_SYMBOL_LIST,
    LV_SYMBOL_BELL,
    LV_SYMBOL_SETTINGS,
    LV_SYMBOL_DUMMY,
};

static const char *s_nav_texts[UI_PAGE_COUNT] = {
    "Monitor",
    "Stats",
    "Remind",
    "Setting",
    "About",
};

/* ── Shared helper: create card ─────────────────────────────────── */
lv_obj_t *ui_create_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
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

/* ── Shared helper: section label ───────────────────────────────── */
lv_obj_t *ui_create_section_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    return lbl;
}

/* ── message send helper ────────────────────────────────────────── */
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

/* ================================================================
 *  NAVBAR
 * ================================================================ */
static void navbar_update_highlight(void)
{
    for (int i = 0; i < UI_PAGE_COUNT; i++)
    {
        if (i == s_current_page)
        {
            lv_obj_set_style_bg_color(s_nav_btns[i],
                                      lv_color_hex(CLR_ACCENT_INDIGO), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(s_nav_btns[i], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_text_color(s_nav_labels[i],
                                        lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_bg_opa(s_nav_btns[i], LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_text_color(s_nav_labels[i],
                                        lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
        }
    }
}

static void navbar_show(void)
{
    if (s_navbar == RT_NULL) return;
    s_navbar_visible = RT_TRUE;
    s_navbar_last_touch = rt_tick_get();
    /* Animate from off-screen to visible */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_navbar);
    lv_anim_set_values(&a, SCREEN_H, SCREEN_H - NAVBAR_H);
    lv_anim_set_time(&a, NAVBAR_ANIM_MS);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void navbar_hide(void)
{
    if (s_navbar == RT_NULL) return;
    s_navbar_visible = RT_FALSE;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_navbar);
    lv_anim_set_values(&a, SCREEN_H - NAVBAR_H, SCREEN_H);
    lv_anim_set_time(&a, NAVBAR_ANIM_MS);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);
}

static void navbar_btn_cb(lv_event_t *e)
{
    int page = (int)(intptr_t)lv_event_get_user_data(e);
    if (page >= 0 && page < UI_PAGE_COUNT)
    {
        s_current_page = page;
        lv_tileview_set_tile_by_index(s_tileview, page, 0, LV_ANIM_ON);
        navbar_update_highlight();
        /* Update header title */
        lv_label_set_text(s_label_title, s_page_titles[page]);
        s_navbar_last_touch = rt_tick_get();
    }
}

static void tileview_change_cb(lv_event_t *e)
{
    lv_obj_t *tile = lv_tileview_get_tile_active(s_tileview);
    for (int i = 0; i < UI_PAGE_COUNT; i++)
    {
        if (tile == s_tiles[i])
        {
            s_current_page = i;
            lv_label_set_text(s_label_title, s_page_titles[i]);
            navbar_update_highlight();
            break;
        }
    }
}

static void navbar_auto_hide_check(void)
{
    if (s_navbar_visible)
    {
        rt_tick_t elapsed = rt_tick_get() - s_navbar_last_touch;
        if (elapsed >= rt_tick_from_millisecond(NAVBAR_AUTO_HIDE_MS))
        {
            navbar_hide();
        }
    }
}

/* ================================================================
 *  STATUS BAR
 * ================================================================ */
static void ui_build_header(lv_obj_t *scr)
{
    s_header_panel = lv_obj_create(scr);
    lv_obj_set_size(s_header_panel, SCREEN_W, HEADER_H);
    lv_obj_set_pos(s_header_panel, 0, 0);
    lv_obj_set_style_bg_color(s_header_panel, lv_color_hex(CLR_HEADER_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_header_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_header_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_header_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(s_header_panel, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_right(s_header_panel, SAFE_R, LV_PART_MAIN);
    lv_obj_clear_flag(s_header_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_label_title = lv_label_create(s_header_panel);
    lv_label_set_text(s_label_title, s_page_titles[UI_PAGE_HOME]);
    lv_obj_set_style_text_color(s_label_title, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_title, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_align(s_label_title, LV_ALIGN_LEFT_MID, 0, 0);

    /* Status dot + state text (right side) */
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
}

/* ================================================================
 *  TILEVIEW + PAGES
 * ================================================================ */
static void ui_build_tileview(lv_obj_t *scr)
{
    s_tileview = lv_tileview_create(scr);
    lv_obj_set_size(s_tileview, SCREEN_W, PAGE_H);
    lv_obj_set_pos(s_tileview, 0, HEADER_H);
    lv_obj_set_style_bg_color(s_tileview, lv_color_hex(CLR_BG_DARK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_tileview, LV_OPA_COVER, LV_PART_MAIN);

    /* Add 5 horizontal tiles */
    s_tiles[0] = lv_tileview_add_tile(s_tileview, 0, 0, LV_DIR_RIGHT);
    s_tiles[1] = lv_tileview_add_tile(s_tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    s_tiles[2] = lv_tileview_add_tile(s_tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    s_tiles[3] = lv_tileview_add_tile(s_tileview, 3, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    s_tiles[4] = lv_tileview_add_tile(s_tileview, 4, 0, LV_DIR_LEFT);

    /* Style each tile */
    for (int i = 0; i < UI_PAGE_COUNT; i++)
    {
        lv_obj_set_style_bg_color(s_tiles[i], lv_color_hex(CLR_BG_DARK), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_tiles[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_pad_all(s_tiles[i], 0, LV_PART_MAIN);
    }

    /* Build page content inside each tile */
    page_home_create(s_tiles[0]);
    page_stats_create(s_tiles[1]);
    page_remind_create(s_tiles[2]);
    page_settings_create(s_tiles[3]);
    page_about_create(s_tiles[4]);

    /* Listen for tile change events */
    lv_obj_add_event_cb(s_tileview, tileview_change_cb, LV_EVENT_VALUE_CHANGED, RT_NULL);
}

/* ================================================================
 *  NAVBAR BUILD
 * ================================================================ */
static void ui_build_navbar(lv_obj_t *scr)
{
    s_navbar = lv_obj_create(scr);
    lv_obj_set_size(s_navbar, CONTENT_W, NAVBAR_H);
    lv_obj_set_pos(s_navbar, 12, SCREEN_H);   /* start hidden off-screen */
    lv_obj_set_style_bg_color(s_navbar, lv_color_hex(CLR_PANEL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_navbar, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_navbar, lv_color_hex(CLR_PANEL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_navbar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(s_navbar, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_navbar, 4, LV_PART_MAIN);
    lv_obj_clear_flag(s_navbar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_navbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_navbar, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_coord_t btn_w = (CONTENT_W - 8 - UI_PAGE_COUNT * 4) / UI_PAGE_COUNT;

    for (int i = 0; i < UI_PAGE_COUNT; i++)
    {
        lv_obj_t *btn = lv_obj_create(s_navbar);
        lv_obj_set_size(btn, btn_w, NAVBAR_H - 12);
        lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn, 2, LV_PART_MAIN);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

        /* Icon */
        lv_obj_t *icon = lv_label_create(btn);
        lv_label_set_text(icon, s_nav_icons[i]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_set_style_text_color(icon, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);

        /* Text */
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, s_nav_texts[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);

        lv_obj_add_event_cb(btn, navbar_btn_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);

        s_nav_btns[i] = btn;
        s_nav_labels[i] = lbl;
    }

    navbar_update_highlight();
}

/* ================================================================
 *  FULL UI BUILD
 * ================================================================ */
static void ui_build(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(CLR_BG_DARK), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    ui_build_tileview(scr);
    ui_build_header(scr);
    ui_build_navbar(scr);
}

/* ================================================================
 *  STATUS BAR UPDATE
 * ================================================================ */
static void ui_handle_state(const char *text)
{
    if (s_label_state)
    {
        lv_label_set_text(s_label_state, text);
    }
    if (s_status_dot)
    {
        if (rt_strcmp(text, "IDLE") == 0)
            lv_obj_set_style_bg_color(s_status_dot, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
        else if (rt_strcmp(text, "RECORDING") == 0 || rt_strcmp(text, "RECORD") == 0)
            lv_obj_set_style_bg_color(s_status_dot, lv_color_hex(CLR_ACCENT_RED), LV_PART_MAIN);
        else
            lv_obj_set_style_bg_color(s_status_dot, lv_color_hex(CLR_ACCENT_GREEN), LV_PART_MAIN);
    }
    page_home_set_state(text);
}

/* ── Environment/stats data parsing ─────────────────────────────── */
static void ui_handle_env(const char *data)
{
    float temp = 0.0f, hum = 0.0f;
    sscanf(data, "%f,%f", &temp, &hum);
    page_home_update_env(temp, hum);
}

static void ui_handle_stats(const char *data)
{
    unsigned long total = 0, day = 0, night = 0, bursts = 0;
    sscanf(data, "%lu,%lu,%lu,%lu", &total, &day, &night, &bursts);
    page_home_update_stats((rt_uint32_t)total, (rt_uint32_t)day,
                           (rt_uint32_t)night, (rt_uint32_t)bursts);

    /* Also refresh the statistics page with hourly data */
    const cough_stat_daily_t *daily = cough_stat_get_daily();
    if (daily)
    {
        page_stats_refresh(daily->hourly, daily->total_today,
                           daily->day_count, daily->night_count,
                           daily->burst_count);
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
    {
        long v = strtol(msg->data, RT_NULL, 10);
        if (v < 0) v = 0;
        if (v > LEVEL_MAX) v = LEVEL_MAX;
        page_home_push_level((rt_uint16_t)v);
        break;
    }

    case UI_CMD_COUGH_EVENT:
        page_home_push_cough();
        break;

    case UI_CMD_REMINDER:
        page_home_update_reminder(msg->data);
        break;

    case UI_CMD_UPDATE_ENV:
        ui_handle_env(msg->data);
        break;

    case UI_CMD_UPDATE_STATS:
        ui_handle_stats(msg->data);
        break;

    case UI_CMD_NAV_TOGGLE:
        if (s_navbar_visible)
            navbar_hide();
        else
            navbar_show();
        break;

    case UI_CMD_NAV_GOTO:
    {
        int page = (int)strtol(msg->data, RT_NULL, 10);
        if (page >= 0 && page < UI_PAGE_COUNT)
        {
            s_current_page = page;
            lv_tileview_set_tile_by_index(s_tileview, page, 0, LV_ANIM_ON);
            lv_label_set_text(s_label_title, s_page_titles[page]);
            navbar_update_highlight();
        }
        break;
    }

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

    LOG_I("lv_init()...");
    lv_init();
    lv_tick_set_cb(&rt_tick_get_millisecond);

    LOG_I("lv_port_disp_init()...");
    lv_port_disp_init();

    LOG_I("Display HW test: filling red framebuffer...");
    lv_port_disp_test_red();
    LOG_I("Display HW test: red frame sent to DC. If screen is RED, display chain OK.");
    LOG_I("Display HW test: waiting 3 seconds...");
    rt_thread_mdelay(3000);
    LOG_I("Display HW test: done, proceeding to LVGL UI");

    LOG_I("lv_port_indev_init()...");
    lv_port_indev_init();

    LOG_I("ui_build()...");
    ui_build();

    LOG_I("UI init complete, entering main loop");
    rt_sem_release(&s_ui_ready_sem);

    while (1)
    {
        while (rt_mq_recv(&s_ui_mq, &msg, sizeof(msg), RT_WAITING_NO) > 0)
        {
            ui_process_message(&msg);
        }

        /* Periodic maintenance */
        page_home_restore_flash();
        navbar_auto_hide_check();

        period_ms = lv_timer_handler();
        if (period_ms == LV_NO_TIMER_READY || period_ms > 100)
        {
            period_ms = 20;
        }
        rt_thread_mdelay(period_ms);
    }
}

/* ================================================================
 *  Public API (thread-safe)
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

void cough_ui_push_reminder(const char *label)
{
    ui_send(UI_CMD_REMINDER, label ? label : "Reminder");
}

void cough_ui_update_env(float temp, float hum)
{
    char tmp[32];
    rt_snprintf(tmp, sizeof(tmp), "%.1f,%.1f", temp, hum);
    ui_send(UI_CMD_UPDATE_ENV, tmp);
}

void cough_ui_update_stats(rt_uint32_t total, rt_uint32_t day,
                           rt_uint32_t night, rt_uint32_t bursts)
{
    char tmp[48];
    rt_snprintf(tmp, sizeof(tmp), "%lu,%lu,%lu,%lu",
                (unsigned long)total, (unsigned long)day,
                (unsigned long)night, (unsigned long)bursts);
    ui_send(UI_CMD_UPDATE_STATS, tmp);
}

void cough_ui_nav_toggle(void)
{
    ui_send(UI_CMD_NAV_TOGGLE, RT_NULL);
}

void cough_ui_nav_goto(int page_index)
{
    char tmp[8];
    rt_snprintf(tmp, sizeof(tmp), "%d", page_index);
    ui_send(UI_CMD_NAV_GOTO, tmp);
}
