#ifndef LCD_CONFIG_H
#define LCD_CONFIG_H

#define EXAMPLE_LCD_H_RES              280
#define EXAMPLE_LCD_V_RES              456

#define LCD_BIT_PER_PIXEL              16

#define EXAMPLE_PIN_NUM_LCD_CS            9
#define EXAMPLE_PIN_NUM_LCD_PCLK          10 
#define EXAMPLE_PIN_NUM_LCD_DATA0         11
#define EXAMPLE_PIN_NUM_LCD_DATA1         12
#define EXAMPLE_PIN_NUM_LCD_DATA2         13
#define EXAMPLE_PIN_NUM_LCD_DATA3         14
#define EXAMPLE_PIN_NUM_LCD_RST           21
#define EXAMPLE_PIN_NUM_BK_LIGHT          (-1)

#define EXAMPLE_LVGL_BUF_HEIGHT        (EXAMPLE_LCD_V_RES / 8)   /* 57 lines, ~64 KB total */

// LVGL Timing Configuration (optimized for CPU efficiency)
// Tick period: 5ms = 200Hz (sufficient for smooth animations, reduces CPU overhead)
// Previously 2ms (500Hz) which caused unnecessary timer interrupts
#define EXAMPLE_LVGL_TICK_PERIOD_MS    5                          // Timer tick interval
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 200                        // Max delay when idle
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 5                          // Min delay (match tick period)
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (4 * 1024)                 // Task stack size
#define EXAMPLE_LVGL_TASK_PRIORITY     2                          // Task priority

#define I2C_ADDR_FT3168 0x38
#define EXAMPLE_PIN_NUM_TOUCH_SCL 48
#define EXAMPLE_PIN_NUM_TOUCH_SDA 47

#endif