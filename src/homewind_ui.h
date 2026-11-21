#pragma once
#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- State Enums --- */
typedef enum {
    CSC_NOT_CONFIGURATED = 0,  /* Not configured: row hidden */
    CSC_STATE_INACTIVE,        /* Configured but not connected: "Disconnected" */
    CSC_STATE_ACTIVE           /* Connected, providing data */
} csc_state_t;

/* Power and cadence rows */
typedef enum {
    ROW_HIDDEN = 0,            /* No source configured: row hidden, HR card grows */
    ROW_DISCONNECTED,          /* Dark row, "Disconnected" */
    ROW_ACTIVE                 /* Coloured row with value */
} row_state_t;

typedef enum {
    FAN_STATE_NOT_CONFIGURATED = 0,  /* Dot grey: slot empty */
    FAN_STATE_INACTIVE,              /* Dot white: configured, no info yet */
    FAN_STATE_ERROR,                 /* Dot orange: not reachable or error */
    FAN_STATE_ACTIVE                 /* Dot green while is_on, else white */
} fan_state_t;

typedef enum {
    HR_NOT_CONFIGURATED = 0,   /* Not configured */
    HR_STATE_INACTIVE,         /* Configured but not connected */
    HR_STATE_ACTIVE            /* Connected, providing data */
} hr_state_t;

/* Status badges "APP" and "Trainer" */
typedef enum {
    BADGE_HIDDEN = 0,
    BADGE_GREY,                /* set up, not connected yet */
    BADGE_ORANGE,              /* was connected, now lost */
    BADGE_GREEN                /* connected */
} badge_state_t;

typedef enum {
    FAN_MODE_PURE = 0,
    FAN_MODE_SMART,
    FAN_MODE_HYBRID
} fan_mode_t;

/* --- Callbacks (called from the LVGL task, with the LVGL lock held) --- */
/** Deprecated since 1.7.0: the main screen has no fan toggles; never called. */
typedef void (*fan_toggle_callback_t)(uint8_t fan_index, bool is_on);
/** Plus or minus tapped. steps: -1 = plus (lower the HR range, more wind), +1 = minus. */
typedef void (*hr_shift_callback_t)(int8_t steps);
/** Long press on the fan mode field: the application picks the next mode. */
typedef void (*fan_mode_callback_t)(void);

/* --- Public Functions --- */
void homewind_create_screens(void);

/* ============================================================================
 * HR Widget API
 * ============================================================================ */

/**
 * @brief Set HR widget with all parameters
 * @param state HR_NOT_CONFIGURATED, HR_STATE_INACTIVE, or HR_STATE_ACTIVE
 * @param sensor_name Sensor name (NULL to keep current)
 * @param value Heart rate value (0 = no data yet, shown as "Disconnected")
 */
void homewind_set_hr(hr_state_t state, const char* sensor_name, uint16_t value);

/**
 * @brief Set HR widget state only (keep name and value)
 */
void homewind_set_hr_state(hr_state_t state);

/**
 * @brief Update HR value only (keep state and name)
 */
void homewind_set_hr_value(uint16_t value);

/* Legacy API (kept for backwards compatibility) */
#define homewind_set_hr_state_simple(state) homewind_set_hr_state(state)
#define homewind_set_hr_state_impl(state, name, val) homewind_set_hr(state, name, val)

/* ============================================================================
 * Power and cadence rows
 * ============================================================================ */

/**
 * @brief Set the power row ("210 W")
 * @param name Source name (NULL to keep current); long names are cut off
 */
void homewind_set_power_row(row_state_t state, const char* name, uint16_t watts);

/**
 * @brief Set the cadence row ("85 RPM")
 * @param name Source name (NULL to keep current); long names are cut off
 */
void homewind_set_cadence_row(row_state_t state, const char* name, uint16_t rpm);

/* CSC API: drives the cadence row (NOT_CONFIGURATED = hidden, INACTIVE = disconnected) */
void homewind_set_csc(csc_state_t state, const char* sensor_name, uint16_t cadence);
void homewind_set_csc_state(csc_state_t state);
void homewind_set_csc_cadence(uint16_t cadence);

/**
 * @brief Deprecated since 1.7.0, use homewind_set_power_row()
 * @param watts < 0 hides the power row, >= 0 shows it with the cadence row's name
 */
void homewind_set_csc_power(int32_t watts);

/* Legacy API (kept for backwards compatibility) */
#define homewind_set_csc_state_simple(state) homewind_set_csc_state(state)

/* ============================================================================
 * Status badges
 * ============================================================================ */

void homewind_set_badges(badge_state_t app, badge_state_t trainer);

/* ============================================================================
 * Fan control field (minus | mode, speed, dots | plus)
 * ============================================================================ */

/**
 * @brief Set one fan dot
 * @param fan_index Fan index (0-3)
 * @param state Fan state
 * @param is_on Fan takes part in the control (green dot while ACTIVE)
 */
void homewind_set_fan(uint8_t fan_index, fan_state_t state, bool is_on);

/** @brief Set one fan dot's state only (NOT_CONFIGURATED and INACTIVE clear is_on) */
void homewind_set_fan_state(uint8_t fan_index, fan_state_t state);

/** @brief Set one fan dot's on state only */
void homewind_set_fan_toggle(uint8_t fan_index, bool is_on);

/* Legacy API (kept for backwards compatibility) */
#define homewind_set_fan_state_impl(idx, state, on) homewind_set_fan(idx, state, on)

/**
 * @brief Set the centre field: mode in effect and fan speed in % ("63", "0" while off)
 */
void homewind_set_fan_control(fan_mode_t mode, bool target_on, uint8_t target_pct);

/** @brief Register the plus/minus and mode callbacks (NULL to disable) */
void homewind_set_fan_control_callbacks(hr_shift_callback_t shift, fan_mode_callback_t mode);

/** @brief Deprecated since 1.7.0: stored, never called */
void homewind_set_fan_toggle_callback(fan_toggle_callback_t callback);

/* ============================================================================
 * Settings & Utility
 * ============================================================================ */

/**
 * @brief Set QR code URL in settings modal
 */
void homewind_set_qr_code_url(const char* url);

/**
 * @brief Hide settings overlay (used by powersave module)
 */
void homewind_hide_settings_overlay(void);

/**
 * @brief Refresh the screensaver from the current values (powersave module)
 */
void homewind_refresh_powersave_display(void);

/**
 * @brief Force-refresh all main screen widgets
 *
 * Invalidates all widget caches and re-renders. Call after returning from
 * soft-powersave to ensure main screen reflects current state.
 * @note Must be called when power state is already ACTIVE (not SOFT_POWERSAVE),
 *       otherwise updates will be skipped by should_skip_main_screen_update().
 */
void homewind_refresh_main_display(void);

/* Boot/AP Screen API (see docs/BOOT_AND_AP_SCREEN_IMPLEMENTATION_PLAN.md) */
void homewind_show_ap_screen(void);
void homewind_show_main_screen(void);

#ifdef __cplusplus
}
#endif
