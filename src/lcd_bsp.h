#ifndef LCD_BSP_H
#define LCD_BSP_H
#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "lv_conf_psram_auto.h"
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "esp_check.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif 

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
void example_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area);
static void example_increase_lvgl_tick(void *arg);
static void example_lvgl_port_task(void *arg);
static void example_lvgl_unlock(void);
static bool example_lvgl_lock(int timeout_ms);
void lcd_lvgl_Init(void);
/** Lock LVGL mutex before calling LVGL from another task (e.g. setup/loop). timeout_ms: -1 = wait forever. */
bool lcd_lvgl_lock(int timeout_ms);
/** Unlock LVGL mutex after lcd_lvgl_lock(). */
void lcd_lvgl_unlock(void);
static void example_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data);
esp_err_t set_amoled_backlight(uint8_t brig);
void lcd_set_rotation(lv_disp_rot_t rotation);

#ifdef QMI8658_SLAVE_ADDR_L
void lcd_set_imu_tilt_thresholds(float tilt_right, float tilt_left);
void lcd_get_imu_tilt_thresholds(float *tilt_right, float *tilt_left);
/**
 * @brief Initialize and start the IMU rotation task
 * 
 * This function initializes the QMI8658 IMU sensor and starts a background
 * task that automatically adjusts display rotation based on accelerometer data.
 * 
 * @return true if IMU was initialized and task started successfully
 * @return false if IMU initialization failed (hardware not present)
 * 
 * @note Only available if QMI8658 hardware is detected
 * @note This function is called automatically by homewind_init()
 */
bool lcd_start_imu_rotation_task(void);
#endif

#ifdef __cplusplus
}
#endif

#endif