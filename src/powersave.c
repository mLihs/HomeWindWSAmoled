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
#define DIM_TIMEOUT_MS       8000    // 8 s: ACTIVE -> DIMMED
#define SOFT_TIMEOUT_MS      20000   // 20 s: DIMMED -> SOFT_POWERSAVE (screensaver)
#define PANEL_OFF_TIMEOUT_MS (10UL * 60UL * 1000UL)  // 10 min screensaver without activity -> panel off
#define BRIGHTNESS_FULL     255     // Full brightness (100%)
#define BRIGHTNESS_DIMMED   120     // Dimmed brightness (~47%)
#define BRIGHTNESS_SOFT_MIN 80      // Soft powersave minimum brightness (~31%)
#define BRIGHTNESS_SOFT_MAX 120      // Soft powersave maximum brightness (~47%)
#define BREATHING_ANIM_TIME 6000    // 6 second cycle (more natural breathing rate)
#define SLOT_GAP            24      // between the screensaver rows (HR | power, cadence | fan)
#define SLOT_INNER_GAP      8       // icon, dots and value inside a slot
#define SLOT_W              108     // widest value "220 BPM" (106; Figma 90, values without units)
#define SLOT_ICON_H         56      // icon 56 glyph (45 px line) 5 px below the top, as in Figma
#define SLOT_VALUE_H        22      // Bold 24 baseline 20 px below the top (pad 2 + 18)

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
static void set_state_panel_off(void);

/* --- State Variables --- */
static ui_power_state_t current_power_state = UI_POWER_STATE_ACTIVE;
static uint32_t last_activity_time = 0;
static uint32_t screensaver_activity_time = 0;  /* panel-off countdown */
static lv_timer_t *inactivity_timer = NULL;
static bool powersave_locked = true;  /* Locked at init (Boot screen active) */
static int16_t last_brightness = -1;  /* last value sent to the panel, -1 = unknown */

/* --- Screen Objects --- */
typedef struct {
    lv_obj_t *slot;
    lv_obj_t *icon;
    lv_obj_t *value;
} powersave_slot_t;

static lv_obj_t *scr_powersave = NULL;
static lv_obj_t *ps_badge_app = NULL;
static lv_obj_t *ps_badge_trainer = NULL;
static powersave_slot_t ps_hr;
static powersave_slot_t ps_power;
static powersave_slot_t ps_cadence;
static lv_obj_t *ps_sensor_row = NULL;  /* power and cadence side by side */
static powersave_slot_t ps_fan;
static lv_obj_t *ps_fan_dots[POWERSAVE_FANS];
static lv_anim_t brightness_anim;

/* --- Get Main Screen (extern from homewind_ui.c) --- */
extern lv_obj_t *scr_main;

static lv_obj_t *get_main_screen(void)
{
    return scr_main;
}

static void set_brightness(uint8_t brightness)
{
    if (brightness == last_brightness) return;
    last_brightness = brightness;
    set_amoled_backlight(brightness);
}

/* Icon (icon 56) above the value (Bold 24); the slot is hidden until configured */
static void create_powersave_slot(lv_obj_t *parent, powersave_slot_t *s, const char *icon, uint32_t color)
{
    s->slot = lv_obj_create(parent);
    apply_transparent_container_style(s->slot);
    lv_obj_set_size(s->slot, SLOT_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s->slot, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s->slot, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s->slot, SLOT_INNER_GAP, 0);
    lv_obj_add_flag(s->slot, LV_OBJ_FLAG_HIDDEN);

    s->icon = lv_label_create(s->slot);
    lv_label_set_text(s->icon, icon);
    lv_obj_set_style_text_color(s->icon, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(s->icon, &lv_font_icon_56, 0);
    lv_obj_set_style_pad_top(s->icon, 5, 0);
    lv_obj_set_height(s->icon, SLOT_ICON_H);

    s->value = lv_label_create(s->slot);
    lv_label_set_text(s->value, "");
    lv_obj_set_style_text_color(s->value, lv_color_white(), 0);
    lv_obj_set_style_text_font(s->value, &lv_font_inter_bold_24, 0);
    lv_obj_set_style_pad_top(s->value, 2, 0);
    lv_obj_set_height(s->value, SLOT_VALUE_H);
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
    LV_UNUSED(var);
    /* Clamp value to valid brightness range and cast to uint8_t */
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    set_brightness((uint8_t)value);
}

static void start_breathing_animation(void)
{
    if (!scr_powersave) return;
    
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

/* --- Powersave Screen Creation: badges at the top, then HR, power, cadence, fan --- */
static void create_powersave_screen(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t w = lv_disp_get_hor_res(disp);
    lv_coord_t h = lv_disp_get_ver_res(disp);

    scr_powersave = lv_obj_create(NULL);
    lv_obj_clear_flag(scr_powersave, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr_powersave, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr_powersave, LV_OPA_COVER, 0);
    /* Children are not clickable: every touch reaches the screen */
    lv_obj_add_event_cb(scr_powersave, overlay_touch_event_cb, LV_EVENT_PRESSED, NULL);

    lv_obj_t *badges = create_badge_row(scr_powersave, &ps_badge_app, &ps_badge_trainer);
    lv_obj_align(badges, LV_ALIGN_TOP_MID, 0, 13);

    lv_obj_t *column = lv_obj_create(scr_powersave);
    apply_transparent_container_style(column);
    lv_obj_set_size(column, w, h - 44);
    lv_obj_set_pos(column, 0, 44);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(column, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(column, SLOT_GAP, 0);

    create_powersave_slot(column, &ps_hr, ICON_HEART, COLOR_ACCENT);

    ps_sensor_row = lv_obj_create(column);
    apply_transparent_container_style(ps_sensor_row);
    lv_obj_set_size(ps_sensor_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ps_sensor_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(ps_sensor_row, 0, 0);
    lv_obj_add_flag(ps_sensor_row, LV_OBJ_FLAG_HIDDEN);
    create_powersave_slot(ps_sensor_row, &ps_power, ICON_BOLT, COLOR_POWER);
    create_powersave_slot(ps_sensor_row, &ps_cadence, ICON_CRANK, COLOR_CADENCE_LIGHT);

    create_powersave_slot(column, &ps_fan, ICON_FAN, COLOR_GREEN);
    /* Fan slot: icon, dots, value */
    lv_obj_t *dots = create_fan_dots(ps_fan.slot, ps_fan_dots);
    lv_obj_move_to_index(dots, 1);
}

/* --- Touch on the screensaver: wake to the main screen --- */
static void overlay_touch_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    on_user_activity();
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
    if (current_power_state == UI_POWER_STATE_PANEL_OFF) {
        lcd_set_display_on(true);
    }
    if (current_power_state == UI_POWER_STATE_SOFT_POWERSAVE) {
        stop_breathing_animation();
    }
    if (current_power_state != UI_POWER_STATE_ACTIVE) {
        set_brightness(BRIGHTNESS_FULL);
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
            set_brightness(BRIGHTNESS_FULL);
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
    } else if (current_power_state == UI_POWER_STATE_SOFT_POWERSAVE &&
               now - screensaver_activity_time > PANEL_OFF_TIMEOUT_MS) {
        set_state_panel_off();
    }
    /* Screensaver and panel off last until activity */
}

/* --- State Transition Functions --- */
void set_state_active(void)
{
    on_user_activity();
}

void set_state_dimmed(void)
{
    lv_obj_t *scr_main = get_main_screen();
    if (!scr_main) return;

    const bool was_screensaver = current_power_state >= UI_POWER_STATE_SOFT_POWERSAVE;
    if (current_power_state == UI_POWER_STATE_PANEL_OFF) {
        lcd_set_display_on(true);
    }
    if (was_screensaver) {
        lv_scr_load(scr_main);
        stop_breathing_animation();
    }

    set_brightness(BRIGHTNESS_DIMMED);
    current_power_state = UI_POWER_STATE_DIMMED;
    if (was_screensaver) {
        homewind_refresh_main_display();   /* updates were skipped in the screensaver */
    }
}

void set_state_soft_powersave(void)
{
    if (!scr_powersave) {
        create_powersave_screen();
    }
    if (current_power_state == UI_POWER_STATE_PANEL_OFF) {
        lcd_set_display_on(true);
    }

    /* Set state first so any code during/after load sees SOFT_POWERSAVE */
    current_power_state = UI_POWER_STATE_SOFT_POWERSAVE;
    screensaver_activity_time = lv_tick_get();

    /* Set initial soft powersave brightness (will be animated) */
    set_brightness(BRIGHTNESS_SOFT_MIN);

    lv_scr_load(scr_powersave);
    homewind_refresh_powersave_display();
    start_breathing_animation();
}

/* Burn-in protection: no pixel shift, the panel goes dark */
static void set_state_panel_off(void)
{
    stop_breathing_animation();
    set_brightness(0);
    lcd_set_display_on(false);
    current_power_state = UI_POWER_STATE_PANEL_OFF;
}

ui_power_state_t get_ui_power_state(void)
{
    return current_power_state;
}

/* --- User Activity Handler (touch) --- */
void on_user_activity(void)
{
    last_activity_time = lv_tick_get();

    if (current_power_state != UI_POWER_STATE_ACTIVE) {
        bool was_screensaver = (current_power_state >= UI_POWER_STATE_SOFT_POWERSAVE);

        if (current_power_state == UI_POWER_STATE_PANEL_OFF) {
            lcd_set_display_on(true);
        }
        if (was_screensaver) {
            lv_obj_t *scr_main = get_main_screen();
            if (scr_main) {
                /* Screens have no PRESS_LOCK: the finger still down would press the control
                 * under it on the main screen as a new touch. Ignore it until released. */
                lv_indev_t *indev = lv_indev_get_act();
                if (indev) lv_indev_wait_release(indev);
                lv_scr_load(scr_main);
                stop_breathing_animation();
                homewind_hide_settings_overlay();
            }
        }

        set_brightness(BRIGHTNESS_FULL);
        current_power_state = UI_POWER_STATE_ACTIVE;

        /* FIX: Refresh main screen widgets after powersave wake.
         * During SOFT_POWERSAVE, main screen updates were skipped by
         * should_skip_main_screen_update(). State must be ACTIVE before
         * this call so the skip-check passes. */
        if (was_screensaver) {
            homewind_refresh_main_display();
        }
    }
}

bool powersave_touch_wakes(void)
{
    const bool wakes = current_power_state != UI_POWER_STATE_ACTIVE;
    on_user_activity();
    return wakes;
}

void homewind_powersave_wake(void)
{
    screensaver_activity_time = lv_tick_get();
    if (current_power_state == UI_POWER_STATE_PANEL_OFF) {
        lcd_set_display_on(true);
        current_power_state = UI_POWER_STATE_SOFT_POWERSAVE;
        start_breathing_animation();
    }
}

/* --- Cached values for dirty-checking (avoid redundant UI updates) --- */
static powersave_view_t powersave_cache;
static bool powersave_cache_valid = false;

/* Icon in its colour with the value, or the grey icon with "--" while disconnected */
static void set_slot(powersave_slot_t *s, bool configured, bool connected, uint32_t color,
                     const char *value)
{
    if (!configured) {
        lv_obj_add_flag(s->slot, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s->slot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(s->icon, lv_color_hex(connected ? color : COLOR_DISCONNECTED), 0);
    lv_label_set_text(s->value, connected ? value : "--");
}

/* --- Update the screensaver (only while it is shown, and only what changed) --- */
void update_powersave_view(const powersave_view_t *v)
{
    if (!scr_powersave || !v) return;
    if (current_power_state < UI_POWER_STATE_SOFT_POWERSAVE && powersave_cache_valid) {
        return;
    }
    const powersave_view_t *c = powersave_cache_valid ? &powersave_cache : NULL;
    char buf[16];

    if (!c || c->app != v->app || c->trainer != v->trainer) {
        set_badge(ps_badge_app, v->app);
        set_badge(ps_badge_trainer, v->trainer);
    }
    if (!c || c->hr_state != v->hr_state || c->hr_value != v->hr_value) {
        const bool live = v->hr_state == HR_STATE_ACTIVE && v->hr_value > 0;
        lv_label_set_text(ps_hr.icon, live ? ICON_HEART : ICON_HEART_OFF);
        snprintf(buf, sizeof(buf), "%u BPM", (unsigned)v->hr_value);
        set_slot(&ps_hr, v->hr_state != HR_NOT_CONFIGURATED, live, COLOR_ACCENT, buf);
    }
    if (!c || c->power_state != v->power_state || c->power_w != v->power_w) {
        snprintf(buf, sizeof(buf), "%u W", (unsigned)v->power_w);
        set_slot(&ps_power, v->power_state != ROW_HIDDEN, v->power_state == ROW_ACTIVE, COLOR_POWER, buf);
    }
    if (!c || c->cadence_state != v->cadence_state || c->cadence_rpm != v->cadence_rpm) {
        snprintf(buf, sizeof(buf), "%u RPM", (unsigned)v->cadence_rpm);
        set_slot(&ps_cadence, v->cadence_state != ROW_HIDDEN, v->cadence_state == ROW_ACTIVE,
                 COLOR_CADENCE_LIGHT, buf);
    }
    if (v->power_state == ROW_HIDDEN && v->cadence_state == ROW_HIDDEN) {
        lv_obj_add_flag(ps_sensor_row, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(ps_sensor_row, LV_OBJ_FLAG_HIDDEN);
    }
    bool fans_changed = !c || c->fan_target_on != v->fan_target_on || c->fan_pct != v->fan_pct;
    bool any_fan = false;
    for (uint8_t i = 0; i < POWERSAVE_FANS; i++) {
        any_fan = any_fan || v->fan_state[i] != FAN_STATE_NOT_CONFIGURATED;
        if (!c || c->fan_state[i] != v->fan_state[i] || c->fan_on[i] != v->fan_on[i]) {
            lv_obj_set_style_bg_color(ps_fan_dots[i], lv_color_hex(fan_dot_color(v->fan_state[i], v->fan_on[i])), 0);
            fans_changed = true;
        }
    }
    if (fans_changed) {
        const bool running = v->fan_target_on && v->fan_pct > 0;
        snprintf(buf, sizeof(buf), "%u %%", running ? (unsigned)v->fan_pct : 0u);
        set_slot(&ps_fan, any_fan, true, running ? COLOR_GREEN : COLOR_WHITE, buf);
    }

    powersave_cache = *v;
    powersave_cache_valid = true;
}

/* --- Initialization --- */
void powersave_init(void)
{
    lv_obj_t *scr_main = get_main_screen();
    if (!scr_main) return;

    /* Create powersave screen */
    create_powersave_screen();

    /* Initialize activity time */
    last_activity_time = lv_tick_get();

    /* Create inactivity timer (check every 500ms) */
    inactivity_timer = lv_timer_create(inactivity_timer_cb, 500, NULL);
    
    /* Ensure display starts at full brightness */
    set_brightness(BRIGHTNESS_FULL);
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

