#ifndef I2C_INTERFACE_H
#define I2C_INTERFACE_H   
#include <stdint.h>
#include <stddef.h>
#include <zephyr/drivers/i2c.h>

typedef enum
{
    I2C_OK,
    I2C_ERROR_BUS,
    I2C_ERROR_NOT_READY
} i2c_status;

i2c_status sensor_write_reg(uint8_t device_addr, uint8_t reg_addr, uint8_t *data, uint16_t length);
i2c_status sensor_read_reg(uint8_t device_addr, uint8_t reg_addr, uint8_t *data, uint16_t length);
#endif