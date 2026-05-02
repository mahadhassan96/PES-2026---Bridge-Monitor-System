#include "I2C_interface.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <string.h>

#define SENSOR_I2C_NODE DT_NODELABEL(i2c0)
static const struct device *g_i2c_dev = DEVICE_DT_GET(SENSOR_I2C_NODE);

i2c_status sensor_write_reg(uint8_t device_addr, uint16_t reg_addr, const uint8_t *data, uint16_t length)
{
    uint8_t reg_buf[2 + 4]; // 2 bytes for reg and 3 for data, adjust if needed
    if (!device_is_ready(g_i2c_dev)) {
        return I2C_ERROR_NOT_READY;
    }
    reg_buf[0] = reg_addr >> 8; 
    reg_buf[1] = reg_addr & 0xFF;
    memcpy(&reg_buf[2], data, length);

    return (i2c_write(g_i2c_dev, reg_buf, length + 2, device_addr) == 0)? I2C_OK : I2C_ERROR_BUS;
}

i2c_status sensor_read_reg(uint8_t device_addr, uint16_t reg_addr, uint8_t *data, uint16_t length)
{
    uint8_t reg_buf[2] = { reg_addr >> 8, reg_addr & 0xFF };
    if (!device_is_ready(g_i2c_dev)) {
        return I2C_ERROR_NOT_READY;
    }

    return (i2c_write_read(g_i2c_dev, device_addr, reg_buf, sizeof(reg_buf), data, length) == 0)? I2C_OK : I2C_ERROR_BUS;
}

i2c_status sensor_write_reg_u8(uint8_t device_addr, uint16_t reg_addr, uint8_t data)
{
    return sensor_write_reg(device_addr, reg_addr, &data, 1);
}

i2c_status sensor_write_reg_u16(uint8_t device_addr, uint16_t reg_addr, uint16_t data)
{
    uint8_t buf[2] = { data >> 8, data & 0xFF };

    return sensor_write_reg(device_addr, reg_addr, buf, 2);
}

i2c_status sensor_write_reg_u32(uint8_t device_addr, uint16_t reg_addr, uint32_t data)
{
    uint8_t buf[4] = {
        (data >> 24) & 0xFF,
        (data >> 16) & 0xFF,
        (data >> 8)  & 0xFF,
        data & 0xFF
    };

    return sensor_write_reg(device_addr, reg_addr, buf, 4);
}

i2c_status sensor_read_reg_u8(uint8_t device_addr, uint16_t reg_addr, uint8_t *data)
{
    return sensor_read_reg(device_addr, reg_addr, data, 1);
}

i2c_status sensor_read_reg_u16(uint8_t device_addr, uint16_t reg_addr, uint16_t *data)
{
    uint8_t buf[2];
    i2c_status status = sensor_read_reg(device_addr, reg_addr, (uint8_t*)buf, 2);
    if (status == I2C_OK) { *data = ((uint16_t)buf[0] << 8) | buf[1]; }

    return status;
}

i2c_status sensor_read_reg_u32(uint8_t device_addr, uint16_t reg_addr, uint32_t *data)
{
    uint8_t buf[4];

    i2c_status status = sensor_read_reg(device_addr, reg_addr, buf, 4);

    if (status == I2C_OK)
    {
        *data = ((uint32_t)buf[0] << 24) |
                 ((uint32_t)buf[1] << 16) |
                 ((uint32_t)buf[2] << 8)  |
                 (uint32_t)buf[3];
    }

    return status;
}
