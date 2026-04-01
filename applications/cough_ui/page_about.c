/*
 * page_about.c — About / system information page
 *
 * Static content showing hardware, software, model info and runtime stats.
 */

#include <rtthread.h>
#include <string.h>
#include <stdio.h>
#include <lvgl.h>

#include "cough_ui_pages.h"

/* ================================================================
 *  page_about_create
 * ================================================================ */
void page_about_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_row(parent, 10, LV_PART_MAIN);

    /* ─── App icon / title area ────────────────────────────────── */
    lv_obj_t *title_card = ui_create_card(parent, CONTENT_W - 8, 110);
    lv_obj_set_style_pad_all(title_card, 16, LV_PART_MAIN);

    lv_obj_t *icon = lv_label_create(title_card);
    lv_label_set_text(icon, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(icon, lv_color_hex(CLR_ACCENT_INDIGO), LV_PART_MAIN);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *name = lv_label_create(title_card);
    lv_label_set_text(name, "Cough Sentinel v1.0");
    lv_obj_set_style_text_font(name, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(name, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 32);

    lv_obj_t *subtitle = lv_label_create(title_card);
    lv_label_set_text(subtitle, "Edge AI Cough Detection System");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 60);

    /* ─── Hardware info card ───────────────────────────────────── */
    lv_obj_t *hw_card = ui_create_card(parent, CONTENT_W - 8, 100);

    lv_obj_t *hw_hdr = ui_create_section_label(hw_card, "HARDWARE PLATFORM");
    lv_obj_align(hw_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *hw_info = lv_label_create(hw_card);
    lv_label_set_text(hw_info,
        "Board:  PSoC Edge E84 Eval\n"
        "CPU:    Cortex-M55 + Cortex-M33\n"
        "WiFi:   CYW55500");
    lv_obj_set_style_text_color(hw_info, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(hw_info, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(hw_info, 6, LV_PART_MAIN);
    lv_obj_align(hw_info, LV_ALIGN_TOP_LEFT, 0, 18);

    /* ─── Software info card ───────────────────────────────────── */
    lv_obj_t *sw_card = ui_create_card(parent, CONTENT_W - 8, 100);

    lv_obj_t *sw_hdr = ui_create_section_label(sw_card, "SOFTWARE FRAMEWORK");
    lv_obj_align(sw_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *sw_info = lv_label_create(sw_card);
    lv_label_set_text(sw_info,
        "RTOS:   RT-Thread 5.0.2\n"
        "GUI:    LVGL 9.2.0\n"
        "ML:     TFLite Micro");
    lv_obj_set_style_text_color(sw_info, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(sw_info, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(sw_info, 6, LV_PART_MAIN);
    lv_obj_align(sw_info, LV_ALIGN_TOP_LEFT, 0, 18);

    /* ─── Model info card ──────────────────────────────────────── */
    lv_obj_t *ml_card = ui_create_card(parent, CONTENT_W - 8, 100);

    lv_obj_t *ml_hdr = ui_create_section_label(ml_card, "MODEL INFORMATION");
    lv_obj_align(ml_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *ml_info = lv_label_create(ml_card);
    lv_label_set_text(ml_info,
        "Arch:   Conv2D-Small-Balanced\n"
        "Input:  40x257 Mel-spectrogram\n"
        "Arena:  53764 / 102400 bytes");
    lv_obj_set_style_text_color(ml_info, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(ml_info, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(ml_info, 6, LV_PART_MAIN);
    lv_obj_align(ml_info, LV_ALIGN_TOP_LEFT, 0, 18);

    /* ─── System runtime card ──────────────────────────────────── */
    lv_obj_t *sys_card = ui_create_card(parent, CONTENT_W - 8, 100);

    lv_obj_t *sys_hdr = ui_create_section_label(sys_card, "SYSTEM RUNTIME");
    lv_obj_align(sys_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *sys_info = lv_label_create(sys_card);
    /* Use LVGL timer to show uptime - static for now */
    rt_tick_t uptime_ms = rt_tick_get_millisecond();
    int hours   = (int)(uptime_ms / 3600000);
    int minutes = (int)((uptime_ms % 3600000) / 60000);
    int seconds = (int)((uptime_ms % 60000) / 1000);

    lv_label_set_text_fmt(sys_info,
        "Uptime: %02d:%02d:%02d\n"
        "Threads: %d\n"
        "Tick: %lu",
        hours, minutes, seconds,
        rt_object_get_length(RT_Object_Class_Thread),
        (unsigned long)rt_tick_get());
    lv_obj_set_style_text_color(sys_info, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(sys_info, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(sys_info, 6, LV_PART_MAIN);
    lv_obj_align(sys_info, LV_ALIGN_TOP_LEFT, 0, 18);

    /* ─── Footer ───────────────────────────────────────────────── */
    lv_obj_t *footer = lv_label_create(parent);
    lv_label_set_text(footer, "Built with " LV_SYMBOL_OK " for EEMC 2026");
    lv_obj_set_style_text_color(footer, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_14, LV_PART_MAIN);
}
