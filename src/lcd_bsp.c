#include "lcd_bsp.h"
#include "esp_lcd_sh8601.h"
#include "lcd_config.h"
#include "homewind_ui.h"
#include "FT3168.h"
#include "qmi8658c.h"  // QMI8658 IMU driver - automatically included for rotation support

static SemaphoreHandle_t lvgl_mux = NULL; //mutex semaphores
#define LCD_HOST    SPI2_HOST

//#define EXAMPLE_Rotate_90
#define SH8601_ID 0x86
#define CO5300_ID 0xff



static esp_lcd_panel_io_handle_t amoled_panel_io_handle = NULL; 
static lv_disp_t *g_display = NULL;
static lv_disp_drv_t *g_disp_drv = NULL;

#ifdef QMI8658_SLAVE_ADDR_L
// Global variables for IMU rotation tilt angles (in m/s²)
// These control the sensitivity of automatic rotation detection
static float g_imu_tilt_right_threshold = 1.0f;   // Tilt right threshold (normal orientation)
static float g_imu_tilt_left_threshold = -1.0f;   // Tilt left threshold (180° rotation)
#endif

// Forward declaration for IMU rotation task (if QMI8658 is available)
// This will only compile if qmi8658c.h was included before this file
#ifdef QMI8658_SLAVE_ADDR_L
static void example_imu_rotation_task(void *arg);
#endif

static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = 
{
  {0x11, (uint8_t []){0x00}, 0, 80},   
  {0xC4, (uint8_t []){0x80}, 1, 0},
  
  {0x35, (uint8_t []){0x00}, 1, 0},

  {0x53, (uint8_t []){0x20}, 1, 1},
  {0x63, (uint8_t []){0xFF}, 1, 1},
  {0x51, (uint8_t []){0x00}, 1, 1},

  {0x29, (uint8_t []){0x00}, 0, 10},

  {0x51, (uint8_t []){0xFF}, 1, 0},    //亮度
};

void lcd_lvgl_Init(void)
{
  static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
  static lv_disp_drv_t disp_drv;      // contains callback functions

  const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK,
                                                               EXAMPLE_PIN_NUM_LCD_DATA0,
                                                               EXAMPLE_PIN_NUM_LCD_DATA1,
                                                               EXAMPLE_PIN_NUM_LCD_DATA2,
                                                               EXAMPLE_PIN_NUM_LCD_DATA3,
                                                               EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * LCD_BIT_PER_PIXEL / 8);
  ESP_ERROR_CHECK_WITHOUT_ABORT(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
  esp_lcd_panel_io_handle_t io_handle = NULL;

  const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS,
                                                                              example_notify_lvgl_flush_ready,
                                                                              &disp_drv);

  sh8601_vendor_config_t vendor_config = 
  {
    .init_cmds = lcd_init_cmds,
    .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
    .flags = 
    {
      .use_qspi_interface = 1,
    },
  };
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
  amoled_panel_io_handle = io_handle;
  esp_lcd_panel_handle_t panel_handle = NULL;
  const esp_lcd_panel_dev_config_t panel_config = 
  {
    .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = LCD_BIT_PER_PIXEL,
    .vendor_config = &vendor_config,
  };
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_disp_on_off(panel_handle, true));

  lv_init();
  lv_color_t *buf1 = heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_DMA);
  assert(buf1);
  lv_color_t *buf2 = heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_DMA);
  assert(buf2);
  lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = EXAMPLE_LCD_H_RES;
  disp_drv.ver_res = EXAMPLE_LCD_V_RES;
  disp_drv.flush_cb = example_lvgl_flush_cb;
  disp_drv.rounder_cb = example_lvgl_rounder_cb;
  disp_drv.draw_buf = &disp_buf;
  disp_drv.user_data = panel_handle;
#ifdef EXAMPLE_Rotate_90
  disp_drv.sw_rotate = 1;
  disp_drv.rotated = LV_DISP_ROT_270;
#endif
  lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
  g_display = disp;
  g_disp_drv = disp->driver;

  static lv_indev_drv_t indev_drv;    // Input device driver (Touch)
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.disp = disp;
  indev_drv.read_cb = example_lvgl_touch_cb;
  lv_indev_drv_register(&indev_drv);

  const esp_timer_create_args_t lvgl_tick_timer_args = 
  {
    .callback = &example_increase_lvgl_tick,
    .name = "lvgl_tick"
  };
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

  lvgl_mux = xSemaphoreCreateMutex(); //mutex semaphores
  assert(lvgl_mux);
  xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);
  
  // NOTE: UI initialization (homewind_create_screens, powersave_init) and
  // IMU rotation task are now handled by homewind_init() - not here in BSP layer.
  // This separation ensures clean architecture: BSP only initializes hardware/LVGL,
  // application layer (homewind_init) creates UI, starts powersave, and IMU tasks.
}

static bool example_lvgl_lock(int timeout_ms)
{
  assert(lvgl_mux && "bsp_display_start must be called first");

  const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
  return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

static void example_lvgl_unlock(void)
{
  assert(lvgl_mux && "bsp_display_start must be called first");
  xSemaphoreGive(lvgl_mux);
}

bool lcd_lvgl_lock(int timeout_ms)
{
  return example_lvgl_lock(timeout_ms);
}

void lcd_lvgl_unlock(void)
{
  example_lvgl_unlock();
}

static void example_lvgl_port_task(void *arg)
{
  uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
  for(;;)
  {
    if (example_lvgl_lock(-1))
    {
      task_delay_ms = lv_timer_handler();
      
      example_lvgl_unlock();
    }
    if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS)
    {
      task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    }
    else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS)
    {
      task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
    }
    vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
  }
}
static void example_increase_lvgl_tick(void *arg)
{
  lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}
static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
  lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
  lv_disp_flush_ready(disp_driver);
  return false;
}
static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
  esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
  const int offsetx1 = area->x1 + 0x14;
  const int offsetx2 = area->x2 + 0x14;
  const int offsety1 = area->y1;
  const int offsety2 = area->y2;

  esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}
void example_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area)
{
  uint16_t x1 = area->x1;
  uint16_t x2 = area->x2;

  uint16_t y1 = area->y1;
  uint16_t y2 = area->y2;

  // round the start of coordinate down to the nearest 2M number
  area->x1 = (x1 >> 1) << 1;
  area->y1 = (y1 >> 1) << 1;
  // round the end of coordinate up to the nearest 2N+1 number
  area->x2 = ((x2 >> 1) << 1) + 1;
  area->y2 = ((y2 >> 1) << 1) + 1;
}
// Touch polling rate limiting (reduces I2C overhead)
// With 5ms LVGL tick, this means touch is polled every ~15ms (≈66Hz) - more responsive
#define TOUCH_POLL_SKIP_COUNT 2  // Poll every N+1 LVGL ticks

static void example_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
  static uint8_t poll_counter = 0;
  static uint16_t last_x = 0, last_y = 0;
  static lv_indev_state_t last_state = LV_INDEV_STATE_RELEASED;
  
  // Rate limiting: only poll I2C every N ticks
  if (++poll_counter <= TOUCH_POLL_SKIP_COUNT) {
    // Return cached values
    data->point.x = last_x;
    data->point.y = last_y;
    data->state = last_state;
    return;
  }
  poll_counter = 0;
  
  // Actually poll touch controller
  uint16_t tp_x, tp_y;
  uint8_t win = getTouch(&tp_x, &tp_y);
  if (win)
  {
    last_x = tp_x;
    last_y = tp_y;
    last_state = LV_INDEV_STATE_PRESSED;
  }
  else
  {
    last_state = LV_INDEV_STATE_RELEASED;
  }
  
  data->point.x = last_x;
  data->point.y = last_y;
  data->state = last_state;
}


esp_err_t set_amoled_backlight(uint8_t brig)
{
  uint32_t lcd_cmd = 0x51;
  lcd_cmd &= 0xff;
  lcd_cmd <<= 8;
  lcd_cmd |= 0x02 << 24;
  return esp_lcd_panel_io_tx_param(amoled_panel_io_handle, lcd_cmd, &brig,1);
}

void lcd_set_rotation(lv_disp_rot_t rotation) {
    if (g_display && g_display->driver) {
        // Lock LVGL before modifying
        if (example_lvgl_lock(100)) {
            // Modify the driver's rotation settings
            g_display->driver->sw_rotate = 1;
            g_display->driver->rotated = rotation;
            
            // Only invalidate if screen exists (safety check)
            lv_obj_t* screen = lv_scr_act();
            if (screen != NULL) {
                lv_obj_invalidate(screen);
            }
            
            // Trigger a refresh
            lv_refr_now(g_display);
            
            example_lvgl_unlock();
        }
    }
}

#ifdef QMI8658_SLAVE_ADDR_L
// IMU Rotation Task Configuration
#define IMU_ROTATION_CHECK_INTERVAL_MS  200   // Check every 200ms (5Hz) - sufficient for rotation
#define IMU_ROTATION_HYSTERESIS         0.3f  // Hysteresis band to prevent flicker at threshold
#define IMU_ROTATION_STABLE_COUNT       2     // Require N consecutive readings before rotation

static void example_imu_rotation_task(void *arg)
{
    float acc[3];
    float gyro[3];
    static lv_disp_rot_t current_rotation = (lv_disp_rot_t)0;
    static lv_disp_rot_t pending_rotation = (lv_disp_rot_t)0;
    static uint8_t stable_count = 0;
    
    // Wait for display and screens to be fully initialized
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    for(;;)
    {
        // Only proceed if display is ready and screen exists
        if (g_display && g_display->driver && lv_scr_act() != NULL) {
            qmi8658_read_xyz(acc, gyro);
            
            // Determine target rotation with hysteresis
            // Use higher threshold to enter new state, lower to exit (prevents oscillation)
            lv_disp_rot_t target_rotation = current_rotation;
            
            if (current_rotation == (lv_disp_rot_t)0) {
                // Currently normal: need stronger signal to rotate to 180°
                if (acc[0] < g_imu_tilt_left_threshold - IMU_ROTATION_HYSTERESIS) {
                    target_rotation = LV_DISP_ROT_180;
                }
            } else {
                // Currently 180°: need stronger signal to rotate to normal
                if (acc[0] > g_imu_tilt_right_threshold + IMU_ROTATION_HYSTERESIS) {
                    target_rotation = (lv_disp_rot_t)0;
                }
            }
            
            // Require stable readings before actually rotating (debounce)
            if (target_rotation != current_rotation) {
                if (target_rotation == pending_rotation) {
                    stable_count++;
                    if (stable_count >= IMU_ROTATION_STABLE_COUNT) {
                        lcd_set_rotation(target_rotation);
                        current_rotation = target_rotation;
                        stable_count = 0;
                    }
                } else {
                    pending_rotation = target_rotation;
                    stable_count = 1;
                }
            } else {
                stable_count = 0;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(IMU_ROTATION_CHECK_INTERVAL_MS));
    }
}

// Function to set IMU rotation tilt thresholds
void lcd_set_imu_tilt_thresholds(float tilt_right, float tilt_left) {
    g_imu_tilt_right_threshold = tilt_right;
    g_imu_tilt_left_threshold = tilt_left;
}

// Function to get current IMU rotation tilt thresholds
void lcd_get_imu_tilt_thresholds(float *tilt_right, float *tilt_left) {
    if (tilt_right) *tilt_right = g_imu_tilt_right_threshold;
    if (tilt_left) *tilt_left = g_imu_tilt_left_threshold;
}

// Public API: Initialize IMU and start rotation task
// Returns true if successful, false if IMU not present/init failed
bool lcd_start_imu_rotation_task(void) {
    // Initialize QMI8658 IMU
    if (qmi8658_init()) {
        // Create IMU rotation task if initialization succeeded
        // Task has internal 2s delay to wait for display to be fully ready
        xTaskCreate(example_imu_rotation_task, "IMU_Rotation", 4096, NULL, 1, NULL);
        return true;
    }
    return false;
}
#endif