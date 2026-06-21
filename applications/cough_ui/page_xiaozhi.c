/*
 * page_xiaozhi.c - Xiaozhi AI assistant page for cough_ui
 *
 * Lightweight LVGL page. Fixed UI text stays ASCII, while dynamic assistant
 * output uses Xiaozhi's embedded TinyTTF font for wider Chinese coverage.
 */

#include <rtthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cough_ui_pages.h"
#include "page_xiaozhi.h"

extern void xiaozhi_trigger_toggle(void);
extern const unsigned char xiaozhi_font[];
extern const int xiaozhi_font_size;

#if LV_FONT_SIMSUN_14_CJK
#define XZ_FONT_CJK     (&lv_font_simsun_14_cjk)
#else
#define XZ_FONT_CJK     (&lv_font_montserrat_14)
#endif
#define XZ_FONT_UI      (&lv_font_montserrat_14)
#define XZ_FONT_OUTPUT_SIZE    20

static lv_obj_t *s_page_container = RT_NULL;
static lv_obj_t *s_status_dot = RT_NULL;
static lv_obj_t *s_status_label = RT_NULL;
static lv_obj_t *s_output_label = RT_NULL;
static lv_obj_t *s_emoji_label = RT_NULL;
static lv_obj_t *s_battery_label = RT_NULL;
static lv_font_t *s_output_font = RT_NULL;
static rt_bool_t s_is_initialized = RT_FALSE;

static void xiaozhi_talk_btn_cb(lv_event_t *e)
{
    RT_UNUSED(e);
    xiaozhi_trigger_toggle();
}

static lv_obj_t *xz_create_label(lv_obj_t *parent, const char *text,
                                 const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    return label;
}

static const lv_font_t *xz_get_output_font(void)
{
#if LV_USE_TINY_TTF
    if (s_output_font == RT_NULL)
    {
        s_output_font = lv_tiny_ttf_create_data(xiaozhi_font, xiaozhi_font_size,
                                                XZ_FONT_OUTPUT_SIZE);
        if (s_output_font == RT_NULL)
        {
            rt_kprintf("[XIAOZHI] create embedded CJK font failed, use fallback\n");
        }
        else
        {
            rt_kprintf("[XIAOZHI] embedded CJK font ready, size=%d\n", XZ_FONT_OUTPUT_SIZE);
        }
    }

    if (s_output_font != RT_NULL)
    {
        return s_output_font;
    }
#endif

    return XZ_FONT_CJK;
}

void page_xiaozhi_init(void)
{
    if (!s_is_initialized)
    {
        s_is_initialized = RT_TRUE;
        rt_kprintf("[XIAOZHI] page_xiaozhi_init done\n");
    }
}

void page_xiaozhi_create(lv_obj_t *parent)
{
    lv_obj_t *title_card;
    lv_obj_t *title_label;
    lv_obj_t *status_card;
    lv_obj_t *emoji_card;
    lv_obj_t *output_card;
    lv_obj_t *output_header;
    lv_obj_t *talk_btn;
    lv_obj_t *talk_label;
    lv_obj_t *hint_label;

    s_page_container = lv_obj_create(parent);
    lv_obj_set_size(s_page_container, CONTENT_W, PAGE_H);
    lv_obj_set_pos(s_page_container, 12, 0);
    lv_obj_set_style_bg_color(s_page_container, lv_color_hex(CLR_BG_DARK), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_page_container, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_page_container, LV_OBJ_FLAG_SCROLLABLE);

    title_card = ui_create_card(s_page_container, CONTENT_W, 72);
    lv_obj_set_pos(title_card, 0, 10);

    title_label = xz_create_label(title_card, "Xiaozhi AI", &lv_font_montserrat_20,
                                  lv_color_hex(CLR_TEXT_WHITE));
    lv_obj_set_style_text_letter_space(title_label, 1, LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 4, 0);

    s_battery_label = xz_create_label(title_card, "Battery: --", XZ_FONT_UI,
                                      lv_color_hex(CLR_TEXT_MUTED));
    lv_obj_align(s_battery_label, LV_ALIGN_RIGHT_MID, -4, 0);

    status_card = ui_create_card(s_page_container, CONTENT_W, 78);
    lv_obj_set_pos(status_card, 0, 96);

    s_status_dot = lv_obj_create(status_card);
    lv_obj_set_size(s_status_dot, 14, 14);
    lv_obj_set_style_radius(s_status_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_status_dot, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_status_dot, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_align(s_status_dot, LV_ALIGN_LEFT_MID, 0, 0);

    s_status_label = xz_create_label(status_card, "Idle - tap Start to talk", XZ_FONT_UI,
                                     lv_color_hex(CLR_ACCENT_CYAN));
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_status_label, CONTENT_W - 80);
    lv_obj_align(s_status_label, LV_ALIGN_LEFT_MID, 28, 0);

    emoji_card = ui_create_card(s_page_container, CONTENT_W, 104);
    lv_obj_set_pos(emoji_card, 0, 188);

    s_emoji_label = xz_create_label(emoji_card, "neutral", &lv_font_montserrat_20,
                                    lv_color_hex(CLR_TEXT_WHITE));
    lv_obj_align(s_emoji_label, LV_ALIGN_LEFT_MID, 4, 0);

    talk_btn = lv_obj_create(emoji_card);
    lv_obj_set_size(talk_btn, 156, 54);
    lv_obj_align(talk_btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_radius(talk_btn, 18, LV_PART_MAIN);
    lv_obj_set_style_bg_color(talk_btn, lv_color_hex(CLR_ACCENT_CYAN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(talk_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(talk_btn, 0, LV_PART_MAIN);
    lv_obj_clear_flag(talk_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(talk_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(talk_btn, xiaozhi_talk_btn_cb, LV_EVENT_CLICKED, RT_NULL);

    talk_label = xz_create_label(talk_btn, "Start / Stop", XZ_FONT_UI,
                                 lv_color_hex(CLR_BG_DARK));
    lv_obj_center(talk_label);

    output_card = ui_create_card(s_page_container, CONTENT_W, 330);
    lv_obj_set_pos(output_card, 0, 306);

    output_header = xz_create_label(output_card, "Assistant Output", XZ_FONT_UI,
                                    lv_color_hex(CLR_TEXT_MUTED));
    lv_obj_set_pos(output_header, 0, 0);

    s_output_label = xz_create_label(output_card,
                                     "Tap Start / Stop to talk with Xiaozhi.\nChinese replies use embedded CJK font.",
                                     xz_get_output_font(), lv_color_hex(CLR_TEXT_WHITE));
    lv_obj_set_width(s_output_label, CONTENT_W - 32);
    lv_label_set_long_mode(s_output_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(s_output_label, 0, 34);

    hint_label = xz_create_label(s_page_container,
                                 "KEY: nav menu    Screen button: Xiaozhi talk",
                                 XZ_FONT_UI, lv_color_hex(CLR_TEXT_MUTED));
    lv_obj_set_width(hint_label, CONTENT_W);
    lv_label_set_long_mode(hint_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(hint_label, 0, 654);

    rt_kprintf("[XIAOZHI] page_xiaozhi_create done\n");
}

void page_xiaozhi_destroy(void)
{
    if (s_page_container != RT_NULL)
    {
        lv_obj_del(s_page_container);
        s_page_container = RT_NULL;
        s_status_dot = RT_NULL;
        s_status_label = RT_NULL;
        s_output_label = RT_NULL;
        s_emoji_label = RT_NULL;
        s_battery_label = RT_NULL;
    }
}

void page_xiaozhi_set_status(const char *status)
{
    if (status == RT_NULL || status[0] == '\0')
    {
        status = "Idle";
    }

    if (s_status_label != RT_NULL)
    {
        lv_label_set_text(s_status_label, status);
    }

    if (s_status_dot != RT_NULL)
    {
        if (strstr(status, "Listen") || strstr(status, "listen"))
        {
            lv_obj_set_style_bg_color(s_status_dot, lv_color_hex(CLR_ACCENT_GREEN), LV_PART_MAIN);
        }
        else if (strstr(status, "Connect") || strstr(status, "connect") || strstr(status, "Wait") || strstr(status, "wait"))
        {
            lv_obj_set_style_bg_color(s_status_dot, lv_color_hex(CLR_ACCENT_AMBER), LV_PART_MAIN);
        }
        else if (strstr(status, "Fail") || strstr(status, "fail") || strstr(status, "ERR") || strstr(status, "error"))
        {
            lv_obj_set_style_bg_color(s_status_dot, lv_color_hex(CLR_ACCENT_RED), LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_bg_color(s_status_dot, lv_color_hex(CLR_ACCENT_CYAN), LV_PART_MAIN);
        }
    }
}

void page_xiaozhi_set_output(const char *output)
{
    if (s_output_label != RT_NULL)
    {
        lv_label_set_text(s_output_label, (output && output[0]) ? output : " ");
    }
}

void page_xiaozhi_clear_output(void)
{
    if (s_output_label != RT_NULL)
    {
        lv_label_set_text(s_output_label, " ");
    }
}

void page_xiaozhi_set_emoji(const char *emoji)
{
    if (s_emoji_label != RT_NULL)
    {
        lv_label_set_text(s_emoji_label, (emoji && emoji[0]) ? emoji : "neutral");
    }
}

void page_xiaozhi_show_ap_config(const char *ap_info)
{
    page_xiaozhi_set_output(ap_info ? ap_info : "AP config active");
}

void page_xiaozhi_show_connecting(void)
{
    page_xiaozhi_set_status("Connecting...");
}

void page_xiaozhi_update_battery(int level)
{
    if (s_battery_label != RT_NULL)
    {
        char buf[32];
        rt_snprintf(buf, sizeof(buf), "Battery: %d%%", level);
        lv_label_set_text(s_battery_label, buf);
    }
}

void page_xiaozhi_update_charging_status(bool is_charging)
{
    RT_UNUSED(is_charging);
}
