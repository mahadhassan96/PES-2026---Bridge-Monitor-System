#ifndef VL53L1X_H
#define VL53L1X_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include "I2C_interface.h"

typedef enum {
    VL53L1_RANGESTATUS_RANGE_VALID                    = 0,
    VL53L1_RANGESTATUS_SIGMA_FAIL                     = 1,
    VL53L1_RANGESTATUS_SIGNAL_FAIL                    = 2,
    VL53L1_RANGESTATUS_RANGE_VALID_MIN_RANGE_CLIPPED  = 3,
    VL53L1_RANGESTATUS_OUT_OF_BOUNDS_FAIL             = 4,
    VL53L1_RANGESTATUS_HARDWARE_FAIL                  = 5,
    VL53L1_RANGESTATUS_RANGE_VALID_NO_WRAP_CHECK_FAIL = 6,
    VL53L1_RANGESTATUS_WRAP_TARGET_FAIL               = 7,
    VL53L1_RANGESTATUS_XTALK_SIGNAL_FAIL              = 9,
    VL53L1_RANGESTATUS_SYNCHRONIZATION_INT            = 10,
    VL53L1_RANGESTATUS_MIN_RANGE_FAIL                 = 13,
    VL53L1_RANGESTATUS_NONE                           = 255
} RangeStatus;

typedef struct {
    uint8_t range_status;
    uint8_t stream_count;
    uint16_t dss_actual_effective_spads_sd0;
    uint16_t ambient_count_rate_mcps_sd0;
    uint16_t final_crosstalk_corrected_range_mm_sd0;
    uint16_t peak_signal_count_rate_crosstalk_corrected_mcps_sd0;
} ResultBuffer;

typedef struct {
    uint16_t range_mm;
    RangeStatus range_status;
    float signal_count_rate;
    float ambient_count_rate;
} MeasurementData;

struct vl53l1x_config {
    struct i2c_dt_spec i2c;
    struct gpio_dt_spec int_gpio;
    uint16_t threshold_mm;
    uint32_t sample_window_ms;
    uint32_t intermeasurement_period_ms;
};

struct vl53l1x_data {
    struct gpio_callback gpio_cb;
    struct k_work work;

    bool did_timeout;
    bool calibrated;

    uint16_t fast_osc_frequency;
    uint16_t osc_calibrate_val;

    uint32_t timeout_value;
    k_timepoint_t timeout_deadline;

    uint16_t range_mm;
    uint8_t range_status;

    ResultBuffer results;
    MeasurementData reading_data;

    uint32_t sum_mm;
    uint16_t sample_count;
    uint16_t avg_mm;
};

uint16_t vl53l1x_read(const struct device *dev);
int vl53l1x_start_continuous(const struct device *dev);
int avgSampleReading(const struct device *dev);

#endif