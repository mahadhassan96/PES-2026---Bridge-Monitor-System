#ifndef READINGS_H  
#define READINGS_H

#include "stdint.h"
#include "stdbool.h"

typedef enum
{
    REQUESTED,
    UNREQUESTED
} reading_type_t;

typedef struct 
{
    uint64_t timestamp;
    reading_type_t type;
    float dist;
    // TODO: figure out which sensor to work with.
    // float accel; 
    float force;
} sensor_reading_t;

typedef struct
{
    bool dist_anomaly;
    // TODO: figure out which sensor to work with.
    // bool accel_anomaly;
    bool force_anomaly;
} sensor_states_t;

#endif /* READINGS_H */