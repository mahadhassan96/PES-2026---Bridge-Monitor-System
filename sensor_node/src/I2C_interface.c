#include "I2C_interface.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <string.h>

#define SENSOR_I2C_NODE DT_NODELABEL(i2c0)
static const struct device *g_i2c_dev = DEVICE_DT_GET(SENSOR_I2C_NODE);

i2c_status sensor_write_reg(uint8_t device_addr, uint8_t reg_addr, uint8_t *data, uint16_t length)
{
    uint8_t tx_buf[4]; // 1 byte for reg 3 for data, adjust if needed
    if (!device_is_ready(g_i2c_dev)) {
        return I2C_ERROR_NOT_READY;
    }
    tx_buf[0] = reg_addr;
    memcpy(&tx_buf[1], data, length);

    return (i2c_write(g_i2c_dev, tx_buf, length, device_addr) == 0)? I2C_OK : I2C_ERROR_BUS;
}

i2c_status sensor_read_reg(uint8_t device_addr, uint8_t reg_addr, uint8_t *data, uint16_t length)
{
    if (!device_is_ready(g_i2c_dev)) {
        return I2C_ERROR_NOT_READY;
    }

    return (i2c_write_read(g_i2c_dev, device_addr, &reg_addr, sizeof(reg_addr), data, length) == 0)? I2C_OK : I2C_ERROR_BUS;
}