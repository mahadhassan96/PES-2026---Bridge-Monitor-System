#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#define UART_NODE DT_NODELABEL(uart0)

static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

int main(void)
{
    unsigned char recv_char;

    if (!device_is_ready(uart_dev)) {
        printk("UART device not ready\n");
        return -1;
    }

    printk("Receiver ready on UART0 (GP0/GP1). Waiting for data...\n");

    while (1) {
        if (uart_poll_in(uart_dev, &recv_char) == 0) {
            printk("%c", recv_char);
        }
        // Small sleep to prevent watchdog issues in tight loops
        k_yield();
    }
}
