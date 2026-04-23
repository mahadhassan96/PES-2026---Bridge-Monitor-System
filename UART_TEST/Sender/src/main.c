#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#define STACK_SIZE 500
#define UART_NODE  DT_NODELABEL(uart0)
#define BTN_NODE   DT_ALIAS(btn)

/* Semaphore: ISR signals button_task */
K_SEM_DEFINE(btn_sem, 0, 1);

static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);
static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(BTN_NODE, gpios);

static struct gpio_callback btn_cb_data;

static void button_isr(const struct device *dev,
                       struct gpio_callback *cb,
                       uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    /* k_sem_give is ISR-safe */
    k_sem_give(&btn_sem);
}

/* Helper: send a string over UART */
static void uart_send(const char *msg)
{
    for (int i = 0; msg[i] != '\0'; i++) {
        uart_poll_out(uart_dev, msg[i]);
    }
}

/* wakes on semaphore, sends a message */
void button_task(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (;;) {
        k_sem_take(&btn_sem, K_FOREVER);   /* blocks until ISR fires */
        uart_send("button pressed!\n");
        printk("Button event sent\n");
    }
}

K_THREAD_DEFINE(button_tid, STACK_SIZE,
                button_task, NULL, NULL, NULL,
                1, 0, 0);

/* configure GPIO interrupt, then keep sending*/
int main(void)
{
    /* Configures button */
    gpio_pin_configure_dt(&btn, GPIO_INPUT);

    /* Registers interrupt on edge to active level */
    gpio_pin_interrupt_configure_dt(&btn, GPIO_INT_EDGE_TO_ACTIVE);

    /* Initializes callback struct with ISR and the pins on which ISR should trigger  */
    gpio_init_callback(&btn_cb_data, button_isr, BIT(btn.pin));

    /* Adds ISR callback to the device */
    gpio_add_callback_dt(&btn, &btn_cb_data);

    printk("Sender started on UART0\n");

    while (1) {
        uart_send("hello from sender\n");
        printk("Message sent\n");
        k_msleep(1000);
    }
}