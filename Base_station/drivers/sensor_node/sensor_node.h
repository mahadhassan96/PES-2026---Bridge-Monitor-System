#ifndef SENSOR_NODE_H
#define SENSOR_NODE_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>

/* Custom channel for force sensor since Zephyr doesn't define one */
#define SENSOR_CHAN_FORCE_N (SENSOR_CHAN_PRIV_START + 0)

/* Data stored by fetch, read by get */
typedef struct {
    int dist;
    int force;
    int accel_x;
    int accel_y;
    int accel_z;
    int64_t last_fetch_time;
} sensor_node_data_t;

/* Called by packet_handler_task when RESPONSE arrives */
void sensor_node_store_response(int dist, int force, int accel_x, int accel_y, int accel_z);

#endif