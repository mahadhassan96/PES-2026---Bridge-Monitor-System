#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#define UART_NODE DT_NODELABEL(uart0)

static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

int main(void)
{
    if (!device_is_ready(uart_dev)) {
        printk("UART device not ready\n");
        return -1;
    }

    printk("Sender started on UART0 (GP0/GP1)\n");

    while (1) {
        const char *msg = "hello from sender\n";
        for (int i = 0; msg[i] != '\0'; i++) {
            uart_poll_out(uart_dev, msg[i]);
        }
        printk("Message sent to TX pin\n");
        k_msleep(1000);
    }
}
