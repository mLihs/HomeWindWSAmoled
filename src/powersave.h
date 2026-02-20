#pragma once
#include "lvgl.h"
#include "homewind_ui.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- UI Power State Enum --- */
typedef enum {
    UI_POWER_STATE_ACTIVE = 0,
    UI_POWER_STATE_DIMMED,
    UI_POWER_STATE_SOFT_POWERSAVE
} ui_power_state_t;

/* --- Breathing Animation Easing Types --- */
typedef enum {
    BREATHING_EASE_LINEAR = 0,
    BREATHING_EASE_QUADRATIC_IN,
    BREATHING_EASE_QUADRATIC_OUT,
    BREATHING_EASE_QUADRATIC_IN_OUT,
    BREATHING_EASE_CUBIC_IN,
    BREATHING_EASE_CUBIC_OUT,
    BREATHING_EASE_CUBIC_IN_OUT,
    BREATHING_EASE_SINE_IN,
    BREATHING_EASE_SINE_OUT,
    BREATHING_EASE_SINE_IN_OUT
} breathing_ease_type_t;

/* --- Public API --- */
void powersave_init(void);
void on_user_activity(void);
void powersave_lock(void);    /* Lock powersave (Boot/AP screen active) */
void powersave_unlock(void);  /* Unlock powersave (Main screen active) */
ui_power_state_t get_ui_power_state(void);
void set_state_active(void);
void set_state_dimmed(void);
void set_state_soft_powersave(void);
void update_powersave_icons(hr_state_t hr_state, csc_state_t csc_state, 
                            uint16_t hr_value, uint16_t csc_value, uint8_t fan_active_count, uint8_t fan_total_count);

/* --- Breathing Curve Configuration --- */
void set_breathing_inhale_curve(breathing_ease_type_t ease_type);
void set_breathing_exhale_curve(breathing_ease_type_t ease_type);

#ifdef __cplusplus
}
#endif

