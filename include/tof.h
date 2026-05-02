#ifndef TOF_H 
#define TOF_H

#include "stdint.h"
#include "stdbool.h" 
#include <zephyr/kernel.h>
#include "include/I2C_interface.h"

#define VL53L1X_ADDR 0x29
#define READ_TIMEOUT_MS 500

typedef enum
{
    SOFT_RESET                                                                 = 0x0000,
    I2C_SLAVE_DEVICE_ADDRESS                                                   = 0x0001,
    OSC_MEASURED__FAST_OSC__FREQUENCY                                          = 0x0006,
    RESULT__OSC_CALIBRATE_VAL                                                  = 0x00DE,
    FIRMWARE__SYSTEM_STATUS                                                    = 0x00E5,
    PAD_I2C_HV__EXTSUP_CONFIG                                                  = 0x002E,
    DSS_CONFIG__TARGET_TOTAL_RATE_MCPS                                         = 0x0024,
    GPIO__TIO_HV_STATUS                                                        = 0x0031,
    SIGMA_ESTIMATOR__EFFECTIVE_PULSE_WIDTH_NS                                  = 0x0036,
    SIGMA_ESTIMATOR__EFFECTIVE_AMBIENT_WIDTH_NS                                = 0x0037,
    ALGO__CROSSTALK_COMPENSATION_VALID_HEIGHT_MM                               = 0x0039,
    ALGO__RANGE_IGNORE_VALID_HEIGHT_MM                                         = 0x003E,
    ALGO__RANGE_MIN_CLIP                                                       = 0x003F,
    ALGO__CONSISTENCY_CHECK__TOLERANCE                                         = 0x0040,
    SYSTEM__THRESH_RATE_HIGH                                                   = 0x0050,
    SYSTEM__THRESH_RATE_LOW                                                    = 0x0052,
    DSS_CONFIG__APERTURE_ATTENUATION                                           = 0x0057,
    MM_CONFIG__TIMEOUT_MACROP_A                                                = 0x005A, // added by Pololu for 16-bit accesses
    MM_CONFIG__TIMEOUT_MACROP_B                                                = 0x005C, // added by Pololu for 16-bit accesses
    RANGE_CONFIG__TIMEOUT_MACROP_A                                             = 0x005E, // added by Pololu for 16-bit accesses
    RANGE_CONFIG__TIMEOUT_MACROP_B                                             = 0x0061, // added by Pololu for 16-bit accesses
    RANGE_CONFIG__SIGMA_THRESH                                                 = 0x0064,
    RANGE_CONFIG__MIN_COUNT_RATE_RTN_LIMIT_MCPS                                = 0x0066,
    SYSTEM__GROUPED_PARAMETER_HOLD_0                                           = 0x0071,
    SYSTEM__GROUPED_PARAMETER_HOLD_1                                           = 0x007C,  
    SD_CONFIG__QUANTIFIER                                                      = 0x007E,
    SYSTEM__GROUPED_PARAMETER_HOLD                                             = 0x0082,
    SYSTEM__SEED_CONFIG                                                        = 0x0077,
    ROI_CONFIG__USER_ROI_CENTRE_SPAD                                           = 0x007F,
    ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE                              = 0x0080,
    SYSTEM__SEQUENCE_CONFIG                                                    = 0x0081,
    DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT                                  = 0x0054,
    DSS_CONFIG__ROI_MODE_CONTROL                                               = 0x004F,
    ALGO__PART_TO_PART_RANGE_OFFSET_MM                                         = 0x001E,
    MM_CONFIG__OUTER_OFFSET_MM                                                 = 0x0022,
    RANGE_CONFIG__VCSEL_PERIOD_A                                               = 0x0060,
    RANGE_CONFIG__VCSEL_PERIOD_B                                               = 0x0063,
    RANGE_CONFIG__VALID_PHASE_HIGH                                             = 0x0069,
    SD_CONFIG__WOI_SD0                                                         = 0x0078,
    SD_CONFIG__WOI_SD1                                                         = 0x0079,
    SD_CONFIG__INITIAL_PHASE_SD0                                               = 0x007A,
    SD_CONFIG__INITIAL_PHASE_SD1                                               = 0x007B,
    SYSTEM__INTERRUPT_CLEAR                                                    = 0x0086,
    SYSTEM__MODE_START                                                         = 0x0087,
    SYSTEM__INTERMEASUREMENT_PERIOD                                            = 0x006C
} regAddr;

typedef enum {
    SHORT_MODE,
    MEDIUM_MODE,
    LONG_MODE
} DistanceMode;

typedef enum 
{
    VL53L1_RANGESTATUS_RANGE_VALID                    =   0,
    VL53L1_RANGESTATUS_SIGMA_FAIL                     =   1,
    VL53L1_RANGESTATUS_SIGNAL_FAIL                    =   2,
    VL53L1_RANGESTATUS_RANGE_VALID_MIN_RANGE_CLIPPED  =   3,
    VL53L1_RANGESTATUS_OUT_OF_BOUNDS_FAIL             =   4,
    VL53L1_RANGESTATUS_HARDWARE_FAIL                  =   5,
    VL53L1_RANGESTATUS_RANGE_VALID_NO_WRAP_CHECK_FAIL =   6,
    VL53L1_RANGESTATUS_WRAP_TARGET_FAIL               =   7,
    VL53L1_RANGESTATUS_XTALK_SIGNAL_FAIL              =   9,
    VL53L1_RANGESTATUS_SYNCHRONIZATION_INT            =   10,
    VL53L1_RANGESTATUS_MIN_RANGE_FAIL                 =   13,
    VL53L1_RANGESTATUS_NONE                           =   255
} RangeStatus;

typedef struct 
{
    uint8_t range_status;
    uint8_t stream_count;
    uint16_t dss_actual_effective_spads_sd0;
    uint16_t ambient_count_rate_mcps_sd0;
    uint16_t final_crosstalk_corrected_range_mm_sd0;
    uint16_t peak_signal_count_rate_crosstalk_corrected_mcps_sd0;
} ResultBuffer;

typedef struct 
{
    uint16_t range_mm;
    RangeStatus range_status;
    float signal_count_rate;
    float ambient_count_rate;
} MeasurementData;

extern uint32_t timeout_value;
extern bool did_timeout;
extern MeasurementData Reading_data;
extern k_timepoint_t timeout_deadline;
extern uint16_t fast_osc_frequency;
extern uint16_t osc_calibrate_val;
static const uint16_t TargetRate = 0x0A00;
static const uint32_t TimingGuard = 4528;

bool tof_init(bool io_2v8);

bool setDistanceMode(DistanceMode mode);

void setROISize(uint8_t width, uint8_t height);

void startTimeout();

void setTimeout(uint16_t timeout) { timeout_value = timeout; }

bool setMeasurementTimingBudget(uint32_t budget_us);

uint32_t calcMacroPeriod(uint8_t vcsel_period);

uint32_t timeoutMicrosecondsToMclks(uint32_t timeout_us, uint32_t macro_period_us);

uint16_t encodeTimeout(uint32_t timeout_mclks);

bool tof_data_ready(void);

bool tof_read_results(void);

static float count_rate_fixed_to_float(uint16_t count_rate_fixed);

void tof_get_Reading_data(void);

bool tof_update_dss(void);

bool tof_start_continuous(uint32_t period_ms);

uint16_t tof_read(bool blocking);

bool avgSampleReading(uint16_t *avg_mm, uint16_t samples_window_timeout_ms);

#endif 
