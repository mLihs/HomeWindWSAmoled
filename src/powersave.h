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
    UI_POWER_STATE_SOFT_POWERSAVE,
    UI_POWER_STATE_PANEL_OFF       /* screensaver, panel switched off (burn-in protection) */
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

/* --- Screensaver content (filled by homewind_ui.c) --- */
#define POWERSAVE_FANS 4
typedef struct {
    hr_state_t hr_state;
    uint16_t hr_value;
    row_state_t power_state;
    uint16_t power_w;
    row_state_t cadence_state;
    uint16_t cadence_rpm;
    badge_state_t app;
    badge_state_t trainer;
    fan_state_t fan_state[POWERSAVE_FANS];
    bool fan_on[POWERSAVE_FANS];
    bool fan_target_on;
    uint8_t fan_pct;
} powersave_view_t;

/* --- Public API --- */
void powersave_init(void);
void on_user_activity(void);
void powersave_lock(void);    /* Lock powersave (Boot/AP screen active) */
void powersave_unlock(void);  /* Unlock powersave (Main screen active) */
ui_power_state_t get_ui_power_state(void);
void set_state_active(void);
void set_state_dimmed(void);
void set_state_soft_powersave(void);

/**
 * @brief Activity that is not a touch (sensor connects or sends data, app connects,
 *        web UI change). Restarts the panel-off timeout; a switched-off panel comes
 *        back on with the screensaver. Main screen and dim timeout stay as they are.
 *        Call under the LVGL lock; cheap enough to call on every data update.
 */
void homewind_powersave_wake(void);

/**
 * @brief Touch on a control (homewind_ui.c, on LV_EVENT_PRESSED)
 * @return true if the touch only woke the display: the control must ignore this press
 */
bool powersave_touch_wakes(void);

void update_powersave_view(const powersave_view_t *view);

/* --- Breathing Curve Configuration --- */
void set_breathing_inhale_curve(breathing_ease_type_t ease_type);
void set_breathing_exhale_curve(breathing_ease_type_t ease_type);

#ifdef __cplusplus
}
#endif
