#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include "include/isr.h"
#include "include/tof.h"
#include <errno.h>

#define TOF_INT_NODE DT_NODELABEL(tof_intrpt)
static const struct gpio_dt_spec tof_intrpt = GPIO_DT_SPEC_GET(TOF_INT_NODE, gpios);

static struct k_work tof_work;
static void tof_gpio_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_work_submit(&tof_work);
}

static void tof_work_handler(struct k_work *work)
{
    uint16_t distance = tof_read(false);
    if (Reading_data.range_status == VL53L1_RANGESTATUS_RANGE_VALID) {
        printk("Detected: %u mm\n", distance);
    }
}

static struct gpio_callback tof_cb;
int sensor_interrupt_init(void)
{
    if (!gpio_is_ready_dt(&tof_intrpt)) { return -ENODEV; }

    gpio_pin_configure_dt(&tof_intrpt, GPIO_INPUT | GPIO_PULL_UP);

    k_work_init(&tof_work, tof_work_handler);
    gpio_pin_interrupt_configure_dt(&tof_intrpt, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&tof_cb, tof_gpio_isr, BIT(tof_intrpt.pin));
    gpio_add_callback(tof_intrpt.port, &tof_cb);

    return 0;
}