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
    int32_t dist;
    int32_t accel_x;
    int32_t accel_y;
    int32_t accel_z;
    int32_t force;
} sensor_reading_t;

typedef struct
{
    bool dist_anomaly;
    // TODO: figure out which sensor to work with.
    // bool accel_anomaly;
    bool force_anomaly;
} sensor_states_t;

typedef enum 
{
    LOW,
    MEDIUM,
    HIGH
} sensor_sensitivity_t;

#endif /* READINGS_H */