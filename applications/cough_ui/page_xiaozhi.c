/*
 * page_xiaozhi.c — 小智AI助手页面实现
 *
 * 功能：封装官方SDK的xiaozhi_ui，集成到cough_ui框架
 *
 * @yyc add
 */

#include <rtthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cough_ui_pages.h"
#include "page_xiaozhi.h"

/* 引用官方SDK的xiaozhi_ui接口 */
extern void xiaozhi_ui_init(void);
extern rt_err_t xiaozhi_ui_wait_ready(rt_int32_t timeout);
extern void xiaozhi_ui_set_status(const char *status);
extern void xiaozhi_ui_set_output(const char *output);
extern void xiaozhi_ui_set_emoji(const char *emoji);
extern void xiaozhi_ui_show_ap_config(const char *ap_info);
extern void xiaozhi_ui_show_connecting(void);
extern void xiaozhi_ui_update_battery(int level);
extern void xiaozhi_ui_update_charging_status(bool is_charging);

/* Widgets - 使用cough_ui的风格 */
static lv_obj_t *s_page_container = RT_NULL;
static lv_obj_t *s_status_label = RT_NULL;
static lv_obj_t *s_output_label = RT_NULL;
static lv_obj_t *s_emoji_label = RT_NULL;
static lv_obj_t *s_hint_label = RT_NULL;
static lv_obj_t *s_battery_label = RT_NULL;
static rt_bool_t s_is_initialized = RT_FALSE;

/* ================================================================
 *  page_xiaozhi_init
 * ================================================================ */
void page_xiaozhi_init(void)
{
    if (!s_is_initialized)
    {
        /* 调用官方SDK的UI初始化 */
        xiaozhi_ui_init();
        s_is_initialized = RT_TRUE;
        rt_kprintf("[XIAOZHI] page_xiaozhi_init done\n");
    }
}

/* ================================================================
 *  page_xiaozhi_create
 * ================================================================ */
void page_xiaozhi_create(lv_obj_t *parent)
{
    /* 等待UI就绪 */
    if (xiaozhi_ui_wait_ready(RT_WAITING_FOREVER) != RT_EOK)
    {
        rt_kprintf("[XIAOZHI] Warning: UI not ready\n");
    }

    /* 主容器 */
    s_page_container = lv_obj_create(parent);
    lv_obj_set_size(s_page_container, CONTENT_W, PAGE_H);
    lv_obj_set_pos(s_page_container, 12, 0);
    lv_obj_set_style_bg_color(s_page_container, lv_color_hex(CLR_BG_DARK), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_page_container, 0, LV_PART_MAIN);

    /* 标题卡片 */
    lv_obj_t *title_card = ui_create_card(s_page_container, CONTENT_W, 60);
    lv_obj_set_pos(title_card, 0, 10);

    lv_obj_t *title_label = lv_label_create(title_card);
    lv_label_set_text(title_label, "小智AI助手");
    lv_obj_set_style_text_color(title_label, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_center(title_label);

    /* 状态显示区域 */
    lv_obj_t *status_card = ui_create_card(s_page_container, CONTENT_W - 40, 50);
    lv_obj_set_pos(status_card, 20, 85);

    s_status_label = lv_label_create(status_card);
    lv_label_set_text(s_status_label, "待命中...");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(CLR_ACCENT_CYAN), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_center(s_status_label);

    /* Emoji显示区域 */
    lv_obj_t *emoji_card = ui_create_card(s_page_container, 120, 120);
    lv_obj_set_pos(emoji_card, (CONTENT_W - 120) / 2, 150);

    s_emoji_label = lv_label_create(emoji_card);
    lv_label_set_text(s_emoji_label, "[o_o]");
    lv_obj_set_style_text_color(s_emoji_label, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_emoji_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_center(s_emoji_label);

    /* 输出显示区域 */
    lv_obj_t *output_card = ui_create_card(s_page_container, CONTENT_W - 40, 250);
    lv_obj_set_pos(output_card, 20, 290);

    lv_obj_t *output_header = ui_create_section_label(output_card, "对话");
    lv_obj_set_pos(output_header, 0, 0);

    s_output_label = lv_label_create(output_card);
    lv_label_set_text(s_output_label, "请按开发板上方按键开始对话\n或点击\"开始聊天\"按钮");
    lv_obj_set_style_text_color(s_output_label, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_output_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_width(s_output_label, CONTENT_W - 60);
    lv_label_set_long_mode(s_output_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(s_output_label, 0, 30);

    /* 提示文字 */
    s_hint_label = lv_label_create(s_page_container);
    lv_label_set_text(s_hint_label, "提示：按开发板上方按键说话");
    lv_obj_set_style_text_color(s_hint_label, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_hint_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(s_hint_label, 20, 560);

    /* 电池状态 */
    s_battery_label = lv_label_create(s_page_container);
    lv_label_set_text(s_battery_label, "电池: --");
    lv_obj_set_style_text_color(s_battery_label, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_battery_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(s_battery_label, LV_ALIGN_BOTTOM_RIGHT, -20, -10);

    rt_kprintf("[XIAOZHI] page_xiaozhi_create done\n");
}

/* ================================================================
 *  page_xiaozhi_destroy
 * ================================================================ */
void page_xiaozhi_destroy(void)
{
    if (s_page_container != RT_NULL)
    {
        lv_obj_del(s_page_container);
        s_page_container = RT_NULL;
    }
}

/* ================================================================
 *  page_xiaozhi_set_status
 * ================================================================ */
void page_xiaozhi_set_status(const char *status)
{
    if (s_status_label != RT_NULL)
    {
        lv_label_set_text(s_status_label, status);
    }
    /* 同时调用官方SDK接口同步状态 */
    xiaozhi_ui_set_status(status);
}

/* ================================================================
 *  page_xiaozhi_set_output
 * ================================================================ */
void page_xiaozhi_set_output(const char *output)
{
    if (s_output_label != RT_NULL)
    {
        lv_label_set_text(s_output_label, output);
    }
    /* 同时调用官方SDK接口同步输出 */
    xiaozhi_ui_set_output(output);
}

/* ================================================================
 *  page_xiaozhi_clear_output
 * ================================================================ */
void page_xiaozhi_clear_output(void)
{
    if (s_output_label != RT_NULL)
    {
        lv_label_set_text(s_output_label, "");
    }
}

/* ================================================================
 *  page_xiaozhi_set_emoji
 * ================================================================ */
void page_xiaozhi_set_emoji(const char *emoji)
{
    if (s_emoji_label != RT_NULL)
    {
        lv_label_set_text(s_emoji_label, emoji);
    }
    /* 同时调用官方SDK接口 */
    xiaozhi_ui_set_emoji(emoji);
}

/* ================================================================
 *  page_xiaozhi_show_ap_config
 * ================================================================ */
void page_xiaozhi_show_ap_config(const char *ap_info)
{
    if (s_output_label != RT_NULL)
    {
        lv_label_set_text(s_output_label, ap_info);
    }
    xiaozhi_ui_show_ap_config(ap_info);
}

/* ================================================================
 *  page_xiaozhi_show_connecting
 * ================================================================ */
void page_xiaozhi_show_connecting(void)
{
    if (s_status_label != RT_NULL)
    {
        lv_label_set_text(s_status_label, "连接中...");
    }
    xiaozhi_ui_show_connecting();
}

/* ================================================================
 *  page_xiaozhi_update_battery
 * ================================================================ */
void page_xiaozhi_update_battery(int level)
{
    if (s_battery_label != RT_NULL)
    {
        char buf[32];
        rt_snprintf(buf, sizeof(buf), "电池: %d%%", level);
        lv_label_set_text(s_battery_label, buf);
    }
    xiaozhi_ui_update_battery(level);
}

/* ================================================================
 *  page_xiaozhi_update_charging_status
 * ================================================================ */
void page_xiaozhi_update_charging_status(bool is_charging)
{
    xiaozhi_ui_update_charging_status(is_charging);
}
