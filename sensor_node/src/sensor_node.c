#include "../../include/sensor_node.h"

#define UART_NODE DT_NODELABEL(uart0)

static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

// Create a packet queue.
K_MSGQ_DEFINE(packet_queue, sizeof(packet_t *), QUEUE_SIZE, __alignof__(packet_t *));

sensor_node_t sn;

void init(sensor_node_t* sn)
{
    // Initialize UART device.
    if (!device_is_ready(uart_dev)) {
        return;
    }
}

void process_packet(packet_t *packet)
{
    printk("[DEBUG] Processing packet!\n");
    // Process the received packet based on its type.
    switch (packet->type) {
        case REQUEST:
            // printk("[DEBUG] Received REQUEST packet.\n");
            // Handle data request from the base station.
            // For example, read sensor data and send a response packet.
            // sensor_reading_t reading = read_sensor_data();
            // send_response(RESPONSE, (uint8_t *)&reading, sizeof(reading));
            printk("[DEBUG] Sending ACK packet!\n");
            send_response(ACK, NULL, 0);
            
            break;
        default:
            // Handle other packet types as needed.
            break;
    }
}

sensor_reading_t read_sensor_data()
{
    sensor_reading_t reading;
    reading.timestamp = k_uptime_get(); // Placeholder timestamp.
    reading.type = REQUESTED; 
    reading.dist = 0.0f;
    reading.force = 0.0f;
    return reading; // Return the simulated sensor reading.
}

void send_response(packet_type_t type, uint8_t *data, uint8_t data_len)
{
    // Build a response packet.
    packet_t *response_packet = build_packet(type, data, data_len);
    if (response_packet) {
        // Send the response packet over UART.
        send_packet(uart_dev, response_packet);

        // Free the allocated memory for the response packet.
        destroy_packet(response_packet);
    }
}

void worker_task()
{
    init(&sn);
    packet_t *packet;
    while (true)
    {
        if (k_msgq_get(&packet_queue, &packet, K_FOREVER) == 0) {
            // Process the received packet.
            process_packet(packet);
            destroy_packet(packet);
        }
    }
}

void uart_read_task()
{
    uint8_t payload_len;

    if (!device_is_ready(uart_dev)) {
        printk("UART not ready\n");
        return;
    }

    while (true)
    {
        // Read packets from the UART channel and handle them.
        if (uart_poll_in(uart_dev, &payload_len) == 0)
        {
            printk("[DEBUG] Received packet of payload length: %d!\n", payload_len);

            // Read the rest of the packet (type and data).
            packet_t *packet = receive_packet(uart_dev, payload_len);
            if(packet)
            {
                // Pass the received packet to the worker task.
                k_msgq_put(&packet_queue, &packet, K_FOREVER);
            }
            
        }
        // Small sleep to prevent busy waiting.
        k_sleep(K_MSEC(1));
    }
}

K_THREAD_DEFINE(uart_read_tid, STACK_SIZE, uart_read_task, NULL, NULL, NULL, READ_PRIO, 0, 0);
K_THREAD_DEFINE(worker_tid, STACK_SIZE, worker_task, NULL, NULL, NULL, WORKER_PRIO, 0, 0);