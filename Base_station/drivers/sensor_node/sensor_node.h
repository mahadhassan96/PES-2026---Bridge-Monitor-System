#ifndef SENSOR_NODE_H
#define SENSOR_NODE_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>

/* Custom channel for force sensor since Zephyr doesn't define one */
#define SENSOR_CHAN_FORCE_N (SENSOR_CHAN_PRIV_START + 0)

/* Data stored by fetch, read by get */
typedef struct {
    float dist;
    float force;
    int64_t last_fetch_time;   /* 0 means fetch never called */
} sensor_node_data_t;

#endif