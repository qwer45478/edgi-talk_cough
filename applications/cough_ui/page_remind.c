/*
 * page_remind.c — Reminder management page
 *
 * Layout: scrollable list of 8 reminder slots with add/edit/delete.
 * Edit popup uses roller for hour/minute, switch for enable, dropdown for label.
 */

#include <rtthread.h>
#include <string.h>
#include <stdio.h>
#include <lvgl.h>

#include "cough_ui_pages.h"
#include "../cough_detect/cough_remind.h"

/* ── Widgets ────────────────────────────────────────────────────── */
static lv_obj_t *s_slot_list = RT_NULL;
static lv_obj_t *s_slot_cards[COUGH_REMIND_MAX_SLOTS];

/* Edit popup widgets */
static lv_obj_t *s_edit_popup      = RT_NULL;
static lv_obj_t *s_edit_roller_h   = RT_NULL;
static lv_obj_t *s_edit_roller_m   = RT_NULL;
static lv_obj_t *s_edit_switch     = RT_NULL;
static lv_obj_t *s_edit_dropdown   = RT_NULL;
static int       s_editing_slot    = -1;

/* Preset label options */
static const char *s_preset_labels[] = {
    "Morning Medicine",
    "Noon Medicine",
    "Evening Medicine",
    "Nebulizer",
    "Temperature Check",
    "Custom",
};
#define PRESET_LABEL_COUNT  6

/* ── Helpers ────────────────────────────────────────────────────── */
static void refresh_slot_card(int idx);
static void show_edit_popup(int slot_idx);

/* ── Slot card click handler ────────────────────────────────────── */
static void slot_edit_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    show_edit_popup(idx);
}

static void slot_delete_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    /* Disable the slot by setting it to 00:00 disabled */
    cough_remind_enable(idx, RT_FALSE);
    cough_remind_set(idx, 0, 0, "");
    refresh_slot_card(idx);
}

/* ================================================================
 *  page_remind_create
 * ================================================================ */
void page_remind_create(lv_obj_t *parent)
{
    lv_coord_t y = 10;

    /* Title */
    lv_obj_t *title_card = ui_create_card(parent, CONTENT_W, 40);
    lv_obj_set_pos(title_card, 12, y);
    lv_obj_set_style_pad_all(title_card, 8, LV_PART_MAIN);

    lv_obj_t *lbl_title = lv_label_create(title_card);
    lv_label_set_text(lbl_title, LV_SYMBOL_BELL " Medication Reminders");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 0, 0);

    y += 40 + 10;

    /* Scrollable list container */
    s_slot_list = lv_obj_create(parent);
    lv_obj_set_size(s_slot_list, CONTENT_W, PAGE_H - y - 10);
    lv_obj_set_pos(s_slot_list, 12, y);
    lv_obj_set_style_bg_opa(s_slot_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_slot_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_slot_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_slot_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_slot_list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_slot_list, 8, LV_PART_MAIN);

    /* Create 8 slot cards */
    for (int i = 0; i < COUGH_REMIND_MAX_SLOTS; i++)
    {
        lv_obj_t *card = lv_obj_create(s_slot_list);
        lv_obj_set_size(card, CONTENT_W - 8, 80);
        lv_obj_set_style_bg_color(card, lv_color_hex(CLR_PANEL_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(card, lv_color_hex(CLR_PANEL_BORDER), LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        /* Time label */
        lv_obj_t *lbl_time = lv_label_create(card);
        lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_22, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_time, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
        lv_obj_align(lbl_time, LV_ALIGN_LEFT_MID, 0, -8);

        /* Label text */
        lv_obj_t *lbl_label = lv_label_create(card);
        lv_obj_set_style_text_font(lbl_label, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_label, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
        lv_obj_align(lbl_label, LV_ALIGN_LEFT_MID, 0, 14);

        /* Edit button */
        lv_obj_t *btn_edit = lv_button_create(card);
        lv_obj_set_size(btn_edit, 60, 36);
        lv_obj_align(btn_edit, LV_ALIGN_RIGHT_MID, -68, 0);
        lv_obj_set_style_bg_color(btn_edit, lv_color_hex(CLR_ACCENT_INDIGO), LV_PART_MAIN);
        lv_obj_set_style_radius(btn_edit, 8, LV_PART_MAIN);

        lv_obj_t *lbl_edit = lv_label_create(btn_edit);
        lv_label_set_text(lbl_edit, LV_SYMBOL_EDIT);
        lv_obj_set_style_text_color(lbl_edit, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
        lv_obj_center(lbl_edit);

        lv_obj_add_event_cb(btn_edit, slot_edit_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);

        /* Delete button */
        lv_obj_t *btn_del = lv_button_create(card);
        lv_obj_set_size(btn_del, 60, 36);
        lv_obj_align(btn_del, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_color(btn_del, lv_color_hex(CLR_ACCENT_RED), LV_PART_MAIN);
        lv_obj_set_style_radius(btn_del, 8, LV_PART_MAIN);

        lv_obj_t *lbl_del = lv_label_create(btn_del);
        lv_label_set_text(lbl_del, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_color(lbl_del, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
        lv_obj_center(lbl_del);

        lv_obj_add_event_cb(btn_del, slot_delete_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);

        s_slot_cards[i] = card;
    }

    /* Refresh all cards to show current slot data */
    page_remind_refresh();
}

/* ── Refresh a single slot card ─────────────────────────────────── */
static void refresh_slot_card(int idx)
{
    if (idx < 0 || idx >= COUGH_REMIND_MAX_SLOTS) return;
    lv_obj_t *card = s_slot_cards[idx];
    if (card == RT_NULL) return;

    const cough_remind_slot_t *slot = cough_remind_get_slot(idx);

    /* Get child labels (time=child0, label=child1 in our layout) */
    lv_obj_t *lbl_time  = lv_obj_get_child(card, 0);
    lv_obj_t *lbl_label = lv_obj_get_child(card, 1);

    if (slot && slot->enabled && (slot->hour > 0 || slot->minute > 0 || slot->label[0] != '\0'))
    {
        /* Active slot — show green left border */
        lv_obj_set_style_border_color(card, lv_color_hex(CLR_ACCENT_GREEN), LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);

        lv_label_set_text_fmt(lbl_time, LV_SYMBOL_OK " %02d:%02d",
                              slot->hour, slot->minute);
        lv_obj_set_style_text_color(lbl_time, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);

        if (slot->label[0] != '\0')
            lv_label_set_text(lbl_label, slot->label);
        else
            lv_label_set_text(lbl_label, "(no label)");
    }
    else if (slot && !slot->enabled && slot->label[0] != '\0')
    {
        /* Disabled slot */
        lv_obj_set_style_border_color(card, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);

        lv_label_set_text_fmt(lbl_time, "  %02d:%02d", slot->hour, slot->minute);
        lv_obj_set_style_text_color(lbl_time, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
        lv_label_set_text(lbl_label, slot->label);
    }
    else
    {
        /* Empty slot */
        lv_obj_set_style_border_color(card, lv_color_hex(CLR_PANEL_BORDER), LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);

        lv_label_set_text(lbl_time, LV_SYMBOL_PLUS " (empty)");
        lv_obj_set_style_text_color(lbl_time, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
        lv_label_set_text(lbl_label, "Tap edit to add");
    }
}

/* ── Refresh all slot cards ─────────────────────────────────────── */
void page_remind_refresh(void)
{
    for (int i = 0; i < COUGH_REMIND_MAX_SLOTS; i++)
    {
        refresh_slot_card(i);
    }
}

/* ================================================================
 *  EDIT POPUP
 * ================================================================ */
static void edit_save_cb(lv_event_t *e)
{
    (void)e;
    if (s_editing_slot < 0 || s_editing_slot >= COUGH_REMIND_MAX_SLOTS)
        goto close;

    int hour   = lv_roller_get_selected(s_edit_roller_h);
    int minute = lv_roller_get_selected(s_edit_roller_m);
    rt_bool_t enabled = lv_obj_has_state(s_edit_switch, LV_STATE_CHECKED) ? RT_TRUE : RT_FALSE;

    /* Get label from dropdown */
    int sel = lv_dropdown_get_selected(s_edit_dropdown);
    const char *label = (sel >= 0 && sel < PRESET_LABEL_COUNT) ?
                        s_preset_labels[sel] : "Custom";

    cough_remind_set(s_editing_slot, (rt_uint8_t)hour, (rt_uint8_t)minute, label);
    cough_remind_enable(s_editing_slot, enabled);
    refresh_slot_card(s_editing_slot);

close:
    if (s_edit_popup)
    {
        lv_obj_delete(s_edit_popup);
        s_edit_popup = RT_NULL;
    }
    s_editing_slot = -1;
}

static void edit_cancel_cb(lv_event_t *e)
{
    (void)e;
    if (s_edit_popup)
    {
        lv_obj_delete(s_edit_popup);
        s_edit_popup = RT_NULL;
    }
    s_editing_slot = -1;
}

static void show_edit_popup(int slot_idx)
{
    if (s_edit_popup != RT_NULL)
    {
        lv_obj_delete(s_edit_popup);
        s_edit_popup = RT_NULL;
    }

    s_editing_slot = slot_idx;
    const cough_remind_slot_t *slot = cough_remind_get_slot(slot_idx);

    /* Semi-transparent overlay */
    s_edit_popup = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_edit_popup, 400, 360);
    lv_obj_center(s_edit_popup);
    lv_obj_set_style_bg_color(s_edit_popup, lv_color_hex(CLR_PANEL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_edit_popup, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_edit_popup, lv_color_hex(CLR_ACCENT_INDIGO), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_edit_popup, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(s_edit_popup, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_edit_popup, 20, LV_PART_MAIN);
    lv_obj_clear_flag(s_edit_popup, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *popup_title = lv_label_create(s_edit_popup);
    lv_label_set_text_fmt(popup_title, "Edit Reminder #%d", slot_idx + 1);
    lv_obj_set_style_text_color(popup_title, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(popup_title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(popup_title, LV_ALIGN_TOP_MID, 0, 0);

    /* Time label */
    lv_obj_t *time_lbl = lv_label_create(s_edit_popup);
    lv_label_set_text(time_lbl, "Time:");
    lv_obj_set_style_text_color(time_lbl, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(time_lbl, LV_ALIGN_TOP_LEFT, 0, 40);

    /* Hour roller */
    static char hour_opts[24 * 4]; /* "00\n01\n...\n23" */
    if (hour_opts[0] == '\0')
    {
        char *p = hour_opts;
        for (int h = 0; h < 24; h++)
        {
            if (h > 0) *p++ = '\n';
            p += rt_snprintf(p, 4, "%02d", h);
        }
    }

    s_edit_roller_h = lv_roller_create(s_edit_popup);
    lv_roller_set_options(s_edit_roller_h, hour_opts, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_size(s_edit_roller_h, 80, 80);
    lv_obj_align(s_edit_roller_h, LV_ALIGN_TOP_LEFT, 60, 32);
    lv_obj_set_style_bg_color(s_edit_roller_h, lv_color_hex(CLR_CHART_BG), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_edit_roller_h, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_edit_roller_h, lv_color_hex(CLR_ACCENT_INDIGO), LV_PART_SELECTED);
    lv_obj_set_style_text_color(s_edit_roller_h, lv_color_hex(CLR_TEXT_WHITE), LV_PART_SELECTED);

    /* Colon separator */
    lv_obj_t *colon = lv_label_create(s_edit_popup);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_text_color(colon, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(colon, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_align(colon, LV_ALIGN_TOP_LEFT, 148, 52);

    /* Minute roller */
    static char min_opts[60 * 4];
    if (min_opts[0] == '\0')
    {
        char *p = min_opts;
        for (int m = 0; m < 60; m++)
        {
            if (m > 0) *p++ = '\n';
            p += rt_snprintf(p, 4, "%02d", m);
        }
    }

    s_edit_roller_m = lv_roller_create(s_edit_popup);
    lv_roller_set_options(s_edit_roller_m, min_opts, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_size(s_edit_roller_m, 80, 80);
    lv_obj_align(s_edit_roller_m, LV_ALIGN_TOP_LEFT, 164, 32);
    lv_obj_set_style_bg_color(s_edit_roller_m, lv_color_hex(CLR_CHART_BG), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_edit_roller_m, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_edit_roller_m, lv_color_hex(CLR_ACCENT_INDIGO), LV_PART_SELECTED);
    lv_obj_set_style_text_color(s_edit_roller_m, lv_color_hex(CLR_TEXT_WHITE), LV_PART_SELECTED);

    /* Label dropdown */
    lv_obj_t *label_lbl = lv_label_create(s_edit_popup);
    lv_label_set_text(label_lbl, "Label:");
    lv_obj_set_style_text_color(label_lbl, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(label_lbl, LV_ALIGN_TOP_LEFT, 0, 130);

    s_edit_dropdown = lv_dropdown_create(s_edit_popup);
    lv_dropdown_set_options(s_edit_dropdown,
        "Morning Medicine\n"
        "Noon Medicine\n"
        "Evening Medicine\n"
        "Nebulizer\n"
        "Temperature Check\n"
        "Custom");
    lv_obj_set_width(s_edit_dropdown, 260);
    lv_obj_align(s_edit_dropdown, LV_ALIGN_TOP_LEFT, 60, 124);
    lv_obj_set_style_bg_color(s_edit_dropdown, lv_color_hex(CLR_CHART_BG), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_edit_dropdown, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_border_color(s_edit_dropdown, lv_color_hex(CLR_PANEL_BORDER), LV_PART_MAIN);

    /* Enable switch */
    lv_obj_t *en_lbl = lv_label_create(s_edit_popup);
    lv_label_set_text(en_lbl, "Enable:");
    lv_obj_set_style_text_color(en_lbl, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(en_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(en_lbl, LV_ALIGN_TOP_LEFT, 0, 190);

    s_edit_switch = lv_switch_create(s_edit_popup);
    lv_obj_set_size(s_edit_switch, 60, 30);
    lv_obj_align(s_edit_switch, LV_ALIGN_TOP_LEFT, 80, 186);
    lv_obj_set_style_bg_color(s_edit_switch, lv_color_hex(CLR_BAR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_edit_switch, lv_color_hex(CLR_ACCENT_GREEN), LV_PART_INDICATOR | LV_STATE_CHECKED);

    /* Cancel button */
    lv_obj_t *btn_cancel = lv_button_create(s_edit_popup);
    lv_obj_set_size(btn_cancel, 120, 44);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 10, 0);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(CLR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_cancel, 8, LV_PART_MAIN);

    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, LV_SYMBOL_CLOSE " Cancel");
    lv_obj_set_style_text_color(lbl_cancel, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_center(lbl_cancel);
    lv_obj_add_event_cb(btn_cancel, edit_cancel_cb, LV_EVENT_CLICKED, RT_NULL);

    /* Save button */
    lv_obj_t *btn_save = lv_button_create(s_edit_popup);
    lv_obj_set_size(btn_save, 120, 44);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -10, 0);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(CLR_ACCENT_INDIGO), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_save, 8, LV_PART_MAIN);

    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, LV_SYMBOL_OK " Save");
    lv_obj_set_style_text_color(lbl_save, lv_color_hex(CLR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_center(lbl_save);
    lv_obj_add_event_cb(btn_save, edit_save_cb, LV_EVENT_CLICKED, RT_NULL);

    /* Populate current values */
    if (slot)
    {
        lv_roller_set_selected(s_edit_roller_h, slot->hour, LV_ANIM_OFF);
        lv_roller_set_selected(s_edit_roller_m, slot->minute, LV_ANIM_OFF);
        if (slot->enabled)
            lv_obj_add_state(s_edit_switch, LV_STATE_CHECKED);

        /* Try to match label to preset */
        int matched = PRESET_LABEL_COUNT - 1; /* default to "Custom" */
        for (int i = 0; i < PRESET_LABEL_COUNT - 1; i++)
        {
            if (rt_strcmp(slot->label, s_preset_labels[i]) == 0)
            {
                matched = i;
                break;
            }
        }
        lv_dropdown_set_selected(s_edit_dropdown, matched);
    }
    else
    {
        lv_roller_set_selected(s_edit_roller_h, 8, LV_ANIM_OFF);
        lv_roller_set_selected(s_edit_roller_m, 0, LV_ANIM_OFF);
        lv_obj_add_state(s_edit_switch, LV_STATE_CHECKED);
    }
}
