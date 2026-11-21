#ifndef UI_STYLE_HELPERS_H
#define UI_STYLE_HELPERS_H

#include "lvgl.h"
#include "homewind_ui.h"
#include "ui_colors.h"

LV_FONT_DECLARE(lv_font_inter_bold_14);

/* Icon font glyphs (lv_font_icon_36 has all, lv_font_icon_56 1-6 and 9; fonts/add_icons.py) */
#define ICON_HEART      "1"
#define ICON_HEART_OFF  "4"
#define ICON_CRANK      "5"
#define ICON_FAN        "6"
#define ICON_PLUS       "7"
#define ICON_MINUS      "8"
#define ICON_BOLT       "9"
#define ICON_GEAR       ":"

#define RADIUS_BADGE    5
#define FAN_DOT_SIZE    12
#define FAN_DOT_GAP     6

/* Not clickable: a touch falls through to the screen or the control below. */
static inline void apply_transparent_container_style(lv_obj_t *obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
}

/* 28 px high: 8 + 14 px line + 6 */
static inline lv_obj_t *create_badge(lv_obj_t *parent, const char *text)
{
    lv_obj_t *badge = lv_label_create(parent);
    lv_label_set_text(badge, text);
    lv_obj_set_style_text_font(badge, &lv_font_inter_bold_14, 0);
    lv_obj_set_style_text_color(badge, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, RADIUS_BADGE, 0);
    lv_obj_set_style_pad_hor(badge, 10, 0);
    /* The 14 px line has 3 px below the baseline: baseline at 19 as in Figma */
    lv_obj_set_style_pad_top(badge, 8, 0);
    lv_obj_set_style_pad_bottom(badge, 6, 0);
    return badge;
}

/* "APP" and "Trainer" badges in a row (gap 8) */
static inline lv_obj_t *create_badge_row(lv_obj_t *parent, lv_obj_t **app, lv_obj_t **trainer)
{
    lv_obj_t *row = lv_obj_create(parent);
    apply_transparent_container_style(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 8, 0);
    *app = create_badge(row, "APP");
    *trainer = create_badge(row, "Trainer");
    return row;
}

static inline void set_badge(lv_obj_t *badge, badge_state_t state)
{
    if (state == BADGE_HIDDEN) {
        lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const uint32_t color = state == BADGE_GREEN ? COLOR_GREEN
                         : state == BADGE_ORANGE ? COLOR_WARNING : COLOR_BADGE_GREY;
    lv_obj_set_style_bg_color(badge, lv_color_hex(color), 0);
    lv_obj_set_style_text_color(badge, state == BADGE_GREY ? lv_color_white() : lv_color_black(), 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_HIDDEN);
}

static inline uint32_t fan_dot_color(fan_state_t state, bool is_on)
{
    switch (state) {
        case FAN_STATE_NOT_CONFIGURATED: return COLOR_FAN_UNCONFIGURED;
        case FAN_STATE_ERROR:            return COLOR_WARNING;
        case FAN_STATE_ACTIVE:           return is_on ? COLOR_GREEN : COLOR_WHITE;
        default:                         return COLOR_WHITE;
    }
}

/* Four 12 px circles, gap 6, one per fan slot */
static inline lv_obj_t *create_fan_dots(lv_obj_t *parent, lv_obj_t *dots[4])
{
    lv_obj_t *row = lv_obj_create(parent);
    apply_transparent_container_style(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, FAN_DOT_GAP, 0);
    for (uint8_t i = 0; i < 4; i++) {
        dots[i] = lv_obj_create(row);
        apply_transparent_container_style(dots[i]);
        lv_obj_set_size(dots[i], FAN_DOT_SIZE, FAN_DOT_SIZE);
        lv_obj_set_style_radius(dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dots[i], lv_color_hex(COLOR_FAN_UNCONFIGURED), 0);
    }
    return row;
}

#endif /* UI_STYLE_HELPERS_H */
