#include "tof.h"
#include "isr.h"
#include "thresholds.h"

int main(void)
{
    if (!tof_init(false)) {
        printk("Failed to initialize ToF sensor\n");
        return -1;
    }

    if (!tof_set_distance_threshold_interrupt(thresholds.min_distance_mm)) {
        printk("Failed to set threshold interrupt\n");
    }

    sensor_interrupt_init();
    tof_start_continuous(80);
    uint16_t sampleddistance = 0;
    while (1) {
        // uint16_t distance = tof_read();
        // if (!did_timeout && distance > 0) {
        //     printk("Valid distance reading: %d mm\n", distance);
        // } else {
        //     // Handle timeout or invalid reading (e.g., log warning, attempt recovery, etc.)
        //     printk("Invalid distance reading: %d mm, did_timeout: %d\n", distance, did_timeout);
        // }

        if (avgSampleReading(&sampleddistance, 3000))
        {
            printk("Average distance over 3 seconds: %d mm\n", sampleddistance);
        } else {
            printk("Failed to get average distance reading\n");
        }
    }

    return 0;
}