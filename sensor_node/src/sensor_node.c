
#include "sensor_node.h"
#include "debug_print.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "vl53l1x.h"
#include "isr.h"
#include <stdint.h>

#define UART_NODE DT_NODELABEL(uart0)

#ifndef SENSOR_CHAN_FORCE
#define SENSOR_CHAN_FORCE SENSOR_CHAN_PRIV_START
#endif

enum adxl313_custom_attr {
    SENSOR_ATTR_ADXL313_RAW_THRESH = SENSOR_ATTR_PRIV_START,
};

static const struct device *const acc_dev = DEVICE_DT_GET(DT_NODELABEL(adxl_313));
static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

static const struct device *const fsr_dev = DEVICE_DT_GET(DT_NODELABEL(fsr_sensor));
static const struct device *tof = DEVICE_DT_GET(DT_NODELABEL(vl53l1x0));

int dynamic_set_variable = 0;

K_MSGQ_DEFINE(packet_queue, sizeof(packet_t *), QUEUE_SIZE, __alignof__(packet_t *));

sensor_node_t sn;

void motion_handler(const struct device *dev, const struct sensor_trigger *trig)
{
    APP_PRINT(SN_TAG, DEBUG_TAG, "Motion detected...Activating emergency protocol!");
    sensor_emergency_isr();
}

void init(sensor_node_t *sn)
{
    // Allow the sensors to configure.
    k_sleep(K_SECONDS(5));

    if (!device_is_ready(acc_dev))
    {
        APP_PRINT(SN_TAG, ERROR_TAG, "Sensor device not ready!");
        return;
    }
    else
    {
        APP_PRINT(SN_TAG, DEBUG_TAG, "Sensor init complete!");
    }

    struct sensor_trigger trig = {
        .type = SENSOR_TRIG_DELTA, 
        .chan = SENSOR_CHAN_ACCEL_XYZ,
    };

    int ret = sensor_trigger_set(acc_dev, &trig, motion_handler);
    
    if (ret != 0) {
        APP_PRINT(SN_TAG, ERROR_TAG, "Failed to set trigger: %d", ret);
        return;
    }

    if (!device_is_ready(uart_dev))
    {
        return;
    }

    if (!device_is_ready(tof)) {
        APP_PRINT(SN_TAG, ERROR_TAG, "VL53L1X not ready!");
        return;
    }

    if (sensor_interrupt_init() < 0) {
        APP_PRINT(SN_TAG, ERROR_TAG, "Failed to init ToF interrupt!");
    }

    if (vl53l1x_start_continuous(tof) < 0) {
        APP_PRINT(SN_TAG, ERROR_TAG, "Failed to start VL53L1X in continuous mode");
        return;
    }
}

static bool in_emergency = false;
static int packet_count = 0;

K_SEM_DEFINE(emergency_sem, 0, 1);

void sensor_emergency_isr(void)
{
    // Only give the semaphore if we aren't already handling an emergency
    if (!in_emergency) {
        k_sem_give(&emergency_sem);
    }
}
void emergency_task()
{
    while (true)
    {
        k_sem_take(&emergency_sem, K_FOREVER);
        
        // Double-check flag to prevent queue spamming
        if (!in_emergency) {
            in_emergency = true;
            packet_t *packet = build_packet(EMERGENCY, NULL, 0);
            if (packet)
            {
                APP_PRINT(SN_TAG, DEBUG_TAG, "Sending emergency packet!");
                k_msgq_put(&packet_queue, &packet, K_FOREVER);
            }
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

        sensor_sample_fetch(acc_dev);
        sensor_channel_get(acc_dev, SENSOR_CHAN_ACCEL_XYZ, accel);

        sensor_sample_fetch(fsr_dev);
        sensor_channel_get(fsr_dev, SENSOR_CHAN_FORCE, &force);

        int32_t distance = avgSampleReading(tof);

        int32_t readings[5] = {
            (int32_t)(accel[0].val1 * 1000 + accel[0].val2 / 1000), /* accel X milli-g */
            (int32_t)(accel[1].val1 * 1000 + accel[1].val2 / 1000), /* accel Y milli-g */
            (int32_t)(accel[2].val1 * 1000 + accel[2].val2 / 1000), /* accel Z milli-g */
            (int32_t)force.val1,                                    /* force mN        */
            distance,                                               /* distance mm     */
        };

        send_response(RESPONSE, (uint8_t *)readings, sizeof(readings));
        break;
    }

    // case CONFIG_DISTANCE:

    // {
    //     dynamic_set_variable = newvaueFromPacket

    // }
    case EMERGENCY_ACK:
    {
        in_emergency = false;
        packet_count = 0;
        send_response(READY, NULL, 0);
        break;
    }

    case CONFIG:
    {
        uint8_t sensitivity = (uint8_t)packet->data[0];
        struct sensor_value raw_sensitivity;
        if(sensitivity == LOW){
            raw_sensitivity.val1 = 0x50;
            sensor_attr_set(acc_dev, SENSOR_CHAN_ACCEL_XYZ, (enum sensor_attribute)SENSOR_ATTR_ADXL313_RAW_THRESH, &raw_sensitivity);
        }
        else if(sensitivity == MEDIUM){
            raw_sensitivity.val1 = 0xF0;
            sensor_attr_set(acc_dev, SENSOR_CHAN_ACCEL_XYZ, (enum sensor_attribute)SENSOR_ATTR_ADXL313_RAW_THRESH, &raw_sensitivity);
        }
        else if(sensitivity == HIGH){
            raw_sensitivity.val1 = 0xFF;
            sensor_attr_set(acc_dev, SENSOR_CHAN_ACCEL_XYZ, (enum sensor_attribute)SENSOR_ATTR_ADXL313_RAW_THRESH, &raw_sensitivity);
        }
    }

    default:
        break;
    }
}

// sensor_reading_t read_sensor_data()
// {
//     sensor_reading_t reading;

//     reading.timestamp = k_uptime_get();
//     reading.type = REQUESTED;
//     reading.dist = 0.0f;
//     reading.force = 0.0f;

//     return reading;
// }

void worker_task()
{
    init(&sn);
    packet_t *packet;

    while (true)
    {
        if (k_msgq_get(&packet_queue, &packet, K_FOREVER) == 0)
        {
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
        APP_PRINT(SN_TAG, ERROR_TAG, "UART not ready!");
        return;
    }

    APP_PRINT(SN_TAG, DEBUG_TAG, "UART read task started");

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
                    }

                    state = 0;
                }
                else if (payload_len > sizeof(payload))
                {
                    APP_PRINT(SN_TAG, ERROR_TAG, "Payload too large: %u bytes", payload_len);
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
                    }

                    state = 0;
                }

                break;
            }

            default:
            {
                APP_PRINT(SN_TAG, ERROR_TAG, "Invalid parser state: (%d). Resetting state machine.", state);
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

#ifndef CONFIG_ZTEST
K_THREAD_DEFINE(uart_read_tid, STACK_SIZE, uart_read_task, NULL, NULL, NULL, READ_PRIO, 0, 0);
K_THREAD_DEFINE(worker_tid, STACK_SIZE, worker_task, NULL, NULL, NULL, WORKER_PRIO, 0, 0);
K_THREAD_DEFINE(emergency_tid, STACK_SIZE, emergency_task, NULL, NULL, NULL, EMERGENCY_PRIO, 0, 0);
#endif