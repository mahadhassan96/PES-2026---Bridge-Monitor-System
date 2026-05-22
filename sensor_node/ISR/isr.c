#include "isr.h"

#define TOF_NODE DT_NODELABEL(vl53l1x0)

static const struct device *tof_dev = DEVICE_DT_GET(TOF_NODE);

static struct k_work tof_work;
static struct gpio_callback tof_cb;

static void tof_gpio_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_work_submit(&tof_work);
}

static void tof_work_handler(struct k_work *work)
{
    uint16_t distance = vl53l1x_read(tof_dev);
    printk("interrupt mode detected: %u mm\n", distance);
}

int sensor_interrupt_init(void)
{
    int ret;

    if (!device_is_ready(tof_dev)) {
        return -ENODEV;
    }

    const struct vl53l1x_config *cfg = tof_dev->config;
    const struct gpio_dt_spec *int_gpio = &cfg->int_gpio;

    if (!gpio_is_ready_dt(int_gpio)) {
        return -ENODEV;
    }

    k_work_init(&tof_work, tof_work_handler);

    ret = gpio_pin_configure_dt(int_gpio, GPIO_INPUT | GPIO_PULL_UP);
    if (ret < 0) {
        return ret;
    }

    gpio_init_callback(&tof_cb, tof_gpio_isr, BIT(int_gpio->pin));

    ret = gpio_add_callback(int_gpio->port, &tof_cb);
    if (ret < 0) {
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(int_gpio, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        return ret;
    }

    return 0;
}