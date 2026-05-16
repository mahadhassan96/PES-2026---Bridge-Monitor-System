
#include "../../include/sensor_node.h"

#define UART_NODE DT_NODELABEL(uart0)

static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

K_MSGQ_DEFINE(packet_queue, sizeof(packet_t *), QUEUE_SIZE, __alignof__(packet_t *));

sensor_node_t sn;

void init(sensor_node_t *sn)
{
    if (!device_is_ready(uart_dev))
    {
        return;
    }
}

void process_packet(packet_t *packet)
{
    switch (packet->type)
    {
        case REQUEST:
        {
            // // Send ACK first
            // packet_t *packet_ack = build_packet(ACK, NULL, 0);
            // if (packet_ack)
            // {
            //     send_packet(uart_dev, packet_ack);
            //     destroy_packet(packet_ack);
            // }

            // Send RESPONSE with 3 static floats
            float readings[3] = {1.23f, 4.56f, 7.89f};
            packet_t *packet_response = build_packet(RESPONSE, (uint8_t *)readings, sizeof(readings));
            if (packet_response)
            {
                print_packet("SENSOR NODE process_packet", packet_response);
                send_packet(uart_dev, packet_response);
                destroy_packet(packet_response);
            }

            break;
        }

        default:
            break;
    }
}

sensor_reading_t read_sensor_data()
{
    sensor_reading_t reading;

    reading.timestamp = k_uptime_get();
    reading.type = REQUESTED;
    reading.dist = 0.0f;
    reading.force = 0.0f;

    return reading;
}

void send_response(packet_type_t type, uint8_t *data, uint8_t data_len)
{
    packet_t *response_packet = build_packet(type, data, data_len);
    if (response_packet)
    {
        send_packet(uart_dev, response_packet);
        destroy_packet(response_packet);
    }
}

void worker_task()
{
    init(&sn);
    packet_t *packet;

    while (true)
    {
        if (k_msgq_get(&packet_queue, &packet, K_FOREVER) == 0)
        {
            print_packet("SENSOR NODE worker_task", packet);
            process_packet(packet);
            destroy_packet(packet);
        }
    }
}

void uart_read_task()
{
    uint8_t received_byte;
    uint8_t packet_type = 0;
    uint8_t payload_len = 0;
    uint8_t payload_index = 0;
    uint8_t payload[64];

    int state = 0;

    if (!device_is_ready(uart_dev))
    {
        printk("[UART] ERROR: UART not ready\n");
        return;
    }

    printk("\n================ UART RX TASK STARTED ================\n");

    while (true)
    {
        if (uart_poll_in(uart_dev, &received_byte) == 0)
        {
            switch (state)
            {
            case 0:
            {
                if (received_byte == SYNC_BYTE)
                {
                    state = 1;
                    payload_index = 0;
                }

                break;
            }

            case 1:
            {
                packet_type = received_byte;
                state = 2;
                break;
            }

            case 2:
            {
                payload_len = received_byte;

                if (payload_len == 0)
                {
                    packet_t *packet = build_packet(packet_type, NULL, 0);

                    if (packet)
                    {
                        k_msgq_put(&packet_queue, &packet, K_FOREVER);
                        printk("[QUEUE] Items waiting: %d\n", k_msgq_num_used_get(&packet_queue));
                        print_packet("SENSOR NODE CASE2 uart_read_task", packet);
                    }

                    state = 0;
                }
                else if (payload_len > sizeof(payload))
                {
                    printk(
                        "\n================ SENSOR NODE ERROR =================\n"
                        "ERROR  : Payload too large\n"
                        "LENGTH : %u\n"
                        "MAX    : %u\n"
                        "===========================================\n",
                        payload_len,
                        sizeof(payload));

                    state = 0;
                }
                else
                {
                    payload_index = 0;
                    state = 3;
                }

                break;
            }

            case 3:
            {
                payload[payload_index++] = received_byte;

                if (payload_index >= payload_len)
                {
                    packet_t *packet = build_packet(packet_type, payload, payload_len);

                    if (packet)
                    {
                        k_msgq_put(&packet_queue, &packet, K_FOREVER);

                        print_packet("SENSOR NODE CASE3 uart_read_task", packet);
                    }

                    state = 0;
                }

                break;
            }

            default:
            {
                printk(
                    "\n================ BASE STATION ERROR =================\n"
                    "ERROR : Invalid parser state (%d)\n"
                    "ACTION: Resetting state machine\n"
                    "===========================================\n",
                    state);

                state = 0;
                break;
            }
            }
        }
        else
        {
            k_sleep(K_MSEC(1)); // nothing received, give worker thread time to launch
        }

        k_yield();
    }
}

K_THREAD_DEFINE(uart_read_tid, STACK_SIZE, uart_read_task, NULL, NULL, NULL, READ_PRIO, 0, 0);

K_THREAD_DEFINE(worker_tid, STACK_SIZE, worker_task, NULL, NULL, NULL, WORKER_PRIO, 0, 0);