
#define DT_DRV_COMPAT adxl_313
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <I2C_interface.h>

#define ADDRESS          0x1D
#define DEVID_0          0x00
#define DATA_FORMAT      0x31
#define INT_ENABLE       0x2E
#define POWER_CTL        0x2D
#define RESET            0x18
#define INT_SOURCE       0x30
#define THRESH_ACT       0x24
#define XYZ_DATA         0x32
#define FIFO_CTL         0x38
#define ACT_INACT_CTL    0x27
#define SCALE_FACTOR     0.009577

struct adxl_313_data {
    const struct device *dev;
    int16_t x;
    int16_t y;
    int16_t z;

    struct gpio_callback gpio_cb;
    struct k_work work;          // Work queue to handle data outside of ISR
    sensor_trigger_handler_t data_ready_handler;
    const struct sensor_trigger *data_ready_trig;
    
};

struct adxl_313_config {
    struct i2c_dt_spec i2c;
    struct gpio_dt_spec int_gpio;
};

static int adxl313_trigger_set(const struct device *dev,
                               const struct sensor_trigger *trig,
                               sensor_trigger_handler_t handler)
{
    struct adxl_313_data *data = dev->data;
    const struct adxl_313_config *config = dev->config;

    if (config->int_gpio.port == NULL) {
        return -EIO;
    }
    
     if (trig->type != SENSOR_TRIG_DELTA && trig->type != SENSOR_TRIG_DATA_READY) {
        return -ENOTSUP;
    }

   gpio_pin_interrupt_configure_dt(&config->int_gpio, GPIO_INT_DISABLE);

     data->data_ready_handler = handler;
    data->data_ready_trig = trig;

    if (handler != NULL) {
        gpio_pin_interrupt_configure_dt(&config->int_gpio, GPIO_INT_EDGE_TO_ACTIVE);
    }

    return 0;
}

static void adxl_313_work_handler(struct k_work *work) {
    struct adxl_313_data *data = CONTAINER_OF(work, struct adxl_313_data, work);
    uint8_t status;
    
    if (sensor_read_reg_adxl313(ADDRESS, INT_SOURCE, &status,1) != 0) {
        return;
    }

    if ((status & 0x10) && data->data_ready_handler) {
        data->data_ready_handler(data->dev, data->data_ready_trig);
        /* Clear interrupt by reading INT_SOURCE again */
        sensor_read_reg_adxl313(ADDRESS, INT_SOURCE, &status, 1);
    }
}

static void adxl_313_gpio_callback(const struct device *port,
                                  struct gpio_callback *cb, uint32_t pins) {
    //printk("GPIO CALLBACK FIRED\n");
    struct adxl_313_data *data = CONTAINER_OF(cb, struct adxl_313_data, gpio_cb);
    k_work_submit(&data->work); 
}

static int adxl_313_sample_fetch(const struct device *dev, enum sensor_channel chan){
    struct adxl_313_data *data = dev->data;
    uint8_t buffer[6];

    int ret = sensor_read_reg_adxl313(ADDRESS, XYZ_DATA, buffer, 6);
    data->x = buffer[0] | (buffer[1] << 8);
    data->y = buffer[2] | (buffer[3] << 8);
    data->z = buffer[4] | (buffer[5] << 8);
    return ret;
}

static int adxl_313_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val){
    struct adxl_313_data *data = dev->data;
    switch (chan) {
    case SENSOR_CHAN_ACCEL_X:
        // Convert raw integer to m/s^2 or Gs
        // Example: data->sample_x * scale
        sensor_value_from_double(val, (double)data->x * SCALE_FACTOR);
        break;
    case SENSOR_CHAN_ACCEL_Y:
        sensor_value_from_double(val, (double)data->y * SCALE_FACTOR);
        break;
    case SENSOR_CHAN_ACCEL_Z:
        sensor_value_from_double(val, (double)data->z * SCALE_FACTOR);
        break;
    case SENSOR_CHAN_ACCEL_XYZ:
        sensor_value_from_double(&val[0], (double)data->x * SCALE_FACTOR);
        sensor_value_from_double(&val[1], (double)data->y * SCALE_FACTOR);
        sensor_value_from_double(&val[2], (double)data->z * SCALE_FACTOR);
        break;
    default:
        return -ENOTSUP;
    }
    return 0;
}

static const struct sensor_driver_api adxl_313_api = {
    .sample_fetch = adxl_313_sample_fetch,
    .channel_get = adxl_313_channel_get,
    .trigger_set = adxl313_trigger_set
};

static int adxl_313_init(const struct device *dev){
    uint8_t dev_id[2];
    printk("Init of ADXL313\n");

    struct adxl_313_data *data = dev->data;
    data->dev = dev;
    const struct adxl_313_config *config = dev->config;

    if (!gpio_is_ready_dt(&config->int_gpio)) {
        printk("Interrupt GPIO device not ready\n");
        return -ENODEV;
    }

    gpio_pin_configure_dt(&config->int_gpio, GPIO_INPUT);
    gpio_init_callback(&data->gpio_cb, adxl_313_gpio_callback, BIT(config->int_gpio.pin));
    gpio_add_callback(config->int_gpio.port, &data->gpio_cb);
    
    k_work_init(&data->work, adxl_313_work_handler);

    if(sensor_read_reg_adxl313(ADDRESS, DEVID_0, dev_id, 2) != I2C_OK){
        return I2C_ERROR_NOT_READY;
    }

    if(dev_id[0] != 0xAD || dev_id[1] != 0x1D){    return I2C_ERROR_BUS;}
 
    uint8_t data_format = 0xB;
    uint8_t power_ctl = 0x08;
    uint8_t int_enable = 0x10;
    uint8_t thresh_act = 0x50;
    uint8_t fifo_ctl = 0;
    uint8_t buffer[6];
    uint8_t status;
    uint8_t act_inact_ctl = 0x70;

    if(sensor_write_reg_adxl313(ADDRESS, DATA_FORMAT, &data_format,1) != I2C_OK){  return I2C_ERROR_BUS;}
    if(sensor_write_reg_adxl313(ADDRESS, FIFO_CTL, &fifo_ctl,1) != I2C_OK){  return I2C_ERROR_BUS;}
    if(sensor_write_reg_adxl313(ADDRESS, THRESH_ACT, &thresh_act,1) != I2C_OK){  return I2C_ERROR_BUS;}
    if(sensor_write_reg_adxl313(ADDRESS, ACT_INACT_CTL, &act_inact_ctl,1) != I2C_OK){  return I2C_ERROR_BUS;}
    if(sensor_write_reg_adxl313(ADDRESS, INT_ENABLE, &int_enable,1) != I2C_OK){  return I2C_ERROR_BUS;}
    
    sensor_read_reg_adxl313(ADDRESS, XYZ_DATA, buffer, 6);
    k_msleep(2);
    if(sensor_write_reg_adxl313(ADDRESS, POWER_CTL, &power_ctl,1) != I2C_OK){  return I2C_ERROR_BUS;}

    k_msleep(20);

    sensor_read_reg_adxl313(ADDRESS, INT_ENABLE, &status,1);
    //printk("int enable: %d\n", status);
    sensor_read_reg_adxl313(ADDRESS, POWER_CTL, &status,1);
    //printk("POWER CTL: %d\n", status);
    sensor_read_reg_adxl313(ADDRESS, INT_SOURCE, &status,1);

    k_msleep(100);

    gpio_pin_interrupt_configure_dt(&config->int_gpio, GPIO_INT_EDGE_TO_ACTIVE);
    return 0;
}

#define adxl_313_INST(inst)                                            \
    static struct adxl_313_data adxl_313_data_##inst;                  \
                                                                       \
    static const struct adxl_313_config adxl_313_config_##inst = {     \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                             \
        .int_gpio = GPIO_DT_SPEC_INST_GET(inst, int1_gpios),           \
    };                                                                 \
                                                                       \
    DEVICE_DT_INST_DEFINE(inst,                                        \
                          adxl_313_init,                               \
                          NULL,                                        \
                          &adxl_313_data_##inst,                       \
                          &adxl_313_config_##inst,                     \
                          POST_KERNEL,                                 \
                          CONFIG_SENSOR_INIT_PRIORITY,                 \
                          &adxl_313_api); 

DT_INST_FOREACH_STATUS_OKAY(adxl_313_INST)