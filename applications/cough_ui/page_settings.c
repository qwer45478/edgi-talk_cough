/*
 * page_settings.c — System settings page
 *
 * Layout: scrollable settings list with WiFi info, sliders, switches.
 */

#include <rtthread.h>
#include <string.h>
#include <stdio.h>
#include <lvgl.h>

#include "cough_ui_pages.h"
#include "../cough_detect/cough_detect.h"
#include "../common/common_network.h"
#include "../common/common_display.h"

/* ── Widgets ────────────────────────────────────────────────────── */
static lv_obj_t *s_label_wifi_status = RT_NULL;
static lv_obj_t *s_label_wifi_ssid   = RT_NULL;
static lv_obj_t *s_slider_threshold  = RT_NULL;
static lv_obj_t *s_label_threshold   = RT_NULL;
static lv_obj_t *s_label_baseline    = RT_NULL;
static lv_obj_t *s_slider_brightness = RT_NULL;
static lv_obj_t *s_label_brightness  = RT_NULL;
static lv_obj_t *s_switch_upload     = RT_NULL;

/* ── Callbacks ──────────────────────────────────────────────────── */
static void threshold_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    if (s_label_threshold)
    {
        lv_label_set_text_fmt(s_label_threshold, "0.%02d", val);
    }
    /* Actual threshold update could be wired via a config API */
}

static void brightness_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    if (s_label_brightness)
    {
        lv_label_set_text_fmt(s_label_brightness, "%d%%", val);
    }
    common_display_set_brightness((rt_uint8_t)val);
}

static void recalibrate_btn_cb(lv_event_t *e)
{
    (void)e;
    /* Request noise re-calibration via dedicated event */
    cough_detect_send_event(CD_EVENT_CALIBRATE);
}

/* ================================================================
 *  page_settings_create
 * ================================================================ */
void page_settings_create(lv_obj_t *parent)
{
    /* Make the parent scrollable for settings overflow */
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_row(parent, 10, LV_PART_MAIN);
    lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);

    /* ─── WiFi card ────────────────────────────────────────────── */
    lv_obj_t *wifi_card = ui_create_card(parent, CONTENT_W - 8, 100);

    lv_obj_t *wifi_hdr = ui_create_section_label(wifi_card, LV_SYMBOL_WIFI " WiFi");
    lv_obj_align(wifi_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_wifi_status = lv_label_create(wifi_card);
    lv_label_set_text(s_label_wifi_status, "Disconnected");
    lv_obj_set_style_text_color(s_label_wifi_status, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_wifi_status, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_label_wifi_status, LV_ALIGN_TOP_LEFT, 0, 22);

    s_label_wifi_ssid = lv_label_create(wifi_card);
    lv_label_set_text(s_label_wifi_ssid, "SSID: --");
    lv_obj_set_style_text_color(s_label_wifi_ssid, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_wifi_ssid, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_label_wifi_ssid, LV_ALIGN_TOP_LEFT, 0, 44);

    /* Update WiFi info */
    page_settings_update_network();

    /* ─── Detection threshold card ─────────────────────────────── */
    lv_obj_t *thr_card = ui_create_card(parent, CONTENT_W - 8, 90);

    lv_obj_t *thr_hdr = ui_create_section_label(thr_card, "Detection Threshold");
    lv_obj_align(thr_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_threshold = lv_label_create(thr_card);
    lv_label_set_text(s_label_threshold, "0.35");
    lv_obj_set_style_text_color(s_label_threshold, lv_color_hex(CLR_ACCENT_CYAN), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_threshold, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(s_label_threshold, LV_ALIGN_TOP_RIGHT, 0, 0);

    s_slider_threshold = lv_slider_create(thr_card);
    lv_obj_set_size(s_slider_threshold, lv_pct(100), 14);
    lv_obj_align(s_slider_threshold, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_slider_set_range(s_slider_threshold, 20, 80);  /* 0.20 ~ 0.80 */
    lv_slider_set_value(s_slider_threshold, 35, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_slider_threshold, lv_color_hex(CLR_BAR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider_threshold, lv_color_hex(CLR_ACCENT_INDIGO), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider_threshold, lv_color_hex(CLR_TEXT_WHITE), LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider_threshold, threshold_slider_cb, LV_EVENT_VALUE_CHANGED, RT_NULL);

    /* ─── Noise calibration card ───────────────────────────────── */
    lv_obj_t *cal_card = ui_create_card(parent, CONTENT_W - 8, 90);

    lv_obj_t *cal_hdr = ui_create_section_label(cal_card, "Noise Calibration");
    lv_obj_align(cal_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_baseline = lv_label_create(cal_card);
    lv_label_set_text_fmt(s_label_baseline, "Baseline: %.1f", cough_detect_get_baseline());
    lv_obj_set_style_text_color(s_label_baseline, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_baseline, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_label_baseline, LV_ALIGN_TOP_LEFT, 0, 22);

    lv_obj_t *btn_recal = lv_button_create(cal_card);
    lv_obj_set_size(btn_recal, 140, 36);
    lv_obj_align(btn_recal, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(btn_recal, lv_color_hex(CLR_ACCENT_AMBER), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_recal, 8, LV_PART_MAIN);

    lv_obj_t *recal_lbl = lv_label_create(btn_recal);
    lv_label_set_text(recal_lbl, LV_SYMBOL_REFRESH " Recalibrate");
    lv_obj_set_style_text_color(recal_lbl, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_center(recal_lbl);
    lv_obj_add_event_cb(btn_recal, recalibrate_btn_cb, LV_EVENT_CLICKED, RT_NULL);

    /* ─── Display brightness card ──────────────────────────────── */
    lv_obj_t *disp_card = ui_create_card(parent, CONTENT_W - 8, 80);

    lv_obj_t *disp_hdr = ui_create_section_label(disp_card, "Display Brightness");
    lv_obj_align(disp_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_label_brightness = lv_label_create(disp_card);
    lv_label_set_text(s_label_brightness, "90%");
    lv_obj_set_style_text_color(s_label_brightness, lv_color_hex(CLR_ACCENT_CYAN), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_brightness, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(s_label_brightness, LV_ALIGN_TOP_RIGHT, 0, 0);

    s_slider_brightness = lv_slider_create(disp_card);
    lv_obj_set_size(s_slider_brightness, lv_pct(100), 14);
    lv_obj_align(s_slider_brightness, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_slider_set_range(s_slider_brightness, 10, 100);
    lv_slider_set_value(s_slider_brightness, 90, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_slider_brightness, lv_color_hex(CLR_BAR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider_brightness, lv_color_hex(CLR_ACCENT_INDIGO), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider_brightness, lv_color_hex(CLR_TEXT_WHITE), LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider_brightness, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, RT_NULL);

    /* ─── Cloud upload card ────────────────────────────────────── */
    lv_obj_t *cloud_card = ui_create_card(parent, CONTENT_W - 8, 80);

    lv_obj_t *cloud_hdr = ui_create_section_label(cloud_card, "Cloud Upload");
    lv_obj_align(cloud_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *lbl_auto = lv_label_create(cloud_card);
    lv_label_set_text(lbl_auto, "Auto upload:");
    lv_obj_set_style_text_color(lbl_auto, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_auto, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(lbl_auto, LV_ALIGN_TOP_LEFT, 0, 28);

    s_switch_upload = lv_switch_create(cloud_card);
    lv_obj_set_size(s_switch_upload, 60, 30);
    lv_obj_align(s_switch_upload, LV_ALIGN_TOP_LEFT, 120, 24);
    lv_obj_set_style_bg_color(s_switch_upload, lv_color_hex(CLR_BAR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_switch_upload, lv_color_hex(CLR_ACCENT_GREEN), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_state(s_switch_upload, LV_STATE_CHECKED);  /* default ON */
}

/* ================================================================
 *  page_settings_update_network
 * ================================================================ */
void page_settings_update_network(void)
{
    const common_network_t *net = common_network_get();
    if (net == RT_NULL) return;

    if (s_label_wifi_status)
    {
        const char *state_str;
        lv_color_t state_clr;
        switch (net->state)
        {
        case NETWORK_STATE_CONNECTED:
            state_str = LV_SYMBOL_OK " Connected";
            state_clr = lv_color_hex(CLR_ACCENT_GREEN);
            break;
        case NETWORK_STATE_CONNECTING:
            state_str = LV_SYMBOL_REFRESH " Connecting...";
            state_clr = lv_color_hex(CLR_ACCENT_AMBER);
            break;
        case NETWORK_STATE_ERROR:
            state_str = LV_SYMBOL_CLOSE " Error";
            state_clr = lv_color_hex(CLR_ACCENT_RED);
            break;
        default:
            state_str = "Disconnected";
            state_clr = lv_color_hex(CLR_TEXT_MUTED);
            break;
        }
        lv_label_set_text(s_label_wifi_status, state_str);
        lv_obj_set_style_text_color(s_label_wifi_status, state_clr, LV_PART_MAIN);
    }

    if (s_label_wifi_ssid && net->ssid[0] != '\0')
    {
        lv_label_set_text_fmt(s_label_wifi_ssid, "SSID: %s", net->ssid);
    }
}
