#include "FT3168.h"
#include "esp_err.h"
#include "lcd_config.h"
#include <string.h>  // For memcpy

#define TEST_I2C_PORT I2C_NUM_0

// Maximum I2C write payload size (register writes are typically small)
// If larger writes are needed, increase this value
#define I2C_WRITE_MAX_LEN 32

uint8_t I2C_writr_buff(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len)
{
  uint8_t ret;
  
  // Use stack buffer instead of heap allocation (avoids malloc/free overhead and fragmentation)
  // Buffer size: 1 byte for register + max payload
  uint8_t stack_buf[I2C_WRITE_MAX_LEN + 1];
  
  // Safety check: truncate if too large (should never happen in normal use)
  if (len > I2C_WRITE_MAX_LEN) {
    len = I2C_WRITE_MAX_LEN;
  }
  
  // Build I2C message: [register, data0, data1, ...]
  stack_buf[0] = reg;
  if (len > 0 && buf != NULL) {
    memcpy(&stack_buf[1], buf, len);
  }
  
  ret = i2c_master_write_to_device(TEST_I2C_PORT, addr, stack_buf, len + 1, 1000);
  return ret;
}
uint8_t I2C_read_buff(uint8_t addr,uint8_t reg,uint8_t *buf,uint8_t len)
{
  uint8_t ret;
  ret = i2c_master_write_read_device(TEST_I2C_PORT,addr,&reg,1,buf,len,1000);
  return ret;
}
// REMOVED: I2C_master_write_read_device() - was never used in codebase
// If needed in future, use i2c_master_write_read_device() directly

void Touch_Init(void)
{
  i2c_config_t conf = 
  {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = EXAMPLE_PIN_NUM_TOUCH_SDA,         // Configure the GPIO of the SDA
    .scl_io_num = EXAMPLE_PIN_NUM_TOUCH_SCL,         // Configure GPIO for SCL
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master = {.clk_speed = 300 * 1000,},  // Select a frequency for the project
    .clk_flags = 0,          // Optionally, use the I2C SCLK SRC FLAG * flag to select the I2C source clock
  };
  ESP_ERROR_CHECK(i2c_param_config(TEST_I2C_PORT, &conf));
  ESP_ERROR_CHECK(i2c_driver_install(TEST_I2C_PORT, conf.mode,0,0,0));

  uint8_t data = 0x00;
  I2C_writr_buff(I2C_ADDR_FT3168,0x00,&data,1); //Switch to normal mode

}
uint8_t getTouch(uint16_t *x,uint16_t *y)
{
  uint8_t data;
  uint8_t buf[4];
  I2C_read_buff(I2C_ADDR_FT3168,0x02,&data,1);
  if(data)
  {
    I2C_read_buff(I2C_ADDR_FT3168,0x03,buf,4);
    *x = (((uint16_t)buf[0] & 0x0f)<<8) | (uint16_t)buf[1];
    *y = (((uint16_t)buf[2] & 0x0f)<<8) | (uint16_t)buf[3];
    if(*x > EXAMPLE_LCD_H_RES)
    *x = EXAMPLE_LCD_H_RES;
    if(*y > EXAMPLE_LCD_V_RES)
    *y = EXAMPLE_LCD_V_RES;
    return 1;
  }
  return 0;
}


