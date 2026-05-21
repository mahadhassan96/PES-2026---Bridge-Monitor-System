#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include "vl53l1x.h"
#include "isr.h"

int main(void)
{
    const struct device *tof =
        DEVICE_DT_GET(DT_NODELABEL(vl53l1x0));

    if (!device_is_ready(tof)) {
        printk("VL53L1X not ready\n");
        return 0;
    }

    printk("VL53L1X ready\n");
    
    if (sensor_interrupt_init() < 0) {
        printk("failed to init ToF interrupt\n");
    }

    if (vl53l1x_start_continuous(tof) < 0) {
        printk("failed to start continuous mode\n");
        return 0;
    }

    printk("continuous mode started\n");

    while (1) {
        int distance = avgSampleReading(tof);

        if (distance < 0) {
            printk("sample fetch failed\n");
        } else {
            printk("Distance: %d mm\n", distance);
        }

        k_sleep(K_MSEC(100));
    }
}