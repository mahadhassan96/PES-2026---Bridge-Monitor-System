#ifndef SENSOR_NODE_H
#define SENSOR_NODE_H

#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#include "readings.h"
#include "coms.h"

#define READ_PRIO    -1
#define WORKER_PRIO  1
#define EMERGENCY_PRIO -2


#define STACK_SIZE 500
#define QUEUE_SIZE 16

// typedef enum
// {
//     BOOT,
//     NORMAL,
//     TRANSMIT,
//     ALERT,
//     ERROR
// } sensor_node_state_t;

// typedef struct 
// {
//     // sensor_node_state_t curr_state;
//     bool anomaly_detected;
//     bool boot_complete;
// } sensor_node_t;

void init(sensor_node_t* sn);
void update(sensor_node_t* sn);

void process_packet(packet_t *packet);

sensor_reading_t read_sensor_data();

void uart_read_task();
void worker_task();

void send_response(packet_type_t type, uint8_t *data, uint8_t data_len);

void sensor_emergency_isr(void);

#endif /* SENSOR_NODE_H */