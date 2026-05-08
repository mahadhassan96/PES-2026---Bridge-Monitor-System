#ifndef THRESHOLDS_H
#define THRESHOLDS_H

#include <stdint.h>

typedef struct{
    const uint16_t min_distance_mm;
    const uint16_t max_angle_tilt_degrees;
    const uint16_t max_pressure_weight_N;
} Thresholds;

extern const Thresholds thresholds;

#endif // THRESHOLDS_H