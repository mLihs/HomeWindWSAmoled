#pragma once
#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- State Enums --- */
typedef enum {
    CSC_NOT_CONFIGURATED = 0,  /* Not configured */
    CSC_STATE_INACTIVE,        /* Configured but not connected */
    CSC_STATE_ACTIVE           /* Connected, providing data */
} csc_state_t;

typedef enum {
    FAN_STATE_NOT_CONFIGURATED = 0,  /* Gray, not clickable */
    FAN_STATE_INACTIVE,              /* Active green, off, clickable */
    FAN_STATE_ERROR,                  /* Error pink/red, clickable, no icon */
    FAN_STATE_ACTIVE                  /* Active green, on, no checkmark, clickable */
} fan_state_t;

typedef enum {
    HR_NOT_CONFIGURATED = 0,   /* Not configured */
    HR_STATE_INACTIVE,         /* Configured but not connected */
    HR_STATE_ACTIVE            /* Connected, providing data */
} hr_state_t;

/* --- Fan Toggle Callback --- */
typedef void (*fan_toggle_callback_t)(uint8_t fan_index, bool is_on);

/* --- Public Functions --- */
void homewind_create_screens(void);

/* ============================================================================
 * HR Widget API (clean, explicit functions - no macro magic)
 * ============================================================================ */

/**
 * @brief Set HR widget with all parameters
 * @param state HR_NOT_CONFIGURATED, HR_STATE_INACTIVE, or HR_STATE_ACTIVE
 * @param sensor_name Sensor name (NULL to keep current)
 * @param value Heart rate value (0 = no data yet, shown as "--")
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
 * CSC Widget API
 * ============================================================================ */

/**
 * @brief Set CSC widget with all parameters
 * @param state CSC_NOT_CONFIGURATED, CSC_STATE_INACTIVE, or CSC_STATE_ACTIVE
 * @param sensor_name Sensor name (NULL to keep current)
 * @param cadence Cadence value in RPM (0 = not pedaling)
 */
void homewind_set_csc(csc_state_t state, const char* sensor_name, uint16_t cadence);

/**
 * @brief Set CSC widget state only (keep name and cadence)
 */
void homewind_set_csc_state(csc_state_t state);

/**
 * @brief Update CSC cadence only (keep state and name)
 */
void homewind_set_csc_cadence(uint16_t cadence);

/* Legacy API (kept for backwards compatibility) */
#define homewind_set_csc_state_simple(state) homewind_set_csc_state(state)

/* ============================================================================
 * Fan Widget API
 * ============================================================================ */

/**
 * @brief Set fan widget with all parameters
 * @param fan_index Fan index (0-3)
 * @param state FAN_STATE_NOT_CONFIGURATED, FAN_STATE_INACTIVE, FAN_STATE_ERROR, or FAN_STATE_ACTIVE
 * @param is_on Toggle state (true = on, false = off)
 */
void homewind_set_fan(uint8_t fan_index, fan_state_t state, bool is_on);

/**
 * @brief Set fan widget state only (keep is_on state, default false for new)
 * @param fan_index Fan index (0-3)
 * @param state Fan state
 */
void homewind_set_fan_state(uint8_t fan_index, fan_state_t state);

/**
 * @brief Toggle fan on/off (keep current state)
 * @param fan_index Fan index (0-3)
 * @param is_on Toggle state
 */
void homewind_set_fan_toggle(uint8_t fan_index, bool is_on);

/* Legacy API (kept for backwards compatibility) */
#define homewind_set_fan_state_impl(idx, state, on) homewind_set_fan(idx, state, on)

/**
 * @brief Register callback for fan toggle events
 */
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
 * @brief Refresh powersave screen labels (HR/CSC/fan) – call when entering SOFT_POWERSAVE
 */
void homewind_refresh_powersave_display(void);

/**
 * @brief Force-refresh all main screen widgets (HR/CSC/fans)
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
