/*
 * HomeWindWSAmoled.h
 * 
 * Main header file for HomeWindWSAmoled Arduino Library
 * 
 * A comprehensive UI library for ESP32-based fitness/cycling devices
 * featuring AMOLED display support, LVGL graphics, and power management.
 * 
 * Version: 1.5.9
 * 
 * Features:
 * - Heart Rate (HR) widget with sensor state management
 * - Cadence Sensor (CSC) widget with RPM display
 * - Fan control widget (up to 4 fans)
 * - Power save modes (Active, Dimmed, Soft Powersave)
 * - Touch screen support
 * - QR code settings modal
 * - Display rotation (manual and automatic IMU-based)
 * 
 * Hardware Requirements:
 * - ESP32 microcontroller
 * - SH8601 AMOLED display (280×456 pixels)
 * - FT3168 touch controller
 * - QMI8658 IMU sensor (optional, for automatic rotation)
 * 
 * Dependencies:
 * - LVGL (Light and Versatile Graphics Library) v8
 * 
 * Note: QMI8658 IMU driver is included in this library for automatic rotation support
 * 
 * Author: HomeWind Project
 * License: See LICENSE file
 */

#ifndef HOMEWIND_WS_AMOLED_H
#define HOMEWIND_WS_AMOLED_H

#include "Arduino.h"

// Include all library headers
#include "lcd_bsp.h"
#include "FT3168.h"
#include "homewind_ui.h"
#include "powersave.h"

// Library version information
#define HOMEWIND_WS_AMOLED_VERSION_MAJOR 1
#define HOMEWIND_WS_AMOLED_VERSION_MINOR 5
#define HOMEWIND_WS_AMOLED_VERSION_PATCH 9
#define HOMEWIND_WS_AMOLED_VERSION "1.5.9"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the HomeWindWSAmoled library
 * 
 * This function initializes the display, LVGL, touch controller,
 * and creates all UI screens. Call this in your setup() function.
 * 
 * The function is idempotent - calling it multiple times is safe
 * and will only initialize once.
 * 
 * Initialization order:
 * 1. Touch controller (FT3168)
 * 2. Display and LVGL (lcd_lvgl_Init)
 * 3. UI screens (homewind_create_screens)
 * 4. Power save system (powersave_init)
 * 5. IMU rotation task (if QMI8658 hardware detected)
 * 
 * @note This function must be called before using any other library functions
 * 
 * @code
 * void setup() {
 *   Serial.begin(115200);
 *   homewind_init();
 * }
 * @endcode
 */
void homewind_init(void);

#ifdef __cplusplus
}
#endif

/**
 * @brief Set display rotation
 * 
 * Manually set the display rotation angle. This function can be called
 * at any time to change the screen orientation.
 * 
 * @param rotation Rotation angle:
 *   - LV_DISP_ROT_0   : No rotation (0°)
 *   - LV_DISP_ROT_90  : Rotate 90° clockwise
 *   - LV_DISP_ROT_180 : Rotate 180°
 *   - LV_DISP_ROT_270 : Rotate 270° clockwise (or 90° counter-clockwise)
 * 
 * @note Automatic rotation based on IMU sensor is enabled automatically
 *       if QMI8658 hardware is detected. No additional code needed!
 * 
 * @example
 * @code
 * // Rotate display 180 degrees
 * homewind_set_rotation(LV_DISP_ROT_180);
 * @endcode
 */
#define homewind_set_rotation(rotation) lcd_set_rotation(rotation)

/**
 * @brief Set IMU tilt angle thresholds for automatic rotation
 * 
 * Adjust the sensitivity of automatic rotation detection. The thresholds
 * are in m/s² (meters per second squared) and represent the accelerometer
 * X-axis value that triggers rotation.
 * 
 * @param tilt_right Threshold for tilting right (normal orientation, 0°)
 *                   Default: 1.0 m/s²
 *                   Higher values = less sensitive (need more tilt)
 *                   Lower values = more sensitive (less tilt needed)
 * 
 * @param tilt_left Threshold for tilting left (180° rotation)
 *                  Default: -1.0 m/s²
 *                  More negative = less sensitive (need more tilt)
 *                  Less negative = more sensitive (less tilt needed)
 * 
 * @note Only works if QMI8658 IMU hardware is present
 * 
 * @example
 * @code
 * // Make rotation more sensitive (trigger with less tilt)
 * homewind_set_imu_tilt_thresholds(0.5f, -0.5f);
 * 
 * // Make rotation less sensitive (need more tilt to trigger)
 * homewind_set_imu_tilt_thresholds(2.0f, -2.0f);
 * @endcode
 */
#define homewind_set_imu_tilt_thresholds(tilt_right, tilt_left) lcd_set_imu_tilt_thresholds(tilt_right, tilt_left)

/**
 * @brief Get current IMU tilt angle thresholds
 * 
 * @param tilt_right Pointer to store right tilt threshold (can be NULL)
 * @param tilt_left Pointer to store left tilt threshold (can be NULL)
 * 
 * @note Only works if QMI8658 IMU hardware is present
 */
#define homewind_get_imu_tilt_thresholds(tilt_right, tilt_left) lcd_get_imu_tilt_thresholds(tilt_right, tilt_left)

#endif // HOMEWIND_WS_AMOLED_H

