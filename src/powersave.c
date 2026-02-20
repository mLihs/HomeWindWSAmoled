// powersave.c
#include "powersave.h"
#include "homewind_ui.h"
#include "lcd_bsp.h"
#include "lv_conf_psram_auto.h"
#include "lvgl.h"
#include "ui_colors.h"
#include "ui_style_helpers.h"
#include <string.h>
#include <stdio.h>

/* --- Font Includes --- */
LV_FONT_DECLARE(lv_font_icon_56);
LV_FONT_DECLARE(lv_font_inter_bold_24);

/* --- Constants --- */
#define DIM_TIMEOUT_MS       8000   // 4 s: ACTIVE -> Light Sleep (DIMMED)
#define SOFT_TIMEOUT_MS      20000   // 4 s: Light Sleep -> Sleep (SOFT_POWERSAVE)
#define BRIGHTNESS_FULL     255     // Full brightness (100%)
#define BRIGHTNESS_DIMMED   120     // Dimmed brightness (~47%)
#define BRIGHTNESS_SOFT_MIN 80      // Soft powersave minimum brightness (~4%)
#define BRIGHTNESS_SOFT_MAX 120      // Soft powersave maximum brightness (~20%)
#define BREATHING_ANIM_TIME 6000    // 6 second cycle (more natural breathing rate)

/* --- Breathing Curve Configuration --- */
static breathing_ease_type_t inhale_ease = BREATHING_EASE_QUADRATIC_IN;
static breathing_ease_type_t exhale_ease = BREATHING_EASE_QUADRATIC_OUT;

/* --- Forward Declarations --- */
static lv_obj_t *get_main_screen(void);
static void inactivity_timer_cb(lv_timer_t *timer);
static void overlay_touch_event_cb(lv_event_t *e);
static int32_t breathing_path_cb(const lv_anim_t *a);
static void start_breathing_animation(void);
static void stop_breathing_animation(void);
static void create_powersave_screen(void);

/* --- State Variables --- */
static ui_power_state_t current_power_state = UI_POWER_STATE_ACTIVE;
static uint32_t last_activity_time = 0;
static lv_timer_t *inactivity_timer = NULL;
static bool powersave_locked = true;  /* Locked at init (Boot screen active) */

/* --- Screen Objects --- */
static lv_obj_t *scr_powersave = NULL;
static lv_obj_t *powersave_container = NULL;
static lv_obj_t *powersave_hr_icon = NULL;
static lv_obj_t *powersave_hr_label = NULL;
static lv_obj_t *powersave_csc_icon = NULL;
static lv_obj_t *powersave_csc_label = NULL;
static lv_obj_t *powersave_fan_icon = NULL;
static lv_obj_t *powersave_fan_label = NULL;
static lv_anim_t brightness_anim;

/* --- Cached formatted strings (avoid repeated snprintf) --- */
static char powersave_hr_str[8] = "--";
static uint16_t powersave_hr_cached = 0xFFFF;
static char powersave_csc_str[8] = "--";
static uint16_t powersave_csc_cached = 0xFFFF;
static char powersave_fan_str[8] = "--";
static uint8_t powersave_fan_active_cached = 0xFF;
static uint8_t powersave_fan_total_cached = 0xFF;

/* --- Get Main Screen (extern from homewind_ui.c) --- */
extern lv_obj_t *scr_main;

static lv_obj_t *get_main_screen(void)
{
    return scr_main;
}

static lv_obj_t *create_powersave_slot(lv_obj_t *parent, const char *icon_text,
                                       lv_color_t icon_color, lv_obj_t **icon_out,
                                       lv_obj_t **label_out, const char *label_text)
{
    lv_obj_t *slot = lv_obj_create(parent);
    apply_transparent_container_style(slot);
    lv_obj_set_size(slot, 60, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(slot, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(slot, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(slot, 0, 0);

    *icon_out = lv_label_create(slot);
    lv_label_set_text(*icon_out, icon_text);
    lv_obj_set_style_text_color(*icon_out, icon_color, 0);
    lv_obj_set_style_text_font(*icon_out, &lv_font_icon_56, 0);
    lv_obj_set_size(*icon_out, 56, 56);

    *label_out = lv_label_create(slot);
    lv_label_set_text(*label_out, label_text);
    lv_obj_set_style_text_color(*label_out, lv_color_white(), 0);
    lv_obj_set_style_text_font(*label_out, &lv_font_inter_bold_24, 0);
    lv_obj_set_style_text_align(*label_out, LV_TEXT_ALIGN_CENTER, 0);

    return slot;
}

/* --- Apply Easing Function --- */
/* Applies the specified easing type to a normalized value (0-1024) */
static int32_t apply_easing(uint32_t t, breathing_ease_type_t ease_type)
{
    /* t is normalized 0-1024 */
    int32_t result;
    
    switch (ease_type) {
        case BREATHING_EASE_LINEAR:
            result = t;
            break;
            
        case BREATHING_EASE_QUADRATIC_IN:
            result = (t * t) / 1024;
            break;
            
        case BREATHING_EASE_QUADRATIC_OUT:
            {
                uint32_t t2 = 1024 - t;
                result = 1024 - (t2 * t2) / 1024;
            }
            break;
            
        case BREATHING_EASE_QUADRATIC_IN_OUT:
            if (t < 512) {
                result = (t * t * 2) / 1024;
            } else {
                uint32_t t2 = 1024 - t;
                result = 1024 - (t2 * t2 * 2) / 1024;
            }
            break;
            
        case BREATHING_EASE_CUBIC_IN:
            result = (t * t * t) / (1024 * 1024);
            break;
            
        case BREATHING_EASE_CUBIC_OUT:
            {
                uint32_t t2 = 1024 - t;
                result = 1024 - (t2 * t2 * t2) / (1024 * 1024);
            }
            break;
            
        case BREATHING_EASE_CUBIC_IN_OUT:
            if (t < 512) {
                result = (t * t * t * 4) / (1024 * 1024);
            } else {
                uint32_t t2 = 1024 - t;
                result = 1024 - (t2 * t2 * t2 * 4) / (1024 * 1024);
            }
            break;
            
        case BREATHING_EASE_SINE_IN:
            /* Approximate sine ease-in using quadratic */
            {
                uint32_t t2 = 1024 - t;
                uint32_t quad = (t2 * t2) / 1024;
                result = 1024 - quad;
            }
            break;
            
        case BREATHING_EASE_SINE_OUT:
            /* Approximate sine ease-out */
            result = (t * t) / 1024;
            break;
            
        case BREATHING_EASE_SINE_IN_OUT:
            if (t < 512) {
                result = (t * t) / 512;
            } else {
                uint32_t t2 = 1024 - t;
                result = 1024 - (t2 * t2) / 512;
            }
            break;
            
        default:
            result = t;
            break;
    }
    
    if (result < 0) result = 0;
    if (result > 1024) result = 1024;
    return result;
}

/* --- Custom Breathing Path with Separate Inhale/Exhale Curves --- */
/* Returns the actual brightness value (BRIGHTNESS_SOFT_MIN to BRIGHTNESS_SOFT_MAX) */
/* We calculate the brightness directly instead of relying on LVGL interpolation */
static int32_t breathing_path_cb(const lv_anim_t *a)
{
    /* Realistic breathing: smooth inhale (0-40%), gentle hold (40-50%), slow exhale (50-100%) */
    uint32_t t = a->act_time;
    uint32_t d = a->time;
    
    /* Normalize time to 0-1024 range with proper handling */
    if (d == 0) return BRIGHTNESS_SOFT_MAX;
    uint32_t t_norm = (t * 1024) / d;
    if (t_norm > 1024) t_norm = 1024;
    
    int32_t path_value;
    
    if (t_norm < 410) {
        /* Inhale phase (0-40%): use configured inhale easing curve */
        uint32_t inhale_t = (t_norm * 1024) / 410;  // 0-1024
        if (inhale_t > 1024) inhale_t = 1024;
        path_value = apply_easing(inhale_t, inhale_ease);
    } else if (t_norm < 512) {
        /* Hold at peak (40-50%): stay at maximum */
        path_value = 1024;
    } else {
        /* Exhale phase (50-100%): use configured exhale easing curve */
        uint32_t exhale_t = ((t_norm - 512) * 1024) / 512;  // 0-1024
        if (exhale_t > 1024) exhale_t = 1024;
        /* Apply exhale easing (inverted, since we go from 1024 to 0) */
        uint32_t ease_result = apply_easing(exhale_t, exhale_ease);
        path_value = 1024 - ease_result;  // Invert: 1024 to 0
    }
    
    /* Clamp path_value to valid range */
    if (path_value < 0) path_value = 0;
    if (path_value > 1024) path_value = 1024;
    
    /* Calculate actual brightness value from path progress */
    /* path_value is 0-1024, map to BRIGHTNESS_SOFT_MIN to BRIGHTNESS_SOFT_MAX */
    int32_t brightness = BRIGHTNESS_SOFT_MIN + 
                         ((BRIGHTNESS_SOFT_MAX - BRIGHTNESS_SOFT_MIN) * path_value) / 1024;
    
    return brightness;
}

/* --- Brightness Animation Callback --- */
/* Optimized: Only write to backlight when value actually changes */
static void brightness_anim_cb(void *var, int32_t value)
{
    static uint8_t last_brightness = 255;
    
    /* Clamp value to valid brightness range and cast to uint8_t */
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    uint8_t brightness = (uint8_t)value;
    
    /* Skip if brightness hasn't changed (reduces I2C/SPI overhead) */
    if (brightness == last_brightness) return;
    
    last_brightness = brightness;
    set_amoled_backlight(brightness);
}

static void start_breathing_animation(void)
{
    if (!powersave_container) return;
    
    /* Stop any existing animation first to prevent conflicts */
    lv_anim_del(NULL, (lv_anim_exec_xcb_t)brightness_anim_cb);
    
    /* Start realistic breathing animation using display brightness only */
    /* This creates a natural breathing effect: inhale fast, hold, exhale slow */
    lv_anim_init(&brightness_anim);
    lv_anim_set_var(&brightness_anim, NULL);  // No object needed, we control brightness directly
    /* Set values to full range, but path callback will limit to SOFT_MIN/MAX */
    lv_anim_set_values(&brightness_anim, 0, 255);
    lv_anim_set_time(&brightness_anim, BREATHING_ANIM_TIME);
    lv_anim_set_repeat_count(&brightness_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&brightness_anim, breathing_path_cb);  // Use custom breathing path
    lv_anim_set_exec_cb(&brightness_anim, (lv_anim_exec_xcb_t)brightness_anim_cb);
    
    /* Ensure we start at MIN brightness immediately */
    brightness_anim_cb(NULL, BRIGHTNESS_SOFT_MIN);
    
    lv_anim_start(&brightness_anim);
}

static void stop_breathing_animation(void)
{
    /* Stop brightness breathing animation */
    lv_anim_del(NULL, (lv_anim_exec_xcb_t)brightness_anim_cb);
}

/* --- Powersave Screen Creation --- */
static void create_powersave_screen(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t w = lv_disp_get_hor_res(disp);
    lv_coord_t h = lv_disp_get_ver_res(disp);

    /* Create powersave screen with pure black background */
    scr_powersave = lv_obj_create(NULL);
    lv_obj_clear_flag(scr_powersave, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr_powersave, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr_powersave, LV_OPA_COVER, 0);

    /* Animated container for breathing effect */
    powersave_container = lv_obj_create(scr_powersave);
    apply_transparent_container_style(powersave_container);
    lv_obj_set_size(powersave_container, w, h);
    lv_obj_align(powersave_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(powersave_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(powersave_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(powersave_container, 28, 0);
    lv_obj_clear_flag(powersave_container, LV_OBJ_FLAG_SCROLLABLE);
    
    /* Make container clickable to catch touches */
    lv_obj_add_flag(powersave_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(powersave_container, overlay_touch_event_cb, LV_EVENT_ALL, NULL);

    /* HR Sensor Slot */
    create_powersave_slot(powersave_container, "1", lv_color_hex(COLOR_ACCENT),
                          &powersave_hr_icon, &powersave_hr_label, "--");

    /* CSC Sensor Slot */
    create_powersave_slot(powersave_container, "5", lv_color_hex(COLOR_CSC),
                          &powersave_csc_icon, &powersave_csc_label, "--");

    /* Fan Sensor Slot (initial "--" until first refresh) */
    create_powersave_slot(powersave_container, "6", lv_color_hex(COLOR_SUCCESS_ICON),
                          &powersave_fan_icon, &powersave_fan_label, "--");
}

/* --- Touch Event Handler for Overlays --- */
static void overlay_touch_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_CLICKED) {
        on_user_activity();
    }
}

/* --- Main screen touch: reset inactivity timer; in DIMMED, restore brightness (touch hits widgets directly) --- */
static void main_screen_touch_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    last_activity_time = lv_tick_get();
    if (current_power_state == UI_POWER_STATE_DIMMED) {
        set_amoled_backlight(BRIGHTNESS_FULL);
        current_power_state = UI_POWER_STATE_ACTIVE;
    }
}

/* --- Guard to prevent multiple deferred transitions --- */
static bool powersave_transition_pending = false;

/* --- Deferred soft powersave (one-shot timer to avoid re-entrancy in timer callback) --- */
static void deferred_soft_powersave_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    powersave_transition_pending = false;
    if (!powersave_locked) {
        set_state_soft_powersave();
    }
}

/* --- Powersave Lock/Unlock (Boot/AP screen) --- */
void powersave_lock(void)
{
    powersave_locked = true;
    /* State-Cleanup: end any active powersave state so AP screen is readable */
    if (current_power_state == UI_POWER_STATE_SOFT_POWERSAVE) {
        stop_breathing_animation();
    }
    if (current_power_state != UI_POWER_STATE_ACTIVE) {
        set_amoled_backlight(BRIGHTNESS_FULL);
        current_power_state = UI_POWER_STATE_ACTIVE;
    }
}

void powersave_unlock(void)
{
    powersave_locked = false;
}

/* --- Inactivity Timer Callback --- */
static void inactivity_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    /* Touch-Detection first (always active, also when locked) */
    lv_indev_t *act_indev = lv_indev_get_act();
    if (act_indev && lv_indev_get_type(act_indev) == LV_INDEV_TYPE_POINTER) {
        last_activity_time = lv_tick_get();
        if (current_power_state == UI_POWER_STATE_DIMMED) {
            set_amoled_backlight(BRIGHTNESS_FULL);
            current_power_state = UI_POWER_STATE_ACTIVE;
        }
        return;
    }

    /* Powersave lock (Boot/AP screen): reset timer, no transitions */
    if (powersave_locked) {
        last_activity_time = lv_tick_get();
        return;
    }

    uint32_t now = lv_tick_get();
    uint32_t diff = now - last_activity_time;

    /* Only transition if we're not already in the target state */
    if (current_power_state == UI_POWER_STATE_ACTIVE && diff > DIM_TIMEOUT_MS) {
        set_state_dimmed();
    } else if (current_power_state == UI_POWER_STATE_DIMMED && diff > SOFT_TIMEOUT_MS && !powersave_transition_pending) {
        /* Defer to next LVGL tick to avoid doing lv_scr_load + animation from inside timer callback
         * Guard prevents multiple timers if inactivity_timer fires again before transition completes */
        powersave_transition_pending = true;
        lv_timer_t *t = lv_timer_create(deferred_soft_powersave_cb, 1, NULL);
        lv_timer_set_repeat_count(t, 1);
    }
    /* Once in SOFT_POWERSAVE, stay there until user activity */
}

/* --- State Transition Functions --- */
void set_state_active(void)
{
    /* This is now handled in on_user_activity() */
    current_power_state = UI_POWER_STATE_ACTIVE;
    last_activity_time = lv_tick_get();
}

void set_state_dimmed(void)
{
    lv_obj_t *scr_main = get_main_screen();
    if (!scr_main) return;

    if (current_power_state == UI_POWER_STATE_SOFT_POWERSAVE) {
        lv_scr_load(scr_main);
        stop_breathing_animation();
    }

    set_amoled_backlight(BRIGHTNESS_DIMMED);
    current_power_state = UI_POWER_STATE_DIMMED;
}

void set_state_soft_powersave(void)
{
    lv_obj_t *scr_main = get_main_screen();
    
    if (!scr_powersave) {
        create_powersave_screen();
    }

    /* Set state first so any code during/after load sees SOFT_POWERSAVE */
    current_power_state = UI_POWER_STATE_SOFT_POWERSAVE;

    /* Set initial soft powersave brightness (will be animated) */
    set_amoled_backlight(BRIGHTNESS_SOFT_MIN);

    lv_scr_load(scr_powersave);
    /* Refresh powersave labels (HR/CSC "--" when not configured) so display is correct */
    homewind_refresh_powersave_display();
    start_breathing_animation();
}

ui_power_state_t get_ui_power_state(void)
{
    return current_power_state;
}

/* --- User Activity Handler --- */
void on_user_activity(void)
{
    last_activity_time = lv_tick_get();

    if (current_power_state != UI_POWER_STATE_ACTIVE) {
        bool was_soft_powersave = (current_power_state == UI_POWER_STATE_SOFT_POWERSAVE);

        if (was_soft_powersave) {
            lv_obj_t *scr_main = get_main_screen();
            if (scr_main) {
                lv_scr_load(scr_main);
                stop_breathing_animation();
                homewind_hide_settings_overlay();
            }
        }

        set_amoled_backlight(BRIGHTNESS_FULL);
        current_power_state = UI_POWER_STATE_ACTIVE;

        /* FIX: Refresh main screen widgets after powersave wake.
         * During SOFT_POWERSAVE, main screen updates were skipped by
         * should_skip_main_screen_update(). State must be ACTIVE before
         * this call so the skip-check passes. */
        if (was_soft_powersave) {
            homewind_refresh_main_display();
        }
    }
}

/* --- Cached values for dirty-checking (avoid redundant UI updates) --- */
static struct {
    hr_state_t hr_state;
    csc_state_t csc_state;
    uint16_t hr_value;
    uint16_t csc_value;
    uint8_t fan_active_count;
    uint8_t fan_total_count;
    bool initialized;
} powersave_cache = { 0 };

/* --- Update Powersave Icons (optimized with dirty-checking) --- */
void update_powersave_icons(hr_state_t hr_state, csc_state_t csc_state,
                            uint16_t hr_value, uint16_t csc_value, uint8_t fan_active_count, uint8_t fan_total_count)
{
    if (!scr_powersave) return;
    
    /* Skip update if powersave screen is not active (save CPU) */
    if (current_power_state != UI_POWER_STATE_SOFT_POWERSAVE && powersave_cache.initialized) {
        /* Just cache the values for when powersave becomes active */
        powersave_cache.hr_state = hr_state;
        powersave_cache.csc_state = csc_state;
        powersave_cache.hr_value = hr_value;
        powersave_cache.csc_value = csc_value;
        powersave_cache.fan_active_count = fan_active_count;
        powersave_cache.fan_total_count = fan_total_count;
        return;
    }

    /* Update HR Sensor (only if changed) */
    if (powersave_hr_icon && powersave_hr_label) {
        bool hr_changed = !powersave_cache.initialized || 
                          hr_state != powersave_cache.hr_state || 
                          hr_value != powersave_cache.hr_value;
        if (hr_changed) {
            if (hr_state == HR_STATE_ACTIVE) {
                lv_label_set_text(powersave_hr_icon, "1");  // Heart icon
                lv_obj_set_style_text_color(powersave_hr_icon, lv_color_hex(COLOR_ACCENT), 0);
                if (hr_value != powersave_hr_cached) {
                    snprintf(powersave_hr_str, sizeof(powersave_hr_str), "%d", hr_value);
                    powersave_hr_cached = hr_value;
                }
                lv_label_set_text(powersave_hr_label, powersave_hr_str);
            } else {
                lv_label_set_text(powersave_hr_icon, "4");  // Disconnected icon
                lv_obj_set_style_text_color(powersave_hr_icon, lv_color_hex(COLOR_DISCONNECTED), 0);
                lv_label_set_text(powersave_hr_label, "--");
            }
        }
    }

    /* Update CSC Sensor (only if changed) */
    if (powersave_csc_icon && powersave_csc_label) {
        bool csc_changed = !powersave_cache.initialized ||
                           csc_state != powersave_cache.csc_state ||
                           csc_value != powersave_cache.csc_value;
        if (csc_changed) {
            if (csc_state == CSC_STATE_ACTIVE) {
                lv_label_set_text(powersave_csc_icon, "5");  // CSC icon
                lv_obj_set_style_text_color(powersave_csc_icon, lv_color_hex(COLOR_CSC), 0);
                if (csc_value != powersave_csc_cached) {
                    snprintf(powersave_csc_str, sizeof(powersave_csc_str), "%d", csc_value);
                    powersave_csc_cached = csc_value;
                }
                lv_label_set_text(powersave_csc_label, powersave_csc_str);
            } else {
                lv_label_set_text(powersave_csc_icon, "5");  // CSC icon (same, but gray)
                lv_obj_set_style_text_color(powersave_csc_icon, lv_color_hex(COLOR_DISCONNECTED), 0);
                lv_label_set_text(powersave_csc_label, "--");
            }
        }
    }

    /* Update Fan Sensor (when in SOFT_POWERSAVE we always apply current counts so display is correct on enter)
     * A1: no fans configured -> "--"
     * A2: active/configured -> "1/2" */
    if (powersave_fan_icon && powersave_fan_label) {
        if (fan_total_count == 0) {
            lv_label_set_text(powersave_fan_icon, "6");
            lv_obj_set_style_text_color(powersave_fan_icon, lv_color_hex(COLOR_DISCONNECTED), 0);
            lv_label_set_text(powersave_fan_label, "--");
            powersave_fan_active_cached = 0;
            powersave_fan_total_cached = 0;
        } else {
            /* Always set icon color from current state so first enter into powersave shows correct active/gray */
            if (fan_active_count > 0) {
                lv_label_set_text(powersave_fan_icon, "6");
                lv_obj_set_style_text_color(powersave_fan_icon, lv_color_hex(COLOR_SUCCESS_ICON), 0);
            } else {
                lv_label_set_text(powersave_fan_icon, "6");
                lv_obj_set_style_text_color(powersave_fan_icon, lv_color_hex(COLOR_DISCONNECTED), 0);
            }
            snprintf(powersave_fan_str, sizeof(powersave_fan_str), "%d/%d", fan_active_count, fan_total_count);
            powersave_fan_active_cached = fan_active_count;
            powersave_fan_total_cached = fan_total_count;
            lv_label_set_text(powersave_fan_label, powersave_fan_str);
        }
    }
    
    /* Update cache */
    powersave_cache.hr_state = hr_state;
    powersave_cache.csc_state = csc_state;
    powersave_cache.hr_value = hr_value;
    powersave_cache.csc_value = csc_value;
    powersave_cache.fan_active_count = fan_active_count;
    powersave_cache.fan_total_count = fan_total_count;
    powersave_cache.initialized = true;
}

/* --- Initialization --- */
void powersave_init(void)
{
    lv_obj_t *scr_main = get_main_screen();
    if (!scr_main) return;

    /* Create powersave screen */
    create_powersave_screen();

    /* Touch on main screen: reset inactivity timer; in DIMMED, wake (brightness full) */
    lv_obj_add_event_cb(scr_main, main_screen_touch_cb, LV_EVENT_PRESSING, NULL);

    /* Initialize activity time */
    last_activity_time = lv_tick_get();

    /* Create inactivity timer (check every 500ms) */
    inactivity_timer = lv_timer_create(inactivity_timer_cb, 500, NULL);
    
    /* Ensure display starts at full brightness */
    set_amoled_backlight(BRIGHTNESS_FULL);
}

/* --- Breathing Curve Configuration Functions --- */
void set_breathing_inhale_curve(breathing_ease_type_t ease_type)
{
    inhale_ease = ease_type;
    /* Restart animation if currently breathing */
    if (current_power_state == UI_POWER_STATE_SOFT_POWERSAVE) {
        stop_breathing_animation();
        start_breathing_animation();
    }
}

void set_breathing_exhale_curve(breathing_ease_type_t ease_type)
{
    exhale_ease = ease_type;
    /* Restart animation if currently breathing */
    if (current_power_state == UI_POWER_STATE_SOFT_POWERSAVE) {
        stop_breathing_animation();
        start_breathing_animation();
    }
}

