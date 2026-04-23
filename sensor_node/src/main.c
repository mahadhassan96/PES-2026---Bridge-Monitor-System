#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include "fsr.h"

#define DELAY_2_MS  100
#define STACK_SIZE  500 

void main_task() 
{
  int32_t force = 0; 
	config_adc();
    for(;;)
    {
		
		get_force(&force);
    printk("Voltage [mV]: %d\n", force);
			
		k_sleep(K_MSEC(1500));
    }
}

K_THREAD_DEFINE(main_tsk, STACK_SIZE, main_task, NULL, NULL, NULL, -1, 0, 0);