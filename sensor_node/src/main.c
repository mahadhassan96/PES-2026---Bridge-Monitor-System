#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define STACK_SIZE  500 

#define LED2_NODE   DT_ALIAS(led2) 

struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios); 

void task(struct gpio_dt_spec* led, int delay) 
{
    gpio_pin_configure_dt(led, GPIO_OUTPUT_ACTIVE); 

    for(;;)
    {
        gpio_pin_toggle_dt(led); 
        k_msleep(delay); 
    }
}

K_THREAD_DEFINE(blink2, STACK_SIZE, task, &led2, 1500, NULL, 5, 0, 0);