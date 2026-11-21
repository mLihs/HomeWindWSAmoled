// homewind_ui.c
#include "homewind_ui.h"
#include "powersave.h"
#include "lv_conf_psram_auto.h"
#include "lvgl.h"
#include "ui_colors.h"
#include "ui_style_helpers.h"
#include "extra/libs/qrcode/lv_qrcode.h" // LVGL QR code library (part of LVGL)
#include "qmi8658c.h"  // For QMI8658_SLAVE_ADDR_L conditional IMU support
#include <string.h>
#include <stdio.h>


/* --- Font Includes --- */
// Declare Inter font files (fonts are compiled separately by Arduino)
// Font files must be in src/ directory for Arduino to compile them
LV_FONT_DECLARE(lv_font_inter_black_64);
LV_FONT_DECLARE(lv_font_inter_bold_14);
LV_FONT_DECLARE(lv_font_inter_bold_24);

LV_FONT_DECLARE(lv_font_icon_36);
LV_FONT_DECLARE(lv_font_icon_56);


/* --- Design Tokens --- */
#define RADIUS_CARD             28
#define RADIUS_CONTROL          32
#define RADIUS_FIELD            8
#define RADIUS_BUTTON           36
#define RADIUS_ICON             18
#define RADIUS_QR               24

#define SCREEN_GAP              8
#define SCREEN_INSET            2       // black margin around the main screen (case bezel)
#define STATUS_BAR_H            44
#define ROW_H                   56
#define ROW_LABEL_W             206
#define HR_NAME_W               248
#define FAN_CONTROL_H           132
#define FAN_SIDE_W              81      // the centre field takes the rest (106)
#define LONG_PRESS_MS           700

/* --- Animation Timing --- */
#define MODAL_ANIMATION_TIME_MS  180  // Animation duration for modal overlay
#define MODAL_SLIDE_DISTANCE     60   // Overlay slides this far (px); every frame redraws the moving area

/* --- Global Objects --- */
lv_obj_t *scr_main;  // Made non-static for powersave module access
static lv_obj_t *scr_boot = NULL;
static lv_obj_t *scr_apmode = NULL;
static lv_obj_t *settings_overlay = NULL;
static lv_obj_t *qr_code_widget = NULL;

/* FIX Issue #2: Track modal visibility to skip LVGL updates when not visible */
static bool settings_modal_visible = false;
static bool ui_refresh_pending = false;  // Flag to refresh UI on modal close

/* --- Status Bar --- */
static lv_obj_t *badge_app;
static lv_obj_t *badge_trainer;
static badge_state_t app_badge = BADGE_GREY;
static badge_state_t trainer_badge = BADGE_HIDDEN;
static bool badges_dirty = true;

/* --- HR Widget Objects --- */
static lv_obj_t *card_hr;
static lv_obj_t *hr_value_row;
static lv_obj_t *lbl_hr_value;
static lv_obj_t *lbl_hr_icon;
static lv_obj_t *lbl_hr_sensor_name;
static lv_obj_t *lbl_hr_status;

/* --- Power and Cadence Rows --- */
#define ROW_DIRTY_STATE  0x01
#define ROW_DIRTY_NAME   0x02
#define ROW_DIRTY_VALUE  0x04
#define ROW_DIRTY_ALL    0x07

typedef struct {
    lv_obj_t *card;
    lv_obj_t *icon;
    lv_obj_t *lbl_name;
    lv_obj_t *lbl_value;
    uint32_t color;
    uint32_t color_dark;
    const char *unit;
    row_state_t state;
    uint16_t value;
    char name[64];
    uint8_t dirty;
} bike_row_t;

static bike_row_t power_row = { .color = COLOR_POWER, .color_dark = COLOR_POWER_DARK,
                                .unit = "W", .dirty = ROW_DIRTY_ALL };
static bike_row_t cadence_row = { .color = COLOR_CADENCE, .color_dark = COLOR_CADENCE_DARK,
                                  .unit = "RPM", .dirty = ROW_DIRTY_ALL };

/* --- Fan Control --- */
#define MAX_FANS 4
static struct {
    fan_state_t state;
    bool is_on;
} fans[MAX_FANS];
static lv_obj_t *fan_dots[MAX_FANS];
static lv_obj_t *lbl_fan_mode;
static lv_obj_t *lbl_fan_icon;
static lv_obj_t *lbl_fan_pct;
static fan_mode_t fan_mode = FAN_MODE_PURE;
static bool fan_target_on = false;
static uint8_t fan_pct = 0;
static bool fan_control_dirty = true;
static bool fan_dots_dirty = true;

static fan_toggle_callback_t fan_toggle_callback = NULL;
static hr_shift_callback_t hr_shift_callback = NULL;
static fan_mode_callback_t fan_mode_callback = NULL;

/* --- Touch Controls --- */
typedef enum {
    CTRL_NONE = 0,     /* screen background: a touch only wakes */
    CTRL_SETTINGS,
    CTRL_MINUS,
    CTRL_MODE,
    CTRL_PLUS,
    CTRL_CLOSE
} control_t;

static bool press_ignored = false;     /* this press only woke the display */
static bool long_press_fired = false;
static uint32_t press_start_ms = 0;

/* --- State Variables --- */
static hr_state_t hr_current_state = HR_NOT_CONFIGURATED;
static uint16_t hr_value = 0;
static char hr_sensor_name[64] = "";
static char qr_code_url[128] = "https://martin-lihs.com";

/* --- HR widget cache (reduces redundant LVGL updates) --- */
static hr_state_t hr_widget_cache_state = (hr_state_t)-1;
static uint16_t hr_widget_cache_value = 0xFFFF;
static char hr_widget_cache_name[64] = "";
static bool hr_widget_cache_initialized = false;

/* --- Forward Declarations --- */
static void animate_overlay_in(void);
static void animate_overlay_out(void);
static void anim_overlay_out_ready_cb(lv_anim_t *a);
static void control_event_cb(lv_event_t *e);
static void create_boot_screen(void);
static void create_ap_screen(void);
static void create_main_screen(void);
static void create_settings_modal(void);
static void update_main_widgets(void);
static void update_badges(void);
static void update_hr_widget(void);
static void update_row(bike_row_t *row);
static void update_fan_control(void);
static bool should_skip_main_screen_update(void);

static void start_anim(lv_anim_t *anim, void *var, int32_t from, int32_t to, uint32_t time_ms,
                       lv_anim_path_cb_t path_cb, lv_anim_exec_xcb_t exec_cb,
                       lv_anim_ready_cb_t ready_cb, void *user_data)
{
    lv_anim_init(anim);
    lv_anim_set_var(anim, var);
    lv_anim_set_values(anim, from, to);
    lv_anim_set_time(anim, time_ms);
    lv_anim_set_path_cb(anim, path_cb);
    lv_anim_set_exec_cb(anim, exec_cb);
    if (ready_cb) {
        lv_anim_set_ready_cb(anim, ready_cb);
    }
    if (user_data) {
        lv_anim_set_user_data(anim, user_data);
    }
    lv_anim_start(anim);
}

/* --- Animation Functions --- */
static void animate_overlay_in(void)
{
    /* FIX Issue #2: Mark modal as visible (skip UI updates while open) */
    settings_modal_visible = true;
    ui_refresh_pending = false;

    lv_obj_set_y(settings_overlay, MODAL_SLIDE_DISTANCE);
    lv_obj_clear_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t a;
    start_anim(&a, settings_overlay, MODAL_SLIDE_DISTANCE, 0, MODAL_ANIMATION_TIME_MS,
               lv_anim_path_ease_out, (lv_anim_exec_xcb_t)lv_obj_set_y, NULL, NULL);
}

static void anim_overlay_out_ready_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    lv_obj_add_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);

    /* FIX Issue #2: Mark modal as hidden and refresh UI if changes occurred */
    settings_modal_visible = false;
    if (ui_refresh_pending) {
        ui_refresh_pending = false;
        update_main_widgets();
    }
}

static void animate_overlay_out(void)
{
    lv_anim_t a;
    start_anim(&a, settings_overlay, lv_obj_get_y(settings_overlay), MODAL_SLIDE_DISTANCE,
               MODAL_ANIMATION_TIME_MS, lv_anim_path_ease_in, (lv_anim_exec_xcb_t)lv_obj_set_y,
               anim_overlay_out_ready_cb, NULL);
}

/* --- Touch: a press that wakes the display triggers nothing, also not on release.
 *     Plus and minus act on click (one step per tap), the mode field on a long press. --- */
static void control_event_cb(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    const control_t ctrl = (control_t)(intptr_t)lv_event_get_user_data(e);

    if (code == LV_EVENT_PRESSED) {
        press_ignored = powersave_touch_wakes();
        long_press_fired = false;
        press_start_ms = lv_tick_get();
        return;
    }
    if (press_ignored) return;

    switch (ctrl) {
        case CTRL_SETTINGS:
            if (code == LV_EVENT_CLICKED) animate_overlay_in();
            break;
        case CTRL_MINUS:
            if (code == LV_EVENT_CLICKED && hr_shift_callback) hr_shift_callback(1);
            break;
        case CTRL_PLUS:
            if (code == LV_EVENT_CLICKED && hr_shift_callback) hr_shift_callback(-1);
            break;
        case CTRL_MODE:
            if (code == LV_EVENT_PRESSING && !long_press_fired &&
                lv_tick_elaps(press_start_ms) >= LONG_PRESS_MS) {
                long_press_fired = true;
                if (fan_mode_callback) fan_mode_callback();
            }
            break;
        case CTRL_CLOSE:
            if (code == LV_EVENT_CLICKED) animate_overlay_out();
            break;
        default:
            break;
    }
}

static void make_control(lv_obj_t *obj, control_t ctrl)
{
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj, control_event_cb, LV_EVENT_ALL, (void *)(intptr_t)ctrl);
}

/* --- Status Bar (badges, gear) --- */
static void create_status_bar(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    apply_transparent_container_style(bar);
    lv_obj_set_size(bar, LV_PCT(100), STATUS_BAR_H);
    /* Square pink bar over the rounded top corners of the HR block */
    lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

    lv_obj_t *badges = create_badge_row(bar, &badge_app, &badge_trainer);
    lv_obj_set_pos(badges, 24, 13);

    lv_obj_t *gear = lv_label_create(bar);
    lv_label_set_text(gear, ICON_GEAR);
    lv_obj_set_style_text_font(gear, &lv_font_icon_36, 0);
    lv_obj_set_style_text_color(gear, lv_color_white(), 0);
    lv_obj_set_pos(gear, 225, 12);  /* glyph 22 × 22 at x 230, y 16: 24 px from the right edge */
}

static void update_badges(void)
{
    if (!badges_dirty || should_skip_main_screen_update()) return;
    badges_dirty = false;
    set_badge(badge_app, app_badge);
    set_badge(badge_trainer, trainer_badge);
}

/* --- HR Widget (height follows the hidden rows: 140, 204 or 268) --- */
static void create_hr_widget(lv_obj_t *parent)
{
    card_hr = lv_obj_create(parent);
    apply_transparent_container_style(card_hr);
    lv_obj_set_width(card_hr, LV_PCT(100));
    lv_obj_set_flex_grow(card_hr, 1);
    lv_obj_set_style_pad_top(card_hr, 6, 0);
    lv_obj_set_style_pad_bottom(card_hr, 12, 0);
    lv_obj_set_style_pad_hor(card_hr, 8, 0);
    lv_obj_set_style_pad_row(card_hr, 0, 0);
    lv_obj_clear_flag(card_hr, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(card_hr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card_hr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* HR Value and Icon Row (height per state, see update_hr_widget) */
    hr_value_row = lv_obj_create(card_hr);
    apply_transparent_container_style(hr_value_row);
    lv_obj_set_size(hr_value_row, LV_SIZE_CONTENT, 76);
    lv_obj_set_flex_flow(hr_value_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hr_value_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    /* 10 px gap in Figma, the heart glyph brings 3 px side bearing */
    lv_obj_set_style_pad_column(hr_value_row, 7, 0);

    lbl_hr_value = lv_label_create(hr_value_row);
    lv_obj_set_style_text_color(lbl_hr_value, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_hr_value, &lv_font_inter_black_64, 0);
    lv_obj_set_style_text_letter_space(lbl_hr_value, -1, 0);

    lbl_hr_icon = lv_label_create(hr_value_row);
    lv_obj_set_style_text_color(lbl_hr_icon, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_hr_icon, &lv_font_icon_56, 0);
    lv_label_set_text(lbl_hr_icon, ICON_HEART);

    /* Sensor name: one line, the rest is cut off */
    lbl_hr_sensor_name = lv_label_create(card_hr);
    lv_obj_set_style_text_color(lbl_hr_sensor_name, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_hr_sensor_name, &lv_font_inter_bold_24, 0);
    lv_obj_set_style_text_align(lbl_hr_sensor_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl_hr_sensor_name, LV_LABEL_LONG_CLIP);
    lv_label_set_text(lbl_hr_sensor_name, "");

    /* "Disconnected" / "Add Sensors" */
    lbl_hr_status = lv_label_create(card_hr);
    lv_obj_set_style_text_color(lbl_hr_status, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_hr_status, &lv_font_inter_bold_24, 0);
    lv_obj_set_style_text_align(lbl_hr_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_hr_status, "");
}

/* Fixed box with the Bold 24 baseline (pad_top + 18) where Figma's line box puts it */
static void set_hr_text_box(lv_obj_t *lbl, lv_coord_t h, lv_coord_t pad_top)
{
    lv_obj_set_style_pad_top(lbl, pad_top, 0);
    lv_obj_set_size(lbl, HR_NAME_W, h);
}

/* Centred if it fits, otherwise left-aligned and cut off at the right edge */
static void set_hr_name_text(void)
{
    lv_coord_t w = lv_txt_get_width(hr_sensor_name, strlen(hr_sensor_name), &lv_font_inter_bold_24, 0,
                                    LV_TEXT_FLAG_NONE);
    lv_obj_set_style_text_align(lbl_hr_sensor_name, w > HR_NAME_W ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_hr_sensor_name, hr_sensor_name);
}

static void update_hr_widget(void)
{
    if (should_skip_main_screen_update()) {
        return;
    }

    if (hr_widget_cache_initialized &&
        hr_widget_cache_state == hr_current_state &&
        hr_widget_cache_value == hr_value &&
        strncmp(hr_widget_cache_name, hr_sensor_name, sizeof(hr_widget_cache_name)) == 0) {
        return;  /* No visual change needed */
    }

    hr_widget_cache_state = hr_current_state;
    hr_widget_cache_value = hr_value;
    strncpy(hr_widget_cache_name, hr_sensor_name, sizeof(hr_widget_cache_name) - 1);
    hr_widget_cache_name[sizeof(hr_widget_cache_name) - 1] = '\0';
    hr_widget_cache_initialized = true;

    if (hr_current_state == HR_STATE_ACTIVE && hr_value > 0) {
        /* Value row 76 + name 34 */
        lv_obj_set_style_pad_top(card_hr, 6, 0);
        lv_obj_set_height(hr_value_row, 76);
        lv_label_set_text(lbl_hr_icon, ICON_HEART);
        lv_label_set_text_fmt(lbl_hr_value, "%u", (unsigned)hr_value);
        lv_obj_clear_flag(lbl_hr_value, LV_OBJ_FLAG_HIDDEN);
        set_hr_text_box(lbl_hr_sensor_name, 34, 8);
        set_hr_name_text();
        lv_obj_clear_flag(lbl_hr_sensor_name, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_hr_status, LV_OBJ_FLAG_HIDDEN);
    } else if (hr_current_state != HR_NOT_CONFIGURATED) {
        /* Heart 45 + name and "Disconnected", 26 px lines; the texts 8 px lower than in Figma.
         * The extra card padding keeps the centred heart in place. */
        lv_obj_set_style_pad_top(card_hr, 6 + 8, 0);
        lv_obj_set_height(hr_value_row, 45);
        lv_label_set_text(lbl_hr_icon, ICON_HEART_OFF);
        lv_obj_add_flag(lbl_hr_value, LV_OBJ_FLAG_HIDDEN);
        set_hr_text_box(lbl_hr_sensor_name, 27 + 8, 4 + 8);
        set_hr_name_text();
        lv_obj_clear_flag(lbl_hr_sensor_name, LV_OBJ_FLAG_HIDDEN);
        set_hr_text_box(lbl_hr_status, 25, 3);
        lv_label_set_text(lbl_hr_status, "Disconnected");
        lv_obj_clear_flag(lbl_hr_status, LV_OBJ_FLAG_HIDDEN);
    } else {
        /* Heart in a 57 px box + "Add Sensors" in a 38 px box */
        lv_obj_set_style_pad_top(card_hr, 6, 0);
        lv_obj_set_height(hr_value_row, 57);
        lv_label_set_text(lbl_hr_icon, ICON_HEART);
        lv_obj_add_flag(lbl_hr_value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_hr_sensor_name, LV_OBJ_FLAG_HIDDEN);
        set_hr_text_box(lbl_hr_status, 38, 10);
        lv_label_set_text(lbl_hr_status, "Add Sensors");
        lv_obj_clear_flag(lbl_hr_status, LV_OBJ_FLAG_HIDDEN);
    }
}

/* --- Power and Cadence Rows --- */
static void create_row(lv_obj_t *parent, bike_row_t *row, const char *icon)
{
    row->card = lv_obj_create(parent);
    lv_obj_set_size(row->card, LV_PCT(100), ROW_H);
    lv_obj_set_style_bg_opa(row->card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row->card, RADIUS_CARD, 0);
    lv_obj_set_style_border_width(row->card, 0, 0);
    lv_obj_set_style_pad_all(row->card, 0, 0);
    lv_obj_clear_flag(row->card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(row->card, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *circle = lv_obj_create(row->card);
    apply_transparent_container_style(circle);
    lv_obj_set_size(circle, 36, 36);
    lv_obj_set_pos(circle, 10, 10);
    lv_obj_set_style_radius(circle, RADIUS_ICON, 0);
    lv_obj_set_style_bg_color(circle, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);

    row->icon = lv_label_create(circle);
    lv_label_set_text(row->icon, icon);
    lv_obj_set_style_text_font(row->icon, &lv_font_icon_36, 0);
    lv_obj_center(row->icon);

    row->lbl_name = lv_label_create(row->card);
    lv_obj_set_style_text_color(row->lbl_name, lv_color_white(), 0);
    lv_obj_set_style_text_font(row->lbl_name, &lv_font_inter_bold_14, 0);
    lv_label_set_long_mode(row->lbl_name, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(row->lbl_name, ROW_LABEL_W, 14);
    lv_obj_set_pos(row->lbl_name, 54, 10);
    lv_label_set_text(row->lbl_name, "");

    row->lbl_value = lv_label_create(row->card);
    lv_obj_set_style_text_color(row->lbl_value, lv_color_white(), 0);
    lv_obj_set_style_text_font(row->lbl_value, &lv_font_inter_bold_24, 0);
    lv_obj_set_pos(row->lbl_value, 54, 26);
    lv_label_set_text(row->lbl_value, "");
}

static void update_row(bike_row_t *row)
{
    if (!row->card || !row->dirty || should_skip_main_screen_update()) return;
    const uint8_t dirty = row->dirty;
    row->dirty = 0;

    if (row->state == ROW_HIDDEN) {
        lv_obj_add_flag(row->card, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (dirty & ROW_DIRTY_STATE) {
        const uint32_t color = row->state == ROW_ACTIVE ? row->color : row->color_dark;
        lv_obj_set_style_bg_color(row->card, lv_color_hex(color), 0);
        lv_obj_set_style_text_color(row->icon, lv_color_hex(color), 0);
        lv_obj_clear_flag(row->card, LV_OBJ_FLAG_HIDDEN);
    }
    if (dirty & ROW_DIRTY_NAME) {
        lv_label_set_text(row->lbl_name, row->name);
    }
    if (dirty & (ROW_DIRTY_STATE | ROW_DIRTY_VALUE)) {
        if (row->state == ROW_ACTIVE) {
            lv_label_set_text_fmt(row->lbl_value, "%u %s", (unsigned)row->value, row->unit);
        } else {
            lv_label_set_text(row->lbl_value, "Disconnected");
        }
    }
}

static void set_row(bike_row_t *row, row_state_t state, const char *name, uint16_t value)
{
    if (state != row->state) {
        /* Hidden rows keep no state: showing one again redraws all of it */
        row->dirty |= state == ROW_HIDDEN ? ROW_DIRTY_STATE : ROW_DIRTY_ALL;
        row->state = state;
    }
    if (name && strncmp(name, row->name, sizeof(row->name) - 1) != 0) {
        strncpy(row->name, name, sizeof(row->name) - 1);
        row->name[sizeof(row->name) - 1] = '\0';
        row->dirty |= ROW_DIRTY_NAME;
    }
    if (value != row->value) {
        row->value = value;
        if (state == ROW_ACTIVE) row->dirty |= ROW_DIRTY_VALUE;
    }
    if (row->dirty) update_row(row);
}

/* --- Fan Control (minus | mode, speed, dots | plus) --- */
static lv_obj_t *create_control_field(lv_obj_t *parent, lv_coord_t w, control_t ctrl)
{
    lv_obj_t *field = lv_obj_create(parent);
    lv_obj_set_size(field, w, LV_PCT(100));
    lv_obj_set_style_bg_color(field, lv_color_hex(COLOR_MODAL_CARD), 0);
    lv_obj_set_style_bg_opa(field, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(field, RADIUS_FIELD, 0);
    lv_obj_set_style_border_width(field, 0, 0);
    lv_obj_set_style_pad_all(field, 0, 0);
    lv_obj_clear_flag(field, LV_OBJ_FLAG_SCROLLABLE);
    make_control(field, ctrl);
    return field;
}

static lv_obj_t *create_field_icon(lv_obj_t *field, const char *icon)
{
    lv_obj_t *lbl = lv_label_create(field);
    lv_label_set_text(lbl, icon);
    lv_obj_set_style_text_font(lbl, &lv_font_icon_36, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
    return lbl;
}

static void create_fan_control(lv_obj_t *parent)
{
    lv_obj_t *control = lv_obj_create(parent);
    apply_transparent_container_style(control);
    lv_obj_set_size(control, LV_PCT(100), FAN_CONTROL_H);
    lv_obj_set_style_radius(control, RADIUS_CONTROL, 0);
    lv_obj_set_style_clip_corner(control, true, 0);
    lv_obj_set_flex_flow(control, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(control, 4, 0);

    lv_obj_t *minus = create_control_field(control, FAN_SIDE_W, CTRL_MINUS);
    lv_obj_set_style_bg_color(minus, lv_color_hex(COLOR_CONTROL_PRESSED), LV_STATE_PRESSED);
    create_field_icon(minus, ICON_MINUS);

    lv_obj_t *center = create_control_field(control, 0, CTRL_MODE);
    lv_obj_set_flex_grow(center, 1);
    lv_obj_set_style_bg_color(center, lv_color_hex(COLOR_CONTROL_PRESSED), LV_STATE_PRESSED);

    lbl_fan_mode = lv_label_create(center);
    lv_obj_set_style_text_font(lbl_fan_mode, &lv_font_inter_bold_24, 0);
    lv_obj_set_style_text_color(lbl_fan_mode, lv_color_white(), 0);
    lv_label_set_text(lbl_fan_mode, "Pure");
    /* Visible gaps 20 | mode | 20 | fan icon | 19 | dots | 20 (6 px higher than Figma) */
    lv_obj_align(lbl_fan_mode, LV_ALIGN_TOP_MID, 0, 19);

    lv_obj_t *target = lv_obj_create(center);
    apply_transparent_container_style(target);
    lv_obj_set_size(target, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(target, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(target, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    /* 6 px gap in Figma, the fan glyph brings 3 px side bearing */
    lv_obj_set_style_pad_column(target, 3, 0);
    lv_obj_align(target, LV_ALIGN_TOP_MID, 0, 54);

    lbl_fan_icon = lv_label_create(target);
    lv_label_set_text(lbl_fan_icon, ICON_FAN);
    lv_obj_set_style_text_font(lbl_fan_icon, &lv_font_icon_36, 0);

    /* 29 px box like Figma's line: baseline 23 px from the top */
    lbl_fan_pct = lv_label_create(target);
    lv_obj_set_style_text_font(lbl_fan_pct, &lv_font_inter_bold_24, 0);
    lv_obj_set_style_pad_top(lbl_fan_pct, 5, 0);
    lv_obj_set_style_pad_bottom(lbl_fan_pct, 1, 0);
    lv_label_set_text(lbl_fan_pct, "0");

    lv_obj_t *dots = create_fan_dots(center, fan_dots);
    lv_obj_align(dots, LV_ALIGN_TOP_MID, 0, 100);

    lv_obj_t *plus = create_control_field(control, FAN_SIDE_W, CTRL_PLUS);
    lv_obj_set_style_bg_color(plus, lv_color_hex(COLOR_CONTROL_PRESSED), LV_STATE_PRESSED);
    create_field_icon(plus, ICON_PLUS);
}

static const char *fan_mode_name(fan_mode_t mode)
{
    switch (mode) {
        case FAN_MODE_SMART:  return "Smart";
        case FAN_MODE_HYBRID: return "Hybrid";
        default:              return "Pure";
    }
}

static void update_fan_control(void)
{
    if ((!fan_control_dirty && !fan_dots_dirty) || should_skip_main_screen_update()) return;

    if (fan_control_dirty) {
        fan_control_dirty = false;
        const bool running = fan_target_on && fan_pct > 0;
        const lv_color_t color = lv_color_hex(running ? COLOR_GREEN : COLOR_WHITE);
        lv_label_set_text(lbl_fan_mode, fan_mode_name(fan_mode));
        lv_obj_set_style_text_color(lbl_fan_icon, color, 0);
        lv_obj_set_style_text_color(lbl_fan_pct, color, 0);
        lv_label_set_text_fmt(lbl_fan_pct, "%u", running ? (unsigned)fan_pct : 0u);
    }
    if (fan_dots_dirty) {
        fan_dots_dirty = false;
        for (uint8_t i = 0; i < MAX_FANS; i++) {
            lv_obj_set_style_bg_color(fan_dots[i], lv_color_hex(fan_dot_color(fans[i].state, fans[i].is_on)), 0);
        }
    }
}

/* --- Screensaver content --- */
static void update_powersave_display(void)
{
    powersave_view_t v;
    v.hr_state = hr_current_state;
    v.hr_value = hr_value;
    v.power_state = power_row.state;
    v.power_w = power_row.value;
    v.cadence_state = cadence_row.state;
    v.cadence_rpm = cadence_row.value;
    v.app = app_badge;
    v.trainer = trainer_badge;
    for (uint8_t i = 0; i < MAX_FANS; i++) {
        v.fan_state[i] = fans[i].state;
        v.fan_on[i] = fans[i].is_on;
    }
    v.fan_target_on = fan_target_on;
    v.fan_pct = fan_pct;
    update_powersave_view(&v);
}

static bool should_skip_main_screen_update(void)
{
    /* Boot/AP/Powersave screen active: skip main screen updates */
    if (lv_scr_act() != scr_main) {
        if (get_ui_power_state() >= UI_POWER_STATE_SOFT_POWERSAVE) {
            update_powersave_display();
        }
        return true;
    }

    if (settings_modal_visible) {
        ui_refresh_pending = true;
        return true;
    }

    return false;
}

static void update_main_widgets(void)
{
    hr_widget_cache_initialized = false;
    badges_dirty = true;
    power_row.dirty = ROW_DIRTY_ALL;
    cadence_row.dirty = ROW_DIRTY_ALL;
    fan_control_dirty = true;
    fan_dots_dirty = true;

    update_badges();
    update_hr_widget();
    update_row(&power_row);
    update_row(&cadence_row);
    update_fan_control();
}

/* --- Settings Modal --- */
static void create_settings_modal(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t w = lv_disp_get_hor_res(disp);
    lv_coord_t h = lv_disp_get_ver_res(disp);

    settings_overlay = lv_obj_create(scr_main);
    lv_obj_set_size(settings_overlay, w, h);
    lv_obj_set_style_bg_color(settings_overlay, lv_color_hex(COLOR_MODAL_BG), 0);
    lv_obj_set_style_bg_opa(settings_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(settings_overlay, 0, 0);
    lv_obj_set_style_radius(settings_overlay, 0, 0);
    lv_obj_set_style_pad_all(settings_overlay, 0, 0);
    lv_obj_clear_flag(settings_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(settings_overlay, 0, MODAL_SLIDE_DISTANCE);
    lv_obj_add_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);
    make_control(settings_overlay, CTRL_NONE);

    /* QR area 244 × 244 */
    lv_obj_t *qr_area = lv_obj_create(settings_overlay);
    apply_transparent_container_style(qr_area);
    lv_obj_set_size(qr_area, 244, 244);
    lv_obj_align(qr_area, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_radius(qr_area, RADIUS_QR, 0);
    lv_obj_set_style_bg_color(qr_area, lv_color_hex(COLOR_QR_AREA), 0);
    lv_obj_set_style_bg_opa(qr_area, LV_OPA_COVER, 0);

    qr_code_widget = lv_qrcode_create(qr_area, 203, lv_color_white(), lv_color_hex(COLOR_QR_AREA));
    lv_qrcode_update(qr_code_widget, qr_code_url, strlen(qr_code_url));
    lv_obj_center(qr_code_widget);

    /* 291 px in one line, 254 px available: two lines */
    lv_obj_t *lbl_scan = lv_label_create(settings_overlay);
    lv_label_set_text(lbl_scan, "Scan to Setup\nHomewind");
    lv_obj_set_style_text_align(lbl_scan, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_scan, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_scan, &lv_font_inter_bold_24, 0);
    lv_obj_set_style_text_line_space(lbl_scan, 3, 0);
    lv_obj_align(lbl_scan, LV_ALIGN_TOP_MID, 0, 282);

    /* Close Button */
    lv_obj_t *btn_close = lv_obj_create(settings_overlay);
    lv_obj_set_size(btn_close, 260, 72);
    lv_obj_align(btn_close, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(COLOR_ACCENT_PRESSED), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn_close, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_close, 0, 0);
    lv_obj_set_style_radius(btn_close, RADIUS_BUTTON, 0);
    lv_obj_clear_flag(btn_close, LV_OBJ_FLAG_SCROLLABLE);
    make_control(btn_close, CTRL_CLOSE);

    lv_obj_t *lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, "Close");
    lv_obj_set_style_text_font(lbl_close, &lv_font_inter_bold_24, 0);
    lv_obj_set_style_text_color(lbl_close, lv_color_white(), 0);
    lv_obj_center(lbl_close);
    lv_obj_set_style_translate_y(lbl_close, 2, 0);
}

/* --- Boot and AP Screen: heart, title, body centred in the top 369 px --- */
static void create_boot_ap_column(lv_obj_t *scr, const char *title, const char *body)
{
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG_APP), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *cont = lv_obj_create(scr);
    apply_transparent_container_style(cont);
    lv_obj_set_size(cont, LV_PCT(100), 369);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 0, 0);

    /* Heart 50 × 45 from icon 56 */
    lv_obj_t *lbl_icon = lv_label_create(cont);
    lv_label_set_text(lbl_icon, ICON_HEART);
    lv_obj_set_style_text_font(lbl_icon, &lv_font_icon_56, 0);
    lv_obj_set_style_text_color(lbl_icon, lv_color_hex(COLOR_ACCENT), 0);

    /* Figma gaps 20 and 8 between line boxes; the top paddings put the
     * baselines there with the tighter LVGL lines (Bold 24: 23, Bold 14: 14) */
    lv_obj_t *lbl_title = lv_label_create(cont);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_font(lbl_title, &lv_font_inter_bold_24, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_pad_top(lbl_title, 24, 0);

    lv_obj_t *lbl_body = lv_label_create(cont);
    lv_label_set_text(lbl_body, body);
    lv_obj_set_style_text_font(lbl_body, &lv_font_inter_bold_14, 0);
    lv_obj_set_style_text_color(lbl_body, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_align(lbl_body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(lbl_body, 4, 0);
    lv_obj_set_style_pad_top(lbl_body, 10, 0);
    lv_obj_set_width(lbl_body, 190);
    lv_label_set_long_mode(lbl_body, LV_LABEL_LONG_WRAP);
}

static void create_boot_screen(void)
{
    scr_boot = lv_obj_create(NULL);
    create_boot_ap_column(scr_boot, "Warming Up", "Starting Homewind\nPlease Wait");
}

/* --- AP Screen (Wifisetup) --- */
static void create_ap_screen(void)
{
    scr_apmode = lv_obj_create(NULL);
    create_boot_ap_column(scr_apmode, "Wifisetup",
                          "Go to Wifi and Connect to the Homewind Device to Setup Wifi");
}

/* --- Main Screen: (44 + HR, grows) + 56 + 56 + 132, gap 8 --- */
static void create_main_screen(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t w = lv_disp_get_hor_res(disp);
    lv_coord_t h = lv_disp_get_ver_res(disp);

    scr_main = lv_obj_create(NULL);
    lv_obj_clear_flag(scr_main, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr_main, lv_color_hex(COLOR_BG_APP), 0);
    lv_obj_set_style_bg_opa(scr_main, LV_OPA_COVER, 0);
    make_control(scr_main, CTRL_NONE);

    lv_obj_t *stack = lv_obj_create(scr_main);
    apply_transparent_container_style(stack);
    lv_obj_set_size(stack, w, h);
    lv_obj_set_style_pad_all(stack, SCREEN_INSET, 0);
    lv_obj_set_flex_flow(stack, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(stack, SCREEN_GAP, 0);

    /* Status bar and HR card in one pink block, rounded at the bottom; a tap opens the QR modal */
    lv_obj_t *top = lv_obj_create(stack);
    apply_transparent_container_style(top);
    lv_obj_set_width(top, LV_PCT(100));
    lv_obj_set_flex_grow(top, 1);
    lv_obj_set_style_bg_color(top, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(top, RADIUS_CARD, 0);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(top, 0, 0);
    make_control(top, CTRL_SETTINGS);

    create_status_bar(top);
    create_hr_widget(top);
    create_row(stack, &power_row, ICON_BOLT);
    create_row(stack, &cadence_row, ICON_CRANK);
    create_fan_control(stack);

    create_settings_modal();
    update_main_widgets();
}

/* --- Public API --- */
void homewind_create_screens(void)
{
    create_boot_screen();
    create_ap_screen();
    create_main_screen();
    lv_scr_load(scr_boot);

    // NOTE: powersave_init() is now called separately by homewind_init()
    // powersave starts with powersave_locked=true (Boot screen active)
}

void homewind_show_ap_screen(void)
{
    if (scr_apmode) {
        lv_scr_load(scr_apmode);
    }
}

void homewind_show_main_screen(void)
{
    if (scr_main) {
        lv_scr_load(scr_main);
        homewind_refresh_main_display();
    }
}

void homewind_refresh_powersave_display(void)
{
    update_powersave_display();
}

void homewind_refresh_main_display(void)
{
    update_main_widgets();
}

/* ============================================================================
 * HR Widget API Implementation (with Change-Guards - O1 optimization)
 * ============================================================================ */

void homewind_set_hr(hr_state_t state, const char* sensor_name, uint16_t value)
{
    bool changed = false;
    if (state != hr_current_state) { hr_current_state = state; changed = true; }
    if (sensor_name && strcmp(sensor_name, hr_sensor_name) != 0) {
        strncpy(hr_sensor_name, sensor_name, sizeof(hr_sensor_name) - 1);
        hr_sensor_name[sizeof(hr_sensor_name) - 1] = '\0';
        changed = true;
    }
    if (value != hr_value) { hr_value = value; changed = true; }
    if (changed) update_hr_widget();
}

void homewind_set_hr_state(hr_state_t state)
{
    if (state != hr_current_state) {
        hr_current_state = state;
        update_hr_widget();
    }
}

void homewind_set_hr_value(uint16_t value)
{
    if (value != hr_value) {
        hr_value = value;
        update_hr_widget();
    }
}

/* ============================================================================
 * Power and Cadence Rows
 * ============================================================================ */

void homewind_set_power_row(row_state_t state, const char* name, uint16_t watts)
{
    set_row(&power_row, state, name, watts);
}

void homewind_set_cadence_row(row_state_t state, const char* name, uint16_t rpm)
{
    set_row(&cadence_row, state, name, rpm);
}

static row_state_t row_from_csc(csc_state_t state)
{
    switch (state) {
        case CSC_STATE_ACTIVE:   return ROW_ACTIVE;
        case CSC_STATE_INACTIVE: return ROW_DISCONNECTED;
        default:                 return ROW_HIDDEN;
    }
}

void homewind_set_csc(csc_state_t state, const char* sensor_name, uint16_t cadence)
{
    set_row(&cadence_row, row_from_csc(state), sensor_name, cadence);
}

void homewind_set_csc_state(csc_state_t state)
{
    set_row(&cadence_row, row_from_csc(state), NULL, cadence_row.value);
}

void homewind_set_csc_cadence(uint16_t cadence)
{
    set_row(&cadence_row, cadence_row.state, NULL, cadence);
}

void homewind_set_csc_power(int32_t watts)
{
    if (watts < 0) {
        set_row(&power_row, ROW_HIDDEN, NULL, 0);
    } else {
        set_row(&power_row, ROW_ACTIVE, cadence_row.name, watts > 0xFFFF ? 0xFFFF : (uint16_t)watts);
    }
}

/* ============================================================================
 * Status Badges
 * ============================================================================ */

void homewind_set_badges(badge_state_t app, badge_state_t trainer)
{
    if (app == app_badge && trainer == trainer_badge) return;
    app_badge = app;
    trainer_badge = trainer;
    badges_dirty = true;
    update_badges();
}

/* ============================================================================
 * Fan Control API Implementation (with Change-Guards - O1 optimization)
 * ============================================================================ */

void homewind_set_fan(uint8_t fan_index, fan_state_t state, bool is_on)
{
    if (fan_index >= MAX_FANS) return;
    if (fans[fan_index].state == state && fans[fan_index].is_on == is_on) return;
    fans[fan_index].state = state;
    fans[fan_index].is_on = is_on;
    fan_dots_dirty = true;
    update_fan_control();
}

void homewind_set_fan_state(uint8_t fan_index, fan_state_t state)
{
    if (fan_index >= MAX_FANS) return;
    const bool is_on = (state == FAN_STATE_NOT_CONFIGURATED || state == FAN_STATE_INACTIVE)
                           ? false : fans[fan_index].is_on;
    homewind_set_fan(fan_index, state, is_on);
}

void homewind_set_fan_toggle(uint8_t fan_index, bool is_on)
{
    if (fan_index >= MAX_FANS) return;
    homewind_set_fan(fan_index, fans[fan_index].state, is_on);
}

void homewind_set_fan_control(fan_mode_t mode, bool target_on, uint8_t target_pct)
{
    if (mode == fan_mode && target_on == fan_target_on && target_pct == fan_pct) return;
    fan_mode = mode;
    fan_target_on = target_on;
    fan_pct = target_pct;
    fan_control_dirty = true;
    update_fan_control();
}

void homewind_set_fan_control_callbacks(hr_shift_callback_t shift, fan_mode_callback_t mode)
{
    hr_shift_callback = shift;
    fan_mode_callback = mode;
}

void homewind_set_fan_toggle_callback(fan_toggle_callback_t callback)
{
    fan_toggle_callback = callback;
}

void homewind_set_qr_code_url(const char* url)
{
    if (!url || !qr_code_widget) return;
    strncpy(qr_code_url, url, sizeof(qr_code_url) - 1);
    qr_code_url[sizeof(qr_code_url) - 1] = '\0';
    lv_qrcode_update(qr_code_widget, qr_code_url, strlen(qr_code_url));
}

void homewind_hide_settings_overlay(void)
{
    if (settings_overlay) {
        lv_anim_del(settings_overlay, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_obj_add_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    /* Callers refresh the main screen themselves (powersave wake) */
    settings_modal_visible = false;
    ui_refresh_pending = false;
}

/* --- Library Initialization (idempotent) --- */
static bool homewind_initialized = false;

void homewind_init(void)
{
    /* Guard: Only initialize once */
    if (homewind_initialized) {
        return;
    }
    
    /* 1. Initialize touch controller */
    extern void Touch_Init(void);
    Touch_Init();
    
    /* 2. Initialize display, SPI, LVGL core, and start LVGL task */
    extern void lcd_lvgl_Init(void);
    lcd_lvgl_Init();
    
    /* 3. Create UI screens (main screen with widgets, settings modal) and
     * 4. initialize power save (timers, breathing animation). The LVGL task
     * runs already, so both need the lock. */
    extern void powersave_init(void);
    extern bool lcd_lvgl_lock(int timeout_ms);
    extern void lcd_lvgl_unlock(void);
    lcd_lvgl_lock(-1);
    homewind_create_screens();
    powersave_init();
    lcd_lvgl_unlock();
    
    /* 5. Start IMU rotation task if QMI8658 hardware is available */
    #ifdef QMI8658_SLAVE_ADDR_L
    extern bool lcd_start_imu_rotation_task(void);
    lcd_start_imu_rotation_task();
    #endif
    
    homewind_initialized = true;
}
