#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h> 

/**
 * @brief 
 * FSR Sensor: GPIO26
 * I2C For Accelerometer: Grove 4
 * 
 */

#define STACK_SIZE  500 
#ifndef SENSOR_CHAN_FORCE
#define SENSOR_CHAN_FORCE SENSOR_CHAN_PRIV_START
#endif

#define LED2_NODE   DT_ALIAS(led2) 
const struct gpio_dt_spec int_pin = GPIO_DT_SPEC_GET(DT_NODELABEL(adxl_313), int1_gpios);
struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios); 

const struct device *const acc_dev = DEVICE_DT_GET(DT_NODELABEL(adxl_313));
const struct device *const fsr_dev = DEVICE_DT_GET(DT_NODELABEL(fsr_sensor));

void motion_handler(const struct device *dev, const struct sensor_trigger *trig)
{
    struct sensor_value accel[3];

    sensor_sample_fetch(dev);

    sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, accel);

    printk("Motion Detected! X: %d.%06d, Y: %d.%06d, Z: %d.%06d\n",
           accel[0].val1, accel[0].val2,
           accel[1].val1, accel[1].val2,
           accel[2].val1, accel[2].val2);
}

void task(struct gpio_dt_spec* led, int delay) 
{
    if(!device_is_ready(acc_dev)) {
        printk("Sensor device not ready\n");
    }
    else{
        printk("Sensor Init complete\n");
    }
    gpio_pin_configure_dt(led, GPIO_OUTPUT_ACTIVE); 
    //struct sensor_value accel[3];
    struct sensor_trigger trig = {
        .type = SENSOR_TRIG_DELTA,    // "Delta" is often used for activity/motion
        .chan = SENSOR_CHAN_ACCEL_XYZ,
    };

    int ret = sensor_trigger_set(acc_dev, &trig, motion_handler);
    if (ret != 0) {
        printk("Failed to set trigger: %d\n", ret);
    }

    if(!device_is_ready(fsr_dev)) {
        printk("Sensor device not ready\n");
    }
    struct sensor_value force;
    for(;;){
        sensor_sample_fetch(fsr_dev);
        sensor_channel_get(fsr_dev, SENSOR_CHAN_FORCE, &force);

        printk("Force [g]: %d\n", force.val1);
        k_sleep(K_MSEC(1500));
    }
}

K_THREAD_DEFINE(blink2, STACK_SIZE, task, &led2, 2000, NULL, 5, 0, 0);