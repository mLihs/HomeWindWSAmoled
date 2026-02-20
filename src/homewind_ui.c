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
#define RADIUS_CARD             36
#define RADIUS_WIDGET           36
#define RADIUS_BUTTON           36
#define RADIUS_ICON             18

/* --- Animation Timing --- */
#define MODAL_ANIMATION_TIME_MS  300  // Animation duration for modal overlay
#define FAN_TOGGLE_ANIMATION_TIME_MS  300  // Animation duration for fan toggle
#define FAN_ANIM_RESOLUTION  1024  // Animation resolution for color interpolation

/* --- Global Objects --- */
lv_obj_t *scr_main;  // Made non-static for powersave module access
static lv_obj_t *scr_boot = NULL;
static lv_obj_t *scr_apmode = NULL;
static lv_obj_t *settings_overlay = NULL;
static lv_obj_t *settings_card = NULL;
static lv_obj_t *qr_code_widget = NULL;

/* FIX Issue #2: Track modal visibility to skip LVGL updates when not visible */
static bool settings_modal_visible = false;
static bool ui_refresh_pending = false;  // Flag to refresh UI on modal close

/* --- HR Widget Objects --- */
static lv_obj_t *card_hr;
static lv_obj_t *row_value;  // Row container for HR value and icons
static lv_obj_t *lbl_hr_value;
static lv_obj_t *lbl_hr_heart;  // Heart icon using icon font (U+31)
static lv_obj_t *lbl_hr_disconnected;  // Disconnected icon (U+34)
static lv_obj_t *lbl_hr_sensor_name;

/* --- CSC Widget Objects --- */
static lv_obj_t *card_csc;
static lv_obj_t *csc_icon_group;
static lv_obj_t *csc_icon_label;  // Error icon (U+33) for inactive/error states
static lv_obj_t *csc_checkmark_label;  // Checkmark icon (U+32) for active state
static lv_obj_t *lbl_csc_sensor_name;
static lv_obj_t *lbl_csc_status;
static csc_state_t csc_current_state = CSC_NOT_CONFIGURATED;
static char csc_sensor_name[32] = "CSC Sensor";
static uint16_t csc_cadence_value = 0;

/* --- Fan Widget Objects (Figma: pill + circle + state text only) --- */
#define MAX_FANS 4
static lv_obj_t *fan_container;
static struct {
    lv_obj_t *card;       /* Pill-shaped container */
    lv_obj_t *icon_group;  /* Circle only (green=off, white=on); hidden for Error/Not Configurated */
    lv_obj_t *lbl_name;    /* State text: "Off", "On", "Error"; hidden for Not Configurated */
    fan_state_t state;
    bool is_on;
} fan_items[MAX_FANS];

/* --- Fan UI Cache (for dirty-checking in update_fan_item) --- */
typedef struct {
    fan_state_t state;
    bool is_on;
    bool initialized;
} fan_ui_cache_t;
static fan_ui_cache_t fan_ui_cache[MAX_FANS] = {0};

/* --- Fan Toggle Callback --- */
typedef void (*fan_toggle_callback_t)(uint8_t fan_index, bool is_on);
static fan_toggle_callback_t fan_toggle_callback = NULL;

/* --- State Variables --- */
static hr_state_t hr_current_state = HR_NOT_CONFIGURATED;
static uint16_t hr_value = 0;
static char hr_sensor_name[32] = "";
static char qr_code_url[128] = "https://martin-lihs.com";

/* --- Cached formatted strings (avoid repeated snprintf calls) --- */
static char hr_value_str[8] = "--";
static uint16_t hr_value_str_cached = 0xFFFF;  /* Force first format */
static char csc_cadence_str[16] = "0 RPM";
static uint16_t csc_cadence_str_cached = 0;

/* --- HR widget cache (reduces redundant LVGL updates) --- */
static hr_state_t hr_widget_cache_state = (hr_state_t)-1;
static uint16_t hr_widget_cache_value = 0xFFFF;
static char hr_widget_cache_name[32] = "";
static bool hr_widget_cache_initialized = false;

/* ============================================================================
 * O3: Pre-defined Styles (created once, reused on state changes)
 * ============================================================================
 * Benefits:
 * - Fewer LVGL setter calls in update functions
 * - Less internal invalidation/layout rebuilds
 * - Clearer state machine (state → style mapping)
 * ============================================================================ */
static bool styles_initialized = false;

/* CSC Widget Styles */
static lv_style_t style_csc_active;
static lv_style_t style_csc_inactive;

static void init_styles(void)
{
    if (styles_initialized) return;
    
    /* CSC Active Style */
    lv_style_init(&style_csc_active);
    lv_style_set_bg_color(&style_csc_active, lv_color_hex(COLOR_CSC));
    lv_style_set_bg_opa(&style_csc_active, LV_OPA_COVER);
    lv_style_set_border_width(&style_csc_active, 0);
    
    /* CSC Inactive Style (LV_OPA_COVER to avoid gray band at top edge from 50% opacity) */
    lv_style_init(&style_csc_inactive);
    lv_style_set_bg_color(&style_csc_inactive, lv_color_hex(COLOR_CSC));
    lv_style_set_bg_opa(&style_csc_inactive, LV_OPA_COVER);
    lv_style_set_border_width(&style_csc_inactive, 0);
    
    styles_initialized = true;
}

/* --- Forward Declarations --- */
static void animate_overlay_in(void);
static void animate_overlay_out(void);
static void anim_overlay_out_ready_cb(lv_anim_t *a);
static void settings_btn_event_cb(lv_event_t *e);
static void close_btn_event_cb(lv_event_t *e);
static void fan_toggle_event_cb(lv_event_t *e);
static void create_boot_screen(void);
static void create_ap_screen(void);
static void create_main_screen(void);
static void create_hr_widget(lv_obj_t *parent);
static void create_csc_widget(lv_obj_t *parent);
static void create_fan_widget(lv_obj_t *parent);
static void create_settings_modal(void);
static void update_hr_widget(void);
static void update_csc_widget(void);
static void update_fan_item(uint8_t index);
static void animate_fan_toggle(uint8_t index);
static const char* get_hr_value_string(void);
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
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t h = lv_disp_get_ver_res(disp);

    /* FIX Issue #2: Mark modal as visible (skip UI updates while open) */
    settings_modal_visible = true;
    ui_refresh_pending = false;

    lv_obj_set_y(settings_overlay, h + 6);  // Start from off-screen position
    lv_obj_clear_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t a;
    start_anim(&a, settings_overlay, h + 6, -4, MODAL_ANIMATION_TIME_MS,
               lv_anim_path_ease_in_out, (lv_anim_exec_xcb_t)lv_obj_set_y, NULL, NULL);
}

static void anim_overlay_out_ready_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    lv_obj_add_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);
    
    /* FIX Issue #2: Mark modal as hidden and refresh UI if changes occurred */
    settings_modal_visible = false;
    if (ui_refresh_pending) {
        ui_refresh_pending = false;
        /* Force refresh all widgets by invalidating caches */
        /* HR widget uses hr_value_str_cached, CSC uses csc_cadence_str_cached */
        /* Fan widget uses fan_ui_cache[] - we need to reset initialized flag */
        hr_widget_cache_initialized = false;
        for (uint8_t i = 0; i < MAX_FANS; i++) {
            fan_ui_cache[i].initialized = false;
        }
        /* Trigger full UI update */
        update_hr_widget();
        update_csc_widget();
        for (uint8_t i = 0; i < MAX_FANS; i++) {
            if (fan_items[i].card) {
                update_fan_item(i);
            }
        }
    }
}

static void animate_overlay_out(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t h = lv_disp_get_ver_res(disp);

    lv_anim_t a;
    start_anim(&a, settings_overlay, lv_obj_get_y(settings_overlay), h + 4, MODAL_ANIMATION_TIME_MS,
               lv_anim_path_ease_in_out, (lv_anim_exec_xcb_t)lv_obj_set_y, anim_overlay_out_ready_cb, NULL);
}

/* --- Fan Toggle Animation (static allocation - no heap fragmentation) --- */
typedef struct {
    lv_obj_t *obj;
    uint32_t start_color_hex;
    uint32_t end_color_hex;
    uint8_t fan_index;  // To identify which fan this belongs to
} fan_color_anim_data_t;

// Static animation data for all fans (avoids lv_mem_alloc/free per animation)
static fan_color_anim_data_t fan_anim_data[MAX_FANS];

static void fan_bg_color_anim_cb(void *var, int32_t value)
{
    fan_color_anim_data_t *data = (fan_color_anim_data_t *)var;
    if (!data || !data->obj) return;
    
    /* Interpolate between start and end colors using hex values */
    uint32_t start = data->start_color_hex;
    uint32_t end = data->end_color_hex;
    
    /* Extract RGB components from hex */
    uint8_t r_start = (start >> 16) & 0xFF;
    uint8_t g_start = (start >> 8) & 0xFF;
    uint8_t b_start = start & 0xFF;
    
    uint8_t r_end = (end >> 16) & 0xFF;
    uint8_t g_end = (end >> 8) & 0xFF;
    uint8_t b_end = end & 0xFF;
    
    /* Interpolate */
    int32_t r = r_start + ((r_end - r_start) * value) / FAN_ANIM_RESOLUTION;
    int32_t g = g_start + ((g_end - g_start) * value) / FAN_ANIM_RESOLUTION;
    int32_t b = b_start + ((b_end - b_start) * value) / FAN_ANIM_RESOLUTION;
    
    /* Clamp values */
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    
    lv_color_t color = lv_color_make((uint8_t)r, (uint8_t)g, (uint8_t)b);
    lv_obj_set_style_bg_color(data->obj, color, 0);
}

static void fan_color_anim_ready_cb(lv_anim_t *a)
{
    fan_color_anim_data_t *data = (fan_color_anim_data_t *)lv_anim_get_user_data(a);
    if (!data || !data->obj) return;
    
    /* Ensure final color is set */
    lv_obj_set_style_bg_color(data->obj, lv_color_hex(data->end_color_hex), 0);
    
    /* No need to free - using static allocation */
    /* Just clear the obj pointer to indicate animation is done */
    data->obj = NULL;
}

static void animate_fan_toggle(uint8_t index)
{
    if (index >= MAX_FANS || !fan_items[index].card) return;
    
    /* Use static animation data for this fan index */
    fan_color_anim_data_t *data = &fan_anim_data[index];
    
    /* Stop any existing animation on this fan first */
    lv_anim_del(data, (lv_anim_exec_xcb_t)fan_bg_color_anim_cb);
    
    /* Get current background color */
    lv_color_t start_color = lv_obj_get_style_bg_color(fan_items[index].card, 0);
    
    /* Convert lv_color_t to hex (RGB format: 0xRRGGBB) */
    uint32_t start_color_hex = 0;
    #if LV_COLOR_DEPTH == 16
        /* For 16-bit color, extract RGB565 components */
        uint16_t c = start_color.full;
        uint8_t r = ((c >> 11) & 0x1F) << 3;
        uint8_t g = ((c >> 5) & 0x3F) << 2;
        uint8_t b = (c & 0x1F) << 3;
        start_color_hex = (r << 16) | (g << 8) | b;
    #elif LV_COLOR_DEPTH == 32
        /* For 32-bit color, extract ARGB8888 components */
        uint32_t c = start_color.full;
        uint8_t r = (c >> 16) & 0xFF;
        uint8_t g = (c >> 8) & 0xFF;
        uint8_t b = c & 0xFF;
        start_color_hex = (r << 16) | (g << 8) | b;
    #else
        /* For 8-bit color, use direct mapping */
        start_color_hex = start_color.full;
    #endif
    
    /* Determine end color hex based on state and is_on */
    uint32_t end_color_hex;
    if (fan_items[index].state == FAN_STATE_NOT_CONFIGURATED) {
        return;  // No animation for not configured
    }
    if (fan_items[index].state == FAN_STATE_ACTIVE) {
        end_color_hex = fan_items[index].is_on ? 
            COLOR_FAN_ACTIVE_ON : 
            COLOR_WHITE;
    } else if (fan_items[index].state == FAN_STATE_ERROR) {
        return;  /* Error has no on/off, no animation */
    } else if (fan_items[index].state == FAN_STATE_INACTIVE) {
        end_color_hex = COLOR_WHITE;  // Fan off pill
    } else {
        return;
    }
    
    /* Setup static animation data (no heap allocation!) */
    data->obj = fan_items[index].card;
    data->start_color_hex = start_color_hex;
    data->end_color_hex = end_color_hex;
    data->fan_index = index;
    
    /* Animate background color */
    lv_anim_t a;
    start_anim(&a, data, 0, FAN_ANIM_RESOLUTION, FAN_TOGGLE_ANIMATION_TIME_MS,
               lv_anim_path_ease_in_out, (lv_anim_exec_xcb_t)fan_bg_color_anim_cb,
               fan_color_anim_ready_cb, data);
}

/* --- Event Handlers --- */
static void settings_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    on_user_activity();  /* wake from DIMMED + reset timer (event goes to widget, not scr_main) */
    animate_overlay_in();
}

static void close_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        animate_overlay_out();
    }
}

static void fan_toggle_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    on_user_activity();  /* wake from DIMMED + reset timer (event goes to widget, not scr_main) */
    uint8_t fan_index = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (fan_index >= MAX_FANS) return;
    
    if (fan_items[fan_index].state == FAN_STATE_NOT_CONFIGURATED) return;
    if (fan_items[fan_index].state == FAN_STATE_INACTIVE) return;  /* not toggleable */
    
    /* Error: no is_on toggle, only callback (e.g. for retry or message) */
    if (fan_items[fan_index].state == FAN_STATE_ERROR) {
        if (fan_toggle_callback) fan_toggle_callback(fan_index, false);
        return;
    }
    
    /* ACTIVE: toggle is_on, callback, update UI */
    fan_items[fan_index].is_on = !fan_items[fan_index].is_on;
    if (fan_toggle_callback) {
        fan_toggle_callback(fan_index, fan_items[fan_index].is_on);
    }
    update_fan_item(fan_index);
    animate_fan_toggle(fan_index);
}

/* --- HR Widget --- */
static void create_hr_widget(lv_obj_t *parent)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t w = lv_disp_get_hor_res(disp);

    card_hr = lv_obj_create(parent);
    lv_obj_set_size(card_hr, LV_PCT(100), 144);
    lv_obj_set_style_bg_color(card_hr, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_bg_opa(card_hr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card_hr, RADIUS_CARD, 0);
    lv_obj_set_style_border_width(card_hr, 0, 0);
    lv_obj_set_style_pad_left(card_hr, 8, 0);
    lv_obj_set_style_pad_right(card_hr, 8, 0);
    lv_obj_set_style_pad_top(card_hr, 8, 0);
    lv_obj_set_style_pad_bottom(card_hr, 16, 0);
    lv_obj_clear_flag(card_hr, LV_OBJ_FLAG_SCROLLABLE);

    /* HR Value and Icon Row */
    row_value = lv_obj_create(card_hr);
    apply_transparent_container_style(row_value);
    lv_obj_set_size(row_value, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_align(row_value, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_flex_flow(row_value, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_value, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(row_value, 10, 0);

    /* HR Value Label */
    lbl_hr_value = lv_label_create(row_value);
    lv_label_set_text(lbl_hr_value, get_hr_value_string());
    lv_obj_set_style_text_color(lbl_hr_value, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_hr_value, &lv_font_inter_black_64, 0);
    lv_obj_set_style_text_letter_space(lbl_hr_value, -1, 0);

    /* Heart Icon - Using icon font (U+31 = heart) for connected state */
    lbl_hr_heart = lv_label_create(row_value);
    lv_label_set_text(lbl_hr_heart, "1");  // U+31 = heart icon
    lv_obj_set_style_text_color(lbl_hr_heart, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_hr_heart, &lv_font_icon_56, 0);
    lv_obj_set_size(lbl_hr_heart, 56, 56);
    lv_obj_set_style_translate_y(lbl_hr_heart, 0, 0);

    /* Disconnected Icon - Using icon font (U+34) for disconnected state */
    lbl_hr_disconnected = lv_label_create(row_value);
    lv_label_set_text(lbl_hr_disconnected, "4");  // U+34 = disconnected icon
    lv_obj_set_style_text_color(lbl_hr_disconnected, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_hr_disconnected, &lv_font_icon_56, 0);
    lv_obj_set_size(lbl_hr_disconnected, 56, 56);
    lv_obj_set_style_translate_y(lbl_hr_disconnected, 0, 0);
    lv_obj_add_flag(lbl_hr_disconnected, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    /* Sensor Name Label */
    lbl_hr_sensor_name = lv_label_create(card_hr);
    lv_label_set_text(lbl_hr_sensor_name, hr_sensor_name);
    lv_obj_set_style_text_color(lbl_hr_sensor_name, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_hr_sensor_name, &lv_font_inter_bold_24, 0);
    lv_obj_align(lbl_hr_sensor_name, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_text_align(lbl_hr_sensor_name, LV_TEXT_ALIGN_CENTER, 0);
    
    update_hr_widget();
}

/* --- Helper: count configured fans (any state except NOT_CONFIGURATED) --- */
static uint8_t count_configured_fans(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_FANS; i++) {
        if (fan_items[i].state != FAN_STATE_NOT_CONFIGURATED) {
            count++;
        }
    }
    return count;
}

/* --- Helper: count fans that are currently ON (among configured) --- */
static uint8_t count_fans_on(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_FANS; i++) {
        if (fan_items[i].state != FAN_STATE_NOT_CONFIGURATED && fan_items[i].is_on) {
            count++;
        }
    }
    return count;
}

/* --- Update powersave screen when sensor states change --- */
static void update_powersave_display(void)
{
    uint8_t fan_configured = count_configured_fans();
    uint8_t fan_active_count = count_fans_on();
    update_powersave_icons(hr_current_state, csc_current_state,
                          hr_value, csc_cadence_value, fan_active_count, fan_configured);
}

/* Helper: Get cached HR value string (only format if value changed) */
static const char* get_hr_value_string(void)
{
    if (hr_value != hr_value_str_cached) {
        if (hr_value == 0) {
            hr_value_str[0] = '-';
            hr_value_str[1] = '-';
            hr_value_str[2] = '\0';
        } else {
            snprintf(hr_value_str, sizeof(hr_value_str), "%d", hr_value);
        }
        hr_value_str_cached = hr_value;
    }
    return hr_value_str;
}

/* Helper: Get cached CSC cadence string (only format if value changed) */
static const char* get_csc_cadence_string(void)
{
    if (csc_cadence_value != csc_cadence_str_cached) {
        snprintf(csc_cadence_str, sizeof(csc_cadence_str), "%d RPM", csc_cadence_value);
        csc_cadence_str_cached = csc_cadence_value;
    }
    return csc_cadence_str;
}

static bool should_skip_main_screen_update(void)
{
    /* Boot/AP/Powersave screen active: skip main screen updates */
    if (lv_scr_act() != scr_main) {
        if (get_ui_power_state() == UI_POWER_STATE_SOFT_POWERSAVE) {
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

    switch (hr_current_state) {
        case HR_STATE_ACTIVE:
            /* Active: Show HR value and heart icon */
            if (lbl_hr_value) {
                lv_obj_clear_flag(lbl_hr_value, LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(lbl_hr_value, get_hr_value_string());
            }
            if (lbl_hr_heart) {
                lv_obj_clear_flag(lbl_hr_heart, LV_OBJ_FLAG_HIDDEN);
            }
            if (lbl_hr_disconnected) {
                lv_obj_add_flag(lbl_hr_disconnected, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_translate_y(lbl_hr_disconnected, -2, 0);
            }
            lv_obj_set_style_translate_y(row_value, 0, 0);
            if (lbl_hr_sensor_name) {
                lv_label_set_text(lbl_hr_sensor_name, hr_sensor_name);
                lv_obj_set_style_translate_y(lbl_hr_sensor_name, 0, 0);
            }
            break;
            
        case HR_STATE_INACTIVE:
            /* Inactive: Sensor configured but not connected - show name + "Not Connected" */
            if (lbl_hr_value) {
                lv_obj_add_flag(lbl_hr_value, LV_OBJ_FLAG_HIDDEN);
            }
            if (lbl_hr_heart) {
                lv_obj_add_flag(lbl_hr_heart, LV_OBJ_FLAG_HIDDEN);
            }
            if (lbl_hr_disconnected) {
                lv_obj_clear_flag(lbl_hr_disconnected, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_translate_y(row_value, -10, 0);
            }
            if (lbl_hr_sensor_name) {
                if (hr_sensor_name && strlen(hr_sensor_name) > 0) {
                    char error_text[64];
                    snprintf(error_text, sizeof(error_text), "%s\nNot Connected", hr_sensor_name);
                    lv_label_set_text(lbl_hr_sensor_name, error_text);
                    lv_obj_set_style_translate_y(lbl_hr_sensor_name, 12, 0);
                } else {
                    lv_label_set_text(lbl_hr_sensor_name, "Not Connected");
                }
            }
            break;
            
        case HR_NOT_CONFIGURATED:
            /* Not configured: Show disconnected icon and "Not Configured" */
            if (lbl_hr_value) {
                lv_obj_add_flag(lbl_hr_value, LV_OBJ_FLAG_HIDDEN);
            }
            if (lbl_hr_heart) {
                lv_obj_add_flag(lbl_hr_heart, LV_OBJ_FLAG_HIDDEN);
            }
            if (lbl_hr_disconnected) {
                lv_obj_clear_flag(lbl_hr_disconnected, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_translate_y(lbl_hr_disconnected, -2, 0);
            }
            lv_obj_set_style_translate_y(row_value, 0, 0);
            if (lbl_hr_sensor_name) {
                lv_label_set_text(lbl_hr_sensor_name, "Not Configured");
                lv_obj_set_style_translate_y(lbl_hr_sensor_name, 0, 0);
            }
            break;
    }
    
    /* Update powersave screen if it exists */
    update_powersave_display();
}

/* --- CSC Widget --- */
static void create_csc_widget(lv_obj_t *parent)
{
    card_csc = lv_obj_create(parent);
    lv_obj_set_size(card_csc, LV_PCT(100), 64);
    lv_obj_set_style_radius(card_csc, RADIUS_WIDGET, 0);
    lv_obj_set_style_border_width(card_csc, 0, 0);
    lv_obj_set_style_pad_left(card_csc, 15, 0);
    lv_obj_set_style_pad_right(card_csc, 15, 0);
    lv_obj_set_style_pad_top(card_csc, 6, 0);
    lv_obj_set_style_pad_bottom(card_csc, 6, 0);
    lv_obj_clear_flag(card_csc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card_csc, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card_csc, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(card_csc, 8, 0);

    /* Icon Group */
    csc_icon_group = lv_obj_create(card_csc);
    lv_obj_set_size(csc_icon_group, 36, 36);
    lv_obj_set_style_radius(csc_icon_group, RADIUS_ICON, 0);
    lv_obj_set_style_border_width(csc_icon_group, 0, 0);
    lv_obj_set_style_pad_all(csc_icon_group, 0, 0);
    lv_obj_clear_flag(csc_icon_group, LV_OBJ_FLAG_SCROLLABLE);

    /* Icon Label - Using icon font (U+32 = checkmark, U+33 = error) */
    csc_icon_label = lv_label_create(csc_icon_group);
    lv_label_set_text(csc_icon_label, "3");  // U+33 = error icon (default for inactive/error)
    lv_obj_set_style_text_color(csc_icon_label, lv_color_hex(COLOR_CSC), 0);
    lv_obj_set_style_text_font(csc_icon_label, &lv_font_icon_36, 0);
    lv_obj_center(csc_icon_label);

    /* Checkmark Label (for active state) */
    csc_checkmark_label = lv_label_create(csc_icon_group);
    lv_label_set_text(csc_checkmark_label, "2");  // U+32 = checkmark icon
    lv_obj_set_style_text_color(csc_checkmark_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(csc_checkmark_label, &lv_font_icon_36, 0);
    lv_obj_center(csc_checkmark_label);
    lv_obj_add_flag(csc_checkmark_label, LV_OBJ_FLAG_HIDDEN);

    /* Label Group */
    lv_obj_t *label_group = lv_obj_create(card_csc);
    apply_transparent_container_style(label_group);
    lv_obj_set_size(label_group, 189, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(label_group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(label_group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(label_group, 2, 0);

    /* Sensor Name Label */
    lbl_csc_sensor_name = lv_label_create(label_group);
    lv_label_set_text(lbl_csc_sensor_name, csc_sensor_name);
    lv_obj_set_style_text_color(lbl_csc_sensor_name, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_csc_sensor_name, &lv_font_inter_bold_14, 0);
    lv_obj_set_style_translate_y(lbl_csc_sensor_name, 2, 0);

    /* Status Label */
    lbl_csc_status = lv_label_create(label_group);
    lv_label_set_text(lbl_csc_status, "Not Configurated");
    lv_obj_set_style_text_color(lbl_csc_status, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_csc_status, &lv_font_inter_bold_24, 0);
    lv_obj_set_style_translate_y(lbl_csc_status, 2, 0);

    update_csc_widget();
}

static void update_csc_widget(void)
{
    if (!card_csc) return;

    if (should_skip_main_screen_update()) {
        return;
    }

    /* O3: Use pre-defined styles instead of individual setter calls */
    /* Cache previous state to avoid redundant style changes */
    static csc_state_t prev_state = (csc_state_t)-1;
    
    if (csc_current_state != prev_state) {
        /* Remove old style, add new style based on state */
        lv_obj_remove_style(card_csc, &style_csc_active, 0);
        lv_obj_remove_style(card_csc, &style_csc_inactive, 0);
        
        switch (csc_current_state) {
            case CSC_NOT_CONFIGURATED:
                lv_obj_add_style(card_csc, &style_csc_inactive, 0);
                break;
            case CSC_STATE_INACTIVE:
            case CSC_STATE_ACTIVE:
                lv_obj_add_style(card_csc, &style_csc_active, 0);
                break;
        }
        prev_state = csc_current_state;
    }

    /* Update icon group */
    if (csc_current_state == CSC_STATE_ACTIVE) {
        /* Active: Show checkmark icon (U+32), hide error icon */
        lv_obj_set_style_bg_opa(csc_icon_group, LV_OPA_TRANSP, 0);
        lv_obj_add_flag(csc_icon_label, LV_OBJ_FLAG_HIDDEN);
        if (csc_checkmark_label) {
            lv_obj_clear_flag(csc_checkmark_label, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        /* Inactive/Error: Show white circle with error icon (U+33) */
        lv_obj_set_style_bg_color(csc_icon_group, lv_color_white(), 0);
        //lv_obj_set_style_bg_opa(csc_icon_group, LV_OPA_COVER, 0);
       // lv_obj_clear_flag(csc_icon_label, LV_OBJ_FLAG_HIDDEN);
        if (csc_checkmark_label) {
            lv_obj_add_flag(csc_checkmark_label, LV_OBJ_FLAG_HIDDEN);
        }
        // Error icon color is already set to COLOR_CSC in create function
    }

    /* Update labels */
    if (lbl_csc_sensor_name) {
        lv_label_set_text(lbl_csc_sensor_name, csc_sensor_name);
    }

    if (lbl_csc_status) {
        switch (csc_current_state) {
            case CSC_NOT_CONFIGURATED:
                lv_label_set_text(lbl_csc_status, "Not Configured");
                break;
            case CSC_STATE_INACTIVE:
                lv_label_set_text(lbl_csc_status, "Not Connected");
                break;
            case CSC_STATE_ACTIVE:
                lv_label_set_text(lbl_csc_status, get_csc_cadence_string());
                break;
        }
    }
    
    /* Update powersave screen if it exists */
    update_powersave_display();
}

/* --- Fan Widget --- */
static void create_fan_widget(lv_obj_t *parent)
{
    fan_container = lv_obj_create(parent);
    apply_transparent_container_style(fan_container);
    lv_obj_set_size(fan_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(fan_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(fan_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(fan_container, 12, 0);
    lv_obj_set_style_pad_column(fan_container, 12, 0);
    lv_obj_clear_flag(fan_container, LV_OBJ_FLAG_SCROLLABLE);

    /* Create 4 fan items (Figma: pill with circle + state text) */
    for (uint8_t i = 0; i < MAX_FANS; i++) {
        /* Pill-shaped card */
        fan_items[i].card = lv_obj_create(fan_container);
        lv_obj_set_size(fan_items[i].card, 133, 64);
        lv_obj_set_style_radius(fan_items[i].card, RADIUS_WIDGET, 0);  /* Pill: radius = half height */
        lv_obj_set_style_border_width(fan_items[i].card, 0, 0);
        lv_obj_set_style_pad_left(fan_items[i].card, 14, 0);
        lv_obj_set_style_pad_right(fan_items[i].card, 14, 0);
        lv_obj_set_style_pad_top(fan_items[i].card, 12, 0);
        lv_obj_set_style_pad_bottom(fan_items[i].card, 12, 0);
        lv_obj_clear_flag(fan_items[i].card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(fan_items[i].card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(fan_items[i].card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_gap(fan_items[i].card, 2, 0);

        /* Circle only (no icon glyphs) – green for Inactive, white for Active */
        fan_items[i].icon_group = lv_obj_create(fan_items[i].card);
        lv_obj_set_size(fan_items[i].icon_group, 36, 36);
        lv_obj_set_style_radius(fan_items[i].icon_group, RADIUS_ICON, 0);
        lv_obj_set_style_border_width(fan_items[i].icon_group, 0, 0);
        lv_obj_set_style_pad_all(fan_items[i].icon_group, 0, 0);
        lv_obj_clear_flag(fan_items[i].icon_group, LV_OBJ_FLAG_SCROLLABLE);

        /* State label: "Off", "On", or "Error" (set in update_fan_item); centered in remaining space */
        fan_items[i].lbl_name = lv_label_create(fan_items[i].card);
        lv_label_set_text(fan_items[i].lbl_name, "Off");
        lv_obj_set_style_text_color(fan_items[i].lbl_name, lv_color_hex(COLOR_TEXT_DARK), 0);
        lv_obj_set_style_text_font(fan_items[i].lbl_name, &lv_font_inter_bold_24, 0);
        lv_obj_set_style_text_align(fan_items[i].lbl_name, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(fan_items[i].lbl_name, 2, 0);
        lv_obj_set_flex_grow(fan_items[i].lbl_name, 1);  /* Take remaining width so text centers in pill */

        lv_obj_add_event_cb(fan_items[i].card, fan_toggle_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        fan_items[i].state = FAN_STATE_NOT_CONFIGURATED;
        fan_items[i].is_on = false;
        update_fan_item(i);
    }
}

static void update_fan_item(uint8_t index)
{
    if (index >= MAX_FANS || !fan_items[index].card) return;

    if (should_skip_main_screen_update()) {
        return;
    }

    /* O3: Skip update if state hasn't changed (reduces LVGL calls) */
    if (fan_ui_cache[index].initialized &&
        fan_ui_cache[index].state == fan_items[index].state &&
        fan_ui_cache[index].is_on == fan_items[index].is_on) {
        return;  /* No visual change needed */
    }
    
    /* Update cache */
    fan_ui_cache[index].state = fan_items[index].state;
    fan_ui_cache[index].is_on = fan_items[index].is_on;
    fan_ui_cache[index].initialized = true;

    switch (fan_items[index].state) {
        case FAN_STATE_NOT_CONFIGURATED: {
            /* Figma: light gray pill, no circle, no text */
            lv_obj_set_style_bg_color(fan_items[index].card, lv_color_hex(COLOR_FAN_UNCONFIGURED), 0);
            lv_obj_set_style_bg_opa(fan_items[index].card, LV_OPA_COVER, 0);
            lv_obj_add_flag(fan_items[index].icon_group, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(fan_items[index].lbl_name, LV_OBJ_FLAG_HIDDEN);
            break;
        }
        case FAN_STATE_ACTIVE:
            /* ACTIVE: label left, circle right */
            lv_obj_move_foreground(fan_items[index].icon_group);
            if (fan_items[index].is_on) {
                lv_obj_clear_flag(fan_items[index].icon_group, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(fan_items[index].lbl_name, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_bg_color(fan_items[index].card, lv_color_hex(COLOR_FAN_ACTIVE_ON), 0);
                lv_obj_set_style_bg_opa(fan_items[index].card, LV_OPA_COVER, 0);
                lv_obj_set_style_bg_color(fan_items[index].icon_group, lv_color_white(), 0);
                lv_obj_set_style_bg_opa(fan_items[index].icon_group, LV_OPA_COVER, 0);
                lv_label_set_text(fan_items[index].lbl_name, "On");
                lv_obj_set_style_text_color(fan_items[index].lbl_name, lv_color_white(), 0);
                break;
            }
            /* ACTIVE with is_on=false: same look as INACTIVE */
        case FAN_STATE_INACTIVE:
            /* INACTIVE: label right, circle left */
            lv_obj_move_foreground(fan_items[index].lbl_name);
            lv_obj_set_style_bg_color(fan_items[index].card, lv_color_hex(COLOR_WHITE), 0);
            lv_obj_set_style_bg_opa(fan_items[index].card, LV_OPA_COVER, 0);
            lv_obj_clear_flag(fan_items[index].icon_group, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(fan_items[index].lbl_name, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(fan_items[index].icon_group, lv_color_hex(COLOR_SUCCESS_ICON), 0);
            lv_obj_set_style_bg_opa(fan_items[index].icon_group, LV_OPA_COVER, 0);
            lv_label_set_text(fan_items[index].lbl_name, "Off");
            lv_obj_set_style_text_color(fan_items[index].lbl_name, lv_color_hex(COLOR_TEXT_DARK), 0);
            break;
        case FAN_STATE_ERROR:
            lv_obj_add_flag(fan_items[index].icon_group, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(fan_items[index].lbl_name, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(fan_items[index].lbl_name, "Error");
            lv_obj_set_style_bg_color(fan_items[index].card, lv_color_hex(COLOR_ACCENT), 0);
            lv_obj_set_style_bg_opa(fan_items[index].card, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(fan_items[index].lbl_name, lv_color_white(), 0);
            break;
    }
    
    /* Update powersave screen if it exists */
    update_powersave_display();
}

/* --- Settings Modal --- */
static void create_settings_modal(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t w = lv_disp_get_hor_res(disp);
    lv_coord_t h = lv_disp_get_ver_res(disp);

    settings_overlay = lv_obj_create(scr_main);
    lv_obj_set_size(settings_overlay, w + 4, h + 6);  // +4 to cover all edges
    lv_obj_set_style_bg_color(settings_overlay, lv_color_hex(COLOR_MODAL_BG), 0);
    //lv_obj_set_style_bg_opa(settings_overlay, LV_OPA_90, 0);  // 90% opacity (rgba(0,0,0,0.9))
    lv_obj_clear_flag(settings_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(settings_overlay, -2, -6);  // Position absolutely: -2px left, -6px top to cover edges

    /* Start: hidden and positioned off-screen */
    lv_obj_set_y(settings_overlay, h + 6);  // Position off-screen, accounting for -6 offset
    lv_obj_add_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);

    /* QR Code Card */
    settings_card = lv_obj_create(settings_overlay);
    lv_coord_t card_w = w - 2;
    lv_coord_t card_h = h - 130;
    lv_obj_set_size(settings_card, card_w, card_h);
    lv_obj_set_style_radius(settings_card, RADIUS_CARD, 0);
    lv_obj_set_style_bg_color(settings_card, lv_color_hex(COLOR_MODAL_CARD), 0);
    lv_obj_set_style_bg_opa(settings_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(settings_card, 0, 0);
    lv_obj_set_style_pad_top(settings_card, 16, 0);
    lv_obj_set_style_pad_bottom(settings_card, 0, 0);
    lv_obj_set_style_pad_hor(settings_card, 12, 0);
    lv_obj_clear_flag(settings_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(settings_card, LV_ALIGN_TOP_MID, 0, 0);  // Moved 4px higher (was 12)

    /* QR Code Widget */
    lv_coord_t qr_size = card_w - 20;
    if (qr_size > 220) qr_size = 220;
    qr_code_widget = lv_qrcode_create(settings_card, qr_size, lv_color_white(), lv_color_hex(COLOR_MODAL_CARD));
    lv_qrcode_update(qr_code_widget, qr_code_url, strlen(qr_code_url));
    lv_obj_align(qr_code_widget, LV_ALIGN_TOP_MID, 0, 10);

    /* Text Label */
    lv_obj_t *lbl_scan = lv_label_create(settings_card);
    lv_label_set_text(lbl_scan, "Scan to Setup\nyour Device");
    lv_obj_set_style_text_align(lbl_scan, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_scan, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_scan, &lv_font_inter_bold_24, 0);
    lv_obj_set_style_pad_top(lbl_scan, 8, 0);
    lv_obj_set_style_pad_bottom(lbl_scan, 8, 0);
    lv_obj_align(lbl_scan, LV_ALIGN_BOTTOM_MID, 0, -8);

    /* Close Button */
    lv_obj_t *btn_close = lv_btn_create(settings_overlay);
    lv_coord_t btn_w = w - 2;
    if (btn_w < 140) btn_w = 140;
    lv_obj_set_size(btn_close, btn_w, 72);
    lv_obj_align(btn_close, LV_ALIGN_BOTTOM_MID, 0, 0);  // Moved 8px lower (was -12)
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_radius(btn_close, RADIUS_BUTTON, 0);
    lv_obj_add_event_cb(btn_close, close_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, "Close");
    lv_obj_set_style_text_font(lbl_close, &lv_font_inter_bold_24, 0);
    lv_obj_center(lbl_close);
    lv_obj_set_style_translate_y(lbl_close, 2, 0);
}

/* --- Boot Screen (Warming Up) --- */
#define BOOT_AP_VERTICAL_GAP 28
static void create_boot_screen(void)
{
    scr_boot = lv_obj_create(NULL);
    lv_obj_clear_flag(scr_boot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr_boot, lv_color_hex(COLOR_BG_APP), 0);
    lv_obj_set_style_bg_opa(scr_boot, LV_OPA_COVER, 0);

    lv_obj_t *cont = lv_obj_create(scr_boot);
    apply_transparent_container_style(cont);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(cont, BOOT_AP_VERTICAL_GAP, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    /* Heart icon (36px * 2 = 72px) */
    lv_obj_t *lbl_icon = lv_label_create(cont);
    lv_label_set_text(lbl_icon, "1");
    lv_obj_set_style_text_font(lbl_icon, &lv_font_icon_36, 0);
    lv_obj_set_style_text_color(lbl_icon, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_transform_zoom(lbl_icon, 256, 0);

    lv_obj_t *lbl_title = lv_label_create(cont);
    lv_label_set_text(lbl_title, "Warming Up");
    lv_obj_set_style_text_font(lbl_title, &lv_font_inter_bold_24, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(COLOR_WHITE), 0);

    lv_obj_t *lbl_sub1 = lv_label_create(cont);
    lv_label_set_text(lbl_sub1, "Starting Homewind");
    lv_obj_set_style_text_font(lbl_sub1, &lv_font_inter_bold_14, 0);
    lv_obj_set_style_text_color(lbl_sub1, lv_color_hex(COLOR_WHITE), 0);

    lv_obj_t *lbl_sub2 = lv_label_create(cont);
    lv_label_set_text(lbl_sub2, "Please Wait");
    lv_obj_set_style_text_font(lbl_sub2, &lv_font_inter_bold_14, 0);
    lv_obj_set_style_text_color(lbl_sub2, lv_color_hex(COLOR_WHITE), 0);
}

/* --- AP Screen (Wifisetup) --- */
static void create_ap_screen(void)
{
    scr_apmode = lv_obj_create(NULL);
    lv_obj_clear_flag(scr_apmode, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr_apmode, lv_color_hex(COLOR_BG_APP), 0);
    lv_obj_set_style_bg_opa(scr_apmode, LV_OPA_COVER, 0);

    lv_obj_t *cont = lv_obj_create(scr_apmode);
    apply_transparent_container_style(cont);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(cont, BOOT_AP_VERTICAL_GAP, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    /* Heart icon (36px * 2 = 72px) */
    lv_obj_t *lbl_icon = lv_label_create(cont);
    lv_label_set_text(lbl_icon, "1");
    lv_obj_set_style_text_font(lbl_icon, &lv_font_icon_36, 0);
    lv_obj_set_style_text_color(lbl_icon, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_text_align(lbl_icon, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_transform_zoom(lbl_icon, 256, 0);

    lv_obj_t *lbl_title = lv_label_create(cont);
    lv_label_set_text(lbl_title, "Wifisetup Mode");
    lv_obj_set_style_text_font(lbl_title, &lv_font_inter_bold_24, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *lbl_body = lv_label_create(cont);
    lv_label_set_text(lbl_body, "Go to Wifi and Connect to the Homewind Device to Setup Wifi");
    lv_obj_set_style_text_font(lbl_body, &lv_font_inter_bold_14, 0);
    lv_obj_set_style_text_color(lbl_body, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_align(lbl_body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_body, 240);
    lv_label_set_long_mode(lbl_body, LV_LABEL_LONG_WRAP);
}

/* --- Main Screen Creation --- */
static void create_main_screen(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t w = lv_disp_get_hor_res(disp);
    lv_coord_t h = lv_disp_get_ver_res(disp);

    /* O3: Initialize pre-defined styles (once) */
    init_styles();
    
    scr_main = lv_obj_create(NULL);
    lv_obj_clear_flag(scr_main, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr_main, lv_color_hex(COLOR_BG_APP), 0);
    lv_obj_set_style_bg_opa(scr_main, LV_OPA_COVER, 0);

    /* Stack Container */
    lv_obj_t *stack = lv_obj_create(scr_main);
    apply_transparent_container_style(stack);
    lv_obj_set_style_pad_row(stack, 12, 0);
    lv_obj_set_style_pad_column(stack, 0, 0);
    lv_coord_t stack_h = h - 72 - 10;  /* keep 10px gap to Settings button (4px more space for content) */
    if (stack_h < 0) stack_h = 0;
    lv_obj_set_size(stack, w - 2, stack_h);
    lv_obj_align(stack, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_flex_flow(stack, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(stack, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(stack, LV_OBJ_FLAG_SCROLLABLE);

    /* Create Widgets */
    create_hr_widget(stack);
    create_csc_widget(stack);
    create_fan_widget(stack);

    /* Settings Button */
    lv_obj_t *btn_settings = lv_btn_create(scr_main);
    lv_obj_set_size(btn_settings, w - 2, 72);
    lv_obj_align(btn_settings, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_settings, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_radius(btn_settings, RADIUS_BUTTON, 0);
    lv_obj_add_event_cb(btn_settings, settings_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_settings = lv_label_create(btn_settings);
    lv_label_set_text(lbl_settings, "Settings");
    lv_obj_set_style_text_font(lbl_settings, &lv_font_inter_bold_24, 0);
    lv_obj_center(lbl_settings);
    lv_obj_set_style_translate_y(lbl_settings, 2, 0);

    /* Create Settings Modal */
    create_settings_modal();
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
    /* Invalidate all widget caches to force full re-render */
    hr_widget_cache_initialized = false;
    
    /* CSC widget uses a function-local static prev_state; we trigger a full
       update by temporarily marking it as uninitialized via a state change.
       The simplest approach: just call update_csc_widget() — the label/icon
       setters are idempotent, and the cost is negligible on wake. */
    
    for (uint8_t i = 0; i < MAX_FANS; i++) {
        fan_ui_cache[i].initialized = false;
    }
    
    /* Re-render all widgets with current in-memory values */
    update_hr_widget();
    update_csc_widget();
    for (uint8_t i = 0; i < MAX_FANS; i++) {
        if (fan_items[i].card) {
            update_fan_item(i);
        }
    }
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
 * CSC Widget API Implementation (with Change-Guards - O1 optimization)
 * ============================================================================ */

void homewind_set_csc(csc_state_t state, const char* sensor_name, uint16_t cadence)
{
    bool changed = false;
    if (state != csc_current_state) { csc_current_state = state; changed = true; }
    if (sensor_name && strcmp(sensor_name, csc_sensor_name) != 0) {
        strncpy(csc_sensor_name, sensor_name, sizeof(csc_sensor_name) - 1);
        csc_sensor_name[sizeof(csc_sensor_name) - 1] = '\0';
        changed = true;
    }
    if (cadence != csc_cadence_value) { csc_cadence_value = cadence; changed = true; }
    if (changed) update_csc_widget();
}

void homewind_set_csc_state(csc_state_t state)
{
    if (state != csc_current_state) {
        csc_current_state = state;
        update_csc_widget();
    }
}

void homewind_set_csc_cadence(uint16_t cadence)
{
    if (cadence != csc_cadence_value) {
        csc_cadence_value = cadence;
        update_csc_widget();
    }
}

/* ============================================================================
 * Fan Widget API Implementation (with Change-Guards - O1 optimization)
 * ============================================================================ */

void homewind_set_fan(uint8_t fan_index, fan_state_t state, bool is_on)
{
    if (fan_index >= MAX_FANS) return;
    if (fan_items[fan_index].state == state && fan_items[fan_index].is_on == is_on) return;
    fan_items[fan_index].state = state;
    fan_items[fan_index].is_on = is_on;
    update_fan_item(fan_index);
    update_powersave_display();
}

void homewind_set_fan_state(uint8_t fan_index, fan_state_t state)
{
    if (fan_index >= MAX_FANS) return;
    if (fan_items[fan_index].state == state) return;
    fan_items[fan_index].state = state;
    if (state == FAN_STATE_NOT_CONFIGURATED || state == FAN_STATE_INACTIVE) {
        fan_items[fan_index].is_on = false;
    }
    update_fan_item(fan_index);
    update_powersave_display();
}

void homewind_set_fan_toggle(uint8_t fan_index, bool is_on)
{
    if (fan_index >= MAX_FANS) return;
    if (fan_items[fan_index].is_on == is_on) return;
    fan_items[fan_index].is_on = is_on;
    update_fan_item(fan_index);
    animate_fan_toggle(fan_index);
    update_powersave_display();
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
        lv_obj_add_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);
    }
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
    
    /* 3. Create UI screens (main screen with widgets, settings modal) */
    homewind_create_screens();
    
    /* 4. Initialize power save system (timers, overlays, breathing animation) */
    extern void powersave_init(void);
    powersave_init();
    
    /* 5. Start IMU rotation task if QMI8658 hardware is available */
    #ifdef QMI8658_SLAVE_ADDR_L
    extern bool lcd_start_imu_rotation_task(void);
    lcd_start_imu_rotation_task();
    #endif
    
    homewind_initialized = true;
}
