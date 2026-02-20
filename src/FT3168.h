#ifndef FT3168_H
#define FT3168_H
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif 

void Touch_Init(void);

uint8_t getTouch(uint16_t *x,uint16_t *y);

// I2C functions used by QMI8658 IMU driver
uint8_t I2C_writr_buff(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len);
uint8_t I2C_read_buff(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif
#endif