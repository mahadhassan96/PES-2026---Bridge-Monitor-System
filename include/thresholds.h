#ifndef THRESHOLDS_H
#define THRESHOLDS_H
#include <cstdint>

typedef struct{
    const uint16_t min_distance_mm;
    const uint16_t max_angle_tilt_degrees;
    const uint16_t max_pressure_weight_N;
} Thresholds;


#endif // THRESHOLDS_H