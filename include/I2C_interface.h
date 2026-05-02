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

i2c_status sensor_write_reg(uint8_t device_addr, uint16_t reg_addr, const uint8_t *data, uint16_t length);
i2c_status sensor_read_reg(uint8_t device_addr, uint16_t reg_addr, uint8_t *data, uint16_t length);
i2c_status sensor_write_reg_u8(uint8_t device_addr, uint16_t reg_addr, uint8_t data);
i2c_status sensor_write_reg_u16(uint8_t device_addr, uint16_t reg_addr, uint16_t data);
i2c_status sensor_write_reg_u32(uint8_t device_addr, uint16_t reg_addr, uint32_t data);
i2c_status sensor_read_reg_u8(uint8_t device_addr, uint16_t reg_addr, uint8_t *data);
i2c_status sensor_read_reg_u16(uint8_t device_addr, uint16_t reg_addr, uint16_t *data);
i2c_status sensor_read_reg_u32(uint8_t device_addr, uint16_t reg_addr, uint32_t *data);

#endif