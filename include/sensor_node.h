#ifndef SENSOR_NODE_H
#define SENSOR_NODE_H

#include <stdbool.h>
#include "readings.h"

typedef enum
{
    BOOT,
    NORMAL,
    TRANSMIT,
    ALERT,
    ERROR
} sensor_node_state_t;

typedef struct 
{
    sensor_node_state_t curr_state;
    bool anomaly_detected;
    bool boot_complete;
} sensor_node_t;

void init(sensor_node_t* sn);
void update(sensor_node_t* sn);

#endif /* SENSOR_NODE_H */