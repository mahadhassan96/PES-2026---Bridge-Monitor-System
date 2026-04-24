// #include "base_station.h"

// #include <zephyr/kernel.h>
// #include <zephyr/sys/printk.h>
// #include <zephyr/drivers/gpio.h>

// #include <stdio.h>
// #include <stdint.h>
// #include <stdlib.h>

// #define NOTICE_DELAY_MS 2000
// #define MS_TO_S 1000
// #define TIME_TARGET 3

// #define STACK_SIZE 500
// #define MAIN_PRIO -1

// // Get macro aliases for each node.
// #define BTN0_NODE DT_ALIAS(button0)

// // Create specs for each node.
// static const struct gpio_dt_spec btn0_spec = GPIO_DT_SPEC_GET(BTN0_NODE, gpios);

// static struct gpio_callback btn_cb_data;

// static uint8_t timing_active = 0;

// /* Creates semaphore with count set to 0 and limit to 1 */
// K_SEM_DEFINE(btn_sem, 0, 1);
// K_MUTEX_DEFINE(active_mutex); 


// // void init(base_station_t* bs)
// // {
// //     // Initialize the base station state
// //     bs->curr_state = BOOT;
// //     bs->anomaly_detected = false;
// //     bs->logging = false;
// //     bs->boot_complete = false;

// //     // Initialize sensors and perform handshake
// //     init_sensors();
// //     sensor_handshake();

// //     // Mark boot as complete
// //     bs->boot_complete = true;
// // }


// void btn_isr(void *arg)
// {
// 	k_sem_give(&btn_sem);
// }

// /* ISR Initialization function */
// void init() {
//     /* Configures LED and button */
//     gpio_pin_configure_dt(&btn0_spec, GPIO_INPUT);

//     /* Registers interrupt on edge to active level */
//     gpio_pin_interrupt_configure_dt(&btn0_spec, GPIO_INT_EDGE_TO_ACTIVE);

//     /* Initializes callback struct with ISR and the pins on which ISR should trigger  */
//     gpio_init_callback(&btn_cb_data, btn_isr, BIT(btn0_spec.pin));

//     /* Adds ISR callback to the device */
//     gpio_add_callback_dt(&btn0_spec, &btn_cb_data);
// }

// /* Toggles LED on button press*/
// void btn_task()
// {
//     init();
// 	int64_t start, end;
//     for (;;) {
//         /* Wait until semaphore is given by ISR (meaning button is pressed), 
//         take semaphore and toggle LED*/
//         k_sem_take(&btn_sem, K_FOREVER);

// 		k_mutex_lock(&active_mutex, K_FOREVER);
// 		if (!timing_active)
// 		{
// 			timing_active = 1;
// 			start = k_uptime_get();
// 			printk("Button pressed, let's begin!\n");
// 		} else {
// 			timing_active = 0;
// 			end = k_uptime_get();
// 			int64_t dt_ms = end - start;
// 			int64_t dt_s  = dt_ms / MS_TO_S;
// 			int64_t dist_s = llabs(TIME_TARGET - dt_s);
// 			printk("Press time: %llds. %llds away from 3s target!\n", dt_s, dist_s);
// 		}
// 		k_mutex_unlock(&active_mutex);
//     }
// }

// K_THREAD_DEFINE(main_tid, STACK_SIZE, btn_task, NULL, NULL, NULL, BTN_PRIO, 0, 0);