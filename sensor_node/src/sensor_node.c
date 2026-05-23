
#include "../../include/sensor_node.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "vl53l1x.h"
#include "isr.h"

#define UART_NODE DT_NODELABEL(uart0)

#ifndef SENSOR_CHAN_FORCE
#define SENSOR_CHAN_FORCE SENSOR_CHAN_PRIV_START
#endif

static const struct device *const acc_dev = DEVICE_DT_GET(DT_NODELABEL(adxl_313));
static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

static const struct device *const fsr_dev = DEVICE_DT_GET(DT_NODELABEL(fsr_sensor));
static const struct device *tof = DEVICE_DT_GET(DT_NODELABEL(vl53l1x0));

K_MSGQ_DEFINE(packet_queue, sizeof(packet_t *), QUEUE_SIZE, __alignof__(packet_t *));

sensor_node_t sn;

void motion_handler(const struct device *dev, const struct sensor_trigger *trig)
{
    struct sensor_value accel[3];

    sensor_sample_fetch(acc_dev);

    sensor_channel_get(acc_dev, SENSOR_CHAN_ACCEL_XYZ, accel);

    printk("Motion Detected! X: %d.%06d, Y: %d.%06d, Z: %d.%06d\n",
           accel[0].val1, accel[0].val2,
           accel[1].val1, accel[1].val2,
           accel[2].val1, accel[2].val2);
}

void init(sensor_node_t *sn)
{
    k_msleep(10000);
	
    if(!device_is_ready(acc_dev)) {
        printk("Sensor device not ready\n");
        return;
    }
    else{
        printk("Sensor Init complete\n");
    }
    
    struct sensor_trigger trig = {
        .type = SENSOR_TRIG_DELTA,    // "Delta" is often used for activity/motion
        .chan = SENSOR_CHAN_ACCEL_XYZ,
    };
    
    int ret = sensor_trigger_set(acc_dev, &trig, motion_handler);
    
    if (ret != 0) {
        printk("Failed to set trigger: %d\n", ret);
        return;
    }

    if (!device_is_ready(uart_dev))
    {
        return;
    }

    if (!device_is_ready(tof)) {
        printk("VL53L1X not ready\n");
        return;
    }

    if (sensor_interrupt_init() < 0) {
        printk("failed to init ToF interrupt\n");
    }

    if (vl53l1x_start_continuous(tof) < 0) {
        printk("failed to start continuous mode\n");
        return;
    }
}

static bool in_emergency = false;
static int packet_count = 0;

K_SEM_DEFINE(emergency_sem, 0, 1);

void sensor_emergency_isr(void)
{
    k_sem_give(&emergency_sem);
}

void emergency_task()
{
    while (true)
    {
        k_sem_take(&emergency_sem, K_FOREVER);
        in_emergency = true;
        packet_t *packet = build_packet(EMERGENCY, NULL, 0);
        if (packet)
        {
            k_msgq_put(&packet_queue, &packet, K_FOREVER);
        }
    }
}


void process_packet(packet_t *packet)
{
    if (in_emergency && packet->type != EMERGENCY_ACK)
    {
        send_response(EMERGENCY, NULL, 0);
        return;
    }

    switch (packet->type)
    {
		/*Request of sensor data*/
        case REQUEST:
        {

			struct sensor_value accel[3];
   			struct sensor_value force;

			// 1- Get the XYZ values
			sensor_sample_fetch(acc_dev);
			sensor_channel_get(acc_dev, SENSOR_CHAN_ACCEL_XYZ, accel);

			//2- GET FSR
			sensor_sample_fetch(fsr_dev);
        	sensor_channel_get(fsr_dev, SENSOR_CHAN_FORCE, &force);

            int readings[5] = {accel[0].val1, accel[1].val1, accel[2].val1, force.val1, 88};
            send_response(RESPONSE, (uint8_t *)readings, sizeof(readings));
            
            int distance = avgSampleReading(tof);

            if (distance < 0) {
                printk("sample fetch failed\n");
            } else {
                printk("Distance: %d mm\n", distance);
            }
            
            break;
        }

        case EMERGENCY_ACK:
        {
            in_emergency = false;
            packet_count = 0;
            send_response(SYN, NULL, 0);
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

K_THREAD_DEFINE(emergency_tid, STACK_SIZE, emergency_task, NULL, NULL, NULL, EMERGENCY_PRIO, 0, 0);