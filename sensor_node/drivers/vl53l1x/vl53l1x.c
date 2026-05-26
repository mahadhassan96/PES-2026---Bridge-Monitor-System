#include "vl53l1x.h"
#define DT_DRV_COMPAT vl53l1x_st
#define READ_TIMEOUT_MS 500

typedef enum {
    SHORT_MODE,
    MEDIUM_MODE,
    LONG_MODE
} DistanceMode;

static bool setDistanceMode(const struct device *dev, DistanceMode mode);
static int setMeasurementTimingBudget(const struct device *dev, uint32_t budget_us);
static void setTimeout(const struct device *dev, uint32_t timeout_ms);
static void startTimeout(const struct device *dev);
static bool isTimeoutExpired(const struct device *dev);
static uint32_t calcMacroPeriod(struct vl53l1x_data *data, uint8_t vcsel_period);
static uint16_t encodeTimeout(uint32_t timeout_mclks);
static uint32_t timeoutMicrosecondsToMclks(uint32_t timeout_us, uint32_t macro_period_us);
static int vl53l1x_set_distance_threshold_interrupt(const struct device *dev);

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
    RESULT__RANGE_STATUS                                                       = 0x0089,
    SYSTEM__INTERMEASUREMENT_PERIOD                                            = 0x006C,
    SYSTEM__INTERRUPT_CONFIG_GPIO                                              = 0x0046,
    SYSTEM__THRESH_HIGH                                                        = 0x0072,
    SYSTEM__THRESH_LOW                                                         = 0x0074
} regAddr;

static const uint16_t TargetRate = 0x0A00;
static const uint32_t TimingGuard = 4528;

static int vl53l1x_init(const struct device *dev)
{
    printk("start init\n");
    struct vl53l1x_data *data = dev->data;
    const struct vl53l1x_config *cfg = dev->config;

    data->did_timeout = false;
    data->fast_osc_frequency = 0;
    data->osc_calibrate_val = 0;
    data->timeout_value = 0;
    data->range_mm = 0;
    data->range_status = VL53L1_RANGESTATUS_NONE;
    memset(&data->reading_data, 0, sizeof(data->reading_data));
    memset(&data->results, 0, sizeof(data->results));

    //printk("start reset\n");
    int res = sensor_write_reg_u8(cfg->i2c.addr, SOFT_RESET, 0x00);
    if(res == I2C_OK)
    {
       k_usleep(100); 
       sensor_write_reg_u8(cfg->i2c.addr, SOFT_RESET, 0x01); 
       k_usleep(100); 
    }
    else{
        //printk("reset failed %d\n", res);
        return -1;
    }

    setTimeout(dev, READ_TIMEOUT_MS);
    startTimeout(dev);
    uint8_t system_status;
    while (sensor_read_reg_u8(cfg->i2c.addr, FIRMWARE__SYSTEM_STATUS, &system_status) != I2C_OK || ((system_status & 0x01) == 0))
    {
        if (isTimeoutExpired(dev))
        {
            /// Note: we can Handle timeout with handshake later.
            //printk("timeout failed\n");
            return -1;
        }
    }
    
    uint8_t io_voltage; // 2.8V
    sensor_read_reg_u8(cfg->i2c.addr, PAD_I2C_HV__EXTSUP_CONFIG, &io_voltage);
    io_voltage |= 0x01;
    sensor_write_reg_u8(cfg->i2c.addr, PAD_I2C_HV__EXTSUP_CONFIG, io_voltage);

    sensor_read_reg_u16(cfg->i2c.addr, OSC_MEASURED__FAST_OSC__FREQUENCY, &data->fast_osc_frequency);
    sensor_read_reg_u16(cfg->i2c.addr, RESULT__OSC_CALIBRATE_VAL, &data->osc_calibrate_val);

    sensor_write_reg_u16(cfg->i2c.addr, DSS_CONFIG__TARGET_TOTAL_RATE_MCPS, TargetRate);
    sensor_write_reg_u8(cfg->i2c.addr, GPIO__TIO_HV_STATUS, 0x02); 
    sensor_write_reg_u8(cfg->i2c.addr, SIGMA_ESTIMATOR__EFFECTIVE_PULSE_WIDTH_NS, 8);
    sensor_write_reg_u8(cfg->i2c.addr, SIGMA_ESTIMATOR__EFFECTIVE_AMBIENT_WIDTH_NS, 16);
    sensor_write_reg_u8(cfg->i2c.addr, ALGO__CROSSTALK_COMPENSATION_VALID_HEIGHT_MM, 0x01);
    sensor_write_reg_u8(cfg->i2c.addr, ALGO__RANGE_IGNORE_VALID_HEIGHT_MM, 0xFF);
    sensor_write_reg_u8(cfg->i2c.addr, ALGO__RANGE_MIN_CLIP, 0x00);
    sensor_write_reg_u8(cfg->i2c.addr, ALGO__CONSISTENCY_CHECK__TOLERANCE, 2);

    sensor_write_reg_u16(cfg->i2c.addr, SYSTEM__THRESH_RATE_HIGH, 0x0000);
    sensor_write_reg_u16(cfg->i2c.addr, SYSTEM__THRESH_RATE_LOW, 0x0000);
    sensor_write_reg_u8(cfg->i2c.addr, DSS_CONFIG__APERTURE_ATTENUATION, 0x38);

    sensor_write_reg_u16(cfg->i2c.addr, RANGE_CONFIG__SIGMA_THRESH, 360);
    sensor_write_reg_u16(cfg->i2c.addr, RANGE_CONFIG__MIN_COUNT_RATE_RTN_LIMIT_MCPS, 192);

    sensor_write_reg_u8(cfg->i2c.addr, SYSTEM__GROUPED_PARAMETER_HOLD_0, 0x01);
    sensor_write_reg_u8(cfg->i2c.addr, SYSTEM__GROUPED_PARAMETER_HOLD_1, 0x01);
    sensor_write_reg_u8(cfg->i2c.addr, SD_CONFIG__QUANTIFIER, 2);

    sensor_write_reg_u8(cfg->i2c.addr, SYSTEM__GROUPED_PARAMETER_HOLD, 0x00);
    sensor_write_reg_u8(cfg->i2c.addr, SYSTEM__SEED_CONFIG, 0x01);

    sensor_write_reg_u8(cfg->i2c.addr, SYSTEM__SEQUENCE_CONFIG, 0x8B);
    sensor_write_reg_u16(cfg->i2c.addr, DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT, 200 << 8);
    sensor_write_reg_u8(cfg->i2c.addr, DSS_CONFIG__ROI_MODE_CONTROL, 2);
    
    setDistanceMode(dev, MEDIUM_MODE);
    setMeasurementTimingBudget(dev, 50000);  // 50ms

    uint16_t MM;
    sensor_read_reg_u16(cfg->i2c.addr, MM_CONFIG__OUTER_OFFSET_MM, &MM);
    sensor_write_reg_u16(cfg->i2c.addr, ALGO__PART_TO_PART_RANGE_OFFSET_MM, MM * 4);

    if (vl53l1x_set_distance_threshold_interrupt(dev) < 0) {
        return -1;
        //printk("thresh failed\n");
    }

    //printk("end init\n");
    return 0;
}

bool setDistanceMode(const struct device *dev, DistanceMode mode)
{
    const struct vl53l1x_config *cfg = dev->config;
    switch (mode)
    {
        case SHORT_MODE:
            sensor_write_reg_u8(cfg->i2c.addr, RANGE_CONFIG__VCSEL_PERIOD_A, 0x07);
            sensor_write_reg_u8(cfg->i2c.addr, RANGE_CONFIG__VCSEL_PERIOD_B, 0x05);
            sensor_write_reg_u8(cfg->i2c.addr, RANGE_CONFIG__VALID_PHASE_HIGH, 0x38);

            sensor_write_reg_u8(cfg->i2c.addr, SD_CONFIG__WOI_SD0, 0x07);
            sensor_write_reg_u8(cfg->i2c.addr, SD_CONFIG__WOI_SD1, 0x05);
            sensor_write_reg_u8(cfg->i2c.addr, SD_CONFIG__INITIAL_PHASE_SD0, 6);
            sensor_write_reg_u8(cfg->i2c.addr, SD_CONFIG__INITIAL_PHASE_SD1, 6);
            break;

        case MEDIUM_MODE:
            sensor_write_reg_u8(cfg->i2c.addr, RANGE_CONFIG__VCSEL_PERIOD_A, 0x0B);
            sensor_write_reg_u8(cfg->i2c.addr, RANGE_CONFIG__VCSEL_PERIOD_B, 0x09);
            sensor_write_reg_u8(cfg->i2c.addr, RANGE_CONFIG__VALID_PHASE_HIGH, 0x78);

            sensor_write_reg_u8(cfg->i2c.addr, SD_CONFIG__WOI_SD0, 0x0B);
            sensor_write_reg_u8(cfg->i2c.addr, SD_CONFIG__WOI_SD1, 0x09);
            sensor_write_reg_u8(cfg->i2c.addr, SD_CONFIG__INITIAL_PHASE_SD0, 10);
            sensor_write_reg_u8(cfg->i2c.addr, SD_CONFIG__INITIAL_PHASE_SD1, 10);
            break;

        case LONG_MODE:
            sensor_write_reg_u8(cfg->i2c.addr, RANGE_CONFIG__VCSEL_PERIOD_A, 0x0F);
            sensor_write_reg_u8(cfg->i2c.addr, RANGE_CONFIG__VCSEL_PERIOD_B, 0x0D);
            sensor_write_reg_u8(cfg->i2c.addr, RANGE_CONFIG__VALID_PHASE_HIGH, 0xB8);

            sensor_write_reg_u8(cfg->i2c.addr, SD_CONFIG__WOI_SD0, 0x0F);
            sensor_write_reg_u8(cfg->i2c.addr, SD_CONFIG__WOI_SD1, 0x0D);
            sensor_write_reg_u8(cfg->i2c.addr, SD_CONFIG__INITIAL_PHASE_SD0, 14);
            sensor_write_reg_u8(cfg->i2c.addr, SD_CONFIG__INITIAL_PHASE_SD1, 14);
            break;

        default:
            return -1; 
    }

    return 0;
}

static int setMeasurementTimingBudget(const struct device *dev, uint32_t budget_us)
{
    const struct vl53l1x_config *cfg = dev->config;
    struct vl53l1x_data *data = dev->data;
    if (budget_us <= TimingGuard) { return -1; }  
    uint32_t final_budget = budget_us - TimingGuard;
    if (final_budget > 1100000){ return -1; }

    final_budget /= 2; 
    uint8_t vcsel_period;
    sensor_read_reg_u8(cfg->i2c.addr, RANGE_CONFIG__VCSEL_PERIOD_A, &vcsel_period);
    uint32_t macro_period_us = calcMacroPeriod(data, vcsel_period);

    sensor_write_reg_u16(cfg->i2c.addr, MM_CONFIG__TIMEOUT_MACROP_A, encodeTimeout(timeoutMicrosecondsToMclks(1, macro_period_us)));
    sensor_write_reg_u16(cfg->i2c.addr, RANGE_CONFIG__TIMEOUT_MACROP_A, encodeTimeout(timeoutMicrosecondsToMclks(final_budget, macro_period_us)));

    macro_period_us = calcMacroPeriod(data, vcsel_period);

    sensor_write_reg_u16(cfg->i2c.addr, MM_CONFIG__TIMEOUT_MACROP_B, encodeTimeout(timeoutMicrosecondsToMclks(1, macro_period_us)));
    sensor_write_reg_u16(cfg->i2c.addr, RANGE_CONFIG__TIMEOUT_MACROP_B, encodeTimeout(timeoutMicrosecondsToMclks(final_budget, macro_period_us)));
   
    return 0;
}

void setROISize(const struct device *dev, uint8_t width, uint8_t height)
{
    const struct vl53l1x_config *cfg = dev->config;
    if ( width > 16) {  width = 16; }
    if (height > 16) { height = 16; }

    if (width > 10 || height > 10)
    {
        sensor_write_reg_u16(cfg->i2c.addr, ROI_CONFIG__USER_ROI_CENTRE_SPAD, 199);
    }

    sensor_write_reg_u16(cfg->i2c.addr, ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE, (height - 1) << 4 | (width - 1));
}

void startTimeout(const struct device *dev)
{
   struct vl53l1x_data *data = dev->data;
   data->timeout_deadline = sys_timepoint_calc(K_MSEC(data->timeout_value));
}

bool isTimeoutExpired(const struct device *dev)
{
    struct vl53l1x_data *data = dev->data;
    return sys_timepoint_expired(data->timeout_deadline);
}

uint32_t calcMacroPeriod(struct vl53l1x_data *data, uint8_t vcsel_period)
{
    uint32_t pll_period_us = ((uint32_t)0x01 << 30) / data->fast_osc_frequency;

    uint8_t vcsel_period_pclks = (vcsel_period + 1) << 1;

    uint32_t macro_period_us = (uint32_t)2304 * pll_period_us;
    macro_period_us >>= 6;
    macro_period_us *= vcsel_period_pclks;
    macro_period_us >>= 6;

    return macro_period_us;
}

uint32_t timeoutMicrosecondsToMclks(uint32_t timeout_us, uint32_t macro_period_us)
{
    return (((uint32_t)timeout_us << 12) + (macro_period_us >> 1)) / macro_period_us;
}

uint16_t encodeTimeout(uint32_t timeout_mclks)
{
    uint32_t ls_byte = 0;
    uint16_t ms_byte = 0;

    if (timeout_mclks > 0)
    {
        ls_byte = timeout_mclks - 1;

        while ((ls_byte & 0xFFFFFF00) > 0)
        {
            ls_byte >>= 1;
            ms_byte++;
        }

        return (ms_byte << 8) | (ls_byte & 0xFF);
    }
    else { return 0; }
}

static int vl53l1x_read_results(const struct device *dev)
{
    uint8_t buf[17];
    const struct vl53l1x_config *cfg = dev->config;
    struct vl53l1x_data *data = dev->data;
    if (sensor_read_reg(cfg->i2c.addr, RESULT__RANGE_STATUS, buf, sizeof(buf)) != I2C_OK) {
        return -1;
    }

    data->results.range_status = buf[0];
    data->results.stream_count = buf[2];

    data->results.dss_actual_effective_spads_sd0 =
        ((uint16_t)buf[3] << 8) | buf[4];

    data->results.ambient_count_rate_mcps_sd0 =
        ((uint16_t)buf[7] << 8) | buf[8];

    data->results.final_crosstalk_corrected_range_mm_sd0 =
        ((uint16_t)buf[13] << 8) | buf[14];

    data->results.peak_signal_count_rate_crosstalk_corrected_mcps_sd0 =
        ((uint16_t)buf[15] << 8) | buf[16];

    return 0;
}

static float count_rate_fixed_to_float(uint16_t count_rate_fixed)
{
    return (float)count_rate_fixed / 128.0f;
}

static void vl53l1x_get_reading_data(const struct device *dev)
{
    struct vl53l1x_data *data = dev->data;
    uint16_t range = data->results.final_crosstalk_corrected_range_mm_sd0;

    data->reading_data.range_mm = ((uint32_t)range * 2011 + 0x0400) / 0x0800;

    switch (data->results.range_status) {
    case 17:
    case 2:
    case 1:
    case 3:
        data->reading_data.range_status = VL53L1_RANGESTATUS_HARDWARE_FAIL;
        break;

    case 13:
        data->reading_data.range_status = VL53L1_RANGESTATUS_MIN_RANGE_FAIL;
        break;

    case 18:
        data->reading_data.range_status = VL53L1_RANGESTATUS_SYNCHRONIZATION_INT;
        break;

    case 5:
        data->reading_data.range_status = VL53L1_RANGESTATUS_OUT_OF_BOUNDS_FAIL;
        break;

    case 4:
        data->reading_data.range_status = VL53L1_RANGESTATUS_SIGNAL_FAIL;
        break;

    case 6:
        data->reading_data.range_status = VL53L1_RANGESTATUS_SIGMA_FAIL;
        break;

    case 7:
        data->reading_data.range_status = VL53L1_RANGESTATUS_WRAP_TARGET_FAIL;
        break;

    case 12:
        data->reading_data.range_status = VL53L1_RANGESTATUS_XTALK_SIGNAL_FAIL;
        break;

    case 8:
        data->reading_data.range_status = VL53L1_RANGESTATUS_RANGE_VALID_MIN_RANGE_CLIPPED;
        break;

    case 9:
        if (data->results.stream_count == 0) {
            data->reading_data.range_status = VL53L1_RANGESTATUS_RANGE_VALID_NO_WRAP_CHECK_FAIL;
        } else {
            data->reading_data.range_status = VL53L1_RANGESTATUS_RANGE_VALID;
        }
        break;

    default:
        data->reading_data.range_status = VL53L1_RANGESTATUS_NONE;
        break;
    }

    data->reading_data.signal_count_rate = count_rate_fixed_to_float(data->results.peak_signal_count_rate_crosstalk_corrected_mcps_sd0);
    data->reading_data.ambient_count_rate = count_rate_fixed_to_float(data->results.ambient_count_rate_mcps_sd0);
}

static int vl53l1x_update_dss(const struct device *dev)
{
    const struct vl53l1x_config *cfg = dev->config;
    struct vl53l1x_data *data = dev->data;

    uint16_t spad_count = data->results.dss_actual_effective_spads_sd0;

    if (spad_count != 0) {
        uint32_t total_rate_per_spad =
            (uint32_t)data->results.peak_signal_count_rate_crosstalk_corrected_mcps_sd0 +
            data->results.ambient_count_rate_mcps_sd0;

        if (total_rate_per_spad > 0xFFFF) {
            total_rate_per_spad = 0xFFFF;
        }

        total_rate_per_spad <<= 16;
        total_rate_per_spad /= spad_count;

        if (total_rate_per_spad != 0) {
            uint32_t required_spads =
                ((uint32_t)TargetRate << 16) / total_rate_per_spad;

            if (required_spads > 0xFFFF) {
                required_spads = 0xFFFF;
            }

            return sensor_write_reg_u16(
                cfg->i2c.addr,
                DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT,
                (uint16_t)required_spads
            ) == I2C_OK ? 0 : -1;
        }
    }

    return sensor_write_reg_u16(
        cfg->i2c.addr,
        DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT,
        0x8000
    ) == I2C_OK ? 0 : -1;
}

uint16_t vl53l1x_read(const struct device *dev)
{
    const struct vl53l1x_config *cfg = dev->config;
    struct vl53l1x_data *data = dev->data;

    if (vl53l1x_read_results(dev) != 0) { return 0; }
    if (vl53l1x_update_dss(dev) != 0) { return 0; }

    vl53l1x_get_reading_data(dev);
    sensor_write_reg_u8(cfg->i2c.addr, SYSTEM__INTERRUPT_CLEAR, 0x01);

    return data->reading_data.range_mm;
}

int avgSampleReading(const struct device *dev)
{
    const struct vl53l1x_config *cfg = dev->config;
    struct vl53l1x_data *data = dev->data;
    data->sum_mm = 0;
    data->sample_count = 0;

    data->did_timeout = false;
    if (cfg->sample_window_ms <= READ_TIMEOUT_MS + 100)
    {
        printk("Sample window timeout must be greater than read timeout + 100ms\n");
        return -1;
    }
    
    setTimeout(dev, cfg->sample_window_ms);
    startTimeout(dev);

    while (!isTimeoutExpired(dev))
    {
        uint16_t distance = vl53l1x_read(dev); 
        if (!data->did_timeout && distance > 0 && data->reading_data.range_status == VL53L1_RANGESTATUS_RANGE_VALID)
        {
            data->sum_mm += distance;
            data->sample_count++;
        }
    }

    if (data->sample_count == 0){ return -1; }
    data->avg_mm = data->sum_mm / data->sample_count;

    return data->avg_mm;
}

int vl53l1x_start_continuous(const struct device *dev)
{
    const struct vl53l1x_config *cfg = dev->config;
    struct vl53l1x_data *data = dev->data;

    if (sensor_write_reg_u32(cfg->i2c.addr, SYSTEM__INTERMEASUREMENT_PERIOD, cfg->intermeasurement_period_ms * data->osc_calibrate_val) != I2C_OK){
        return -1;
    }

    if (sensor_write_reg_u8(cfg->i2c.addr, SYSTEM__INTERRUPT_CLEAR, 0x01) != I2C_OK ){
        return -1;
    }
  
    return sensor_write_reg_u8(cfg->i2c.addr, SYSTEM__MODE_START, 0x40) == I2C_OK;  
}

static int vl53l1x_set_distance_threshold_interrupt(const struct device *dev)
{
    const struct vl53l1x_config *cfg = dev->config;
    i2c_status ret;

    ret = sensor_write_reg_u16(cfg->i2c.addr, SYSTEM__THRESH_LOW, cfg->threshold_mm);
    printk("Write THRESH_LOW ret=%d value=%d\n", ret, cfg->threshold_mm);
    if (ret != I2C_OK) {
        return -1;
    }

    ret = sensor_write_reg_u16(cfg->i2c.addr, SYSTEM__THRESH_HIGH, 0xFFFF);
    printk("Write THRESH_HIGH ret=%d value=0x%04X\n", ret, 0xFFFF);
    if (ret != I2C_OK) {
        return -1;
    }

    ret = sensor_write_reg_u8(cfg->i2c.addr, SYSTEM__INTERRUPT_CONFIG_GPIO, 0x04);
    printk("Write INTERRUPT_CONFIG_GPIO ret=%d value=0x%02X\n", ret, 0x04);
    if (ret != I2C_OK) {
        return -1;
    }

    ret = sensor_write_reg_u8(cfg->i2c.addr, SYSTEM__INTERRUPT_CLEAR, 0x01);
    printk("Write INTERRUPT_CLEAR ret=%d value=0x%02X\n", ret, 0x01);
    if (ret != I2C_OK) {
        return -1;
    }

    printk("Distance threshold interrupt enabled\n");
    return 0;
}

static void setTimeout(const struct device *dev, uint32_t timeout_ms)
{
    struct vl53l1x_data *data = dev->data;
    data->timeout_value = timeout_ms;
}

#define VL53L1X_DEFINE(inst)                                                \
    static struct vl53l1x_data vl53l1x_data_##inst;                         \
                                                                            \
    static const struct vl53l1x_config vl53l1x_config_##inst = {            \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                                  \
        .int_gpio = GPIO_DT_SPEC_INST_GET(inst, int_gpios),                 \
        .threshold_mm = DT_INST_PROP(inst, threshold_mm),                   \
        .intermeasurement_period_ms = DT_INST_PROP(inst, intermeasurement_period_ms), \
        .sample_window_ms = DT_INST_PROP(inst, sample_window_ms),           \
    };                                                                      \
                                                                            \
    DEVICE_DT_INST_DEFINE(inst,                                             \
                          vl53l1x_init,                                     \
                          NULL,                                             \
                          &vl53l1x_data_##inst,                            \
                          &vl53l1x_config_##inst,                          \
                          POST_KERNEL,                                      \
                          CONFIG_SENSOR_INIT_PRIORITY,                      \
                          NULL);

DT_INST_FOREACH_STATUS_OKAY(VL53L1X_DEFINE)

