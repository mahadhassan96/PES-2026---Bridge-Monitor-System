#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define DELAY_2_MS    100
#define DELAY_3_MS    200
#define DELAY_4_MS    300
#define DELAY_5_MS    500

#define STACK_SIZE  500 

#define LED2_NODE   DT_ALIAS(led2) 
#define LED3_NODE   DT_ALIAS(led3) 
#define LED4_NODE   DT_ALIAS(led4) 
#define LED5_NODE   DT_ALIAS(led5) 

struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios); 
struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios); 
struct gpio_dt_spec led4 = GPIO_DT_SPEC_GET(LED4_NODE, gpios); 
struct gpio_dt_spec led5 = GPIO_DT_SPEC_GET(LED5_NODE, gpios); 

void task(struct gpio_dt_spec* led, int delay) 
{
    gpio_pin_configure_dt(led, GPIO_OUTPUT_ACTIVE); 

    for(;;)
    {
        gpio_pin_toggle_dt(led); 
        k_msleep(delay); 
    }
}

K_THREAD_DEFINE(blink2, STACK_SIZE, task, &led2, DELAY_2_MS, NULL, 5, 0, 0);
K_THREAD_DEFINE(blink3, STACK_SIZE, task, &led3, DELAY_3_MS, NULL, 6, 0, 0);
K_THREAD_DEFINE(blink4, STACK_SIZE, task, &led4, DELAY_4_MS, NULL, 7, 0, 0);
K_THREAD_DEFINE(blink5, STACK_SIZE, task, &led5, DELAY_5_MS, NULL, 8, 0, 0);