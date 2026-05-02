#include "include/tof.h"

int main(void)
{
    if (!tof_init(false)) {
        printk("Failed to initialize ToF sensor\n");
        return -1;
    }
    tof_start_continuous(50);
    uint16_t sampleddistance = 0;
    while (1) {
        uint16_t distance = tof_read(true);
        if (!did_timeout && distance > 0) {
            printk("Valid distance reading: %d mm\n", distance);
        } else {
            // Handle timeout or invalid reading (e.g., log warning, attempt recovery, etc.)
            printk("Invalid distance reading\n");
        }

        // if (avgSampleReading(&sampleddistance, 3000);)
        // {
        //     printk("Average distance over 3 seconds: %d mm\n", sampleddistance);
        // } else {
        //     printk("Failed to get average distance reading\n");
        // }
    }

    return 0;
}