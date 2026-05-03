#include "include/tof.h"

bool tof_init(bool io_2v8)
{

    if(sensor_write_reg_u8(VL53L1X_ADDR, SOFT_RESET, 0x00) == I2C_OK)
    {
       k_usleep(100); // Sleep for 100ms after reset
       sensor_write_reg_u8(VL53L1X_ADDR, SOFT_RESET, 0x01); 
       k_delay(1); // Sleep for 100ms after reset
    }

    setTimeout(READ_TIMEOUT_MS);
    startTimeout();
    uint8_t system_status;
    while (sensor_read_reg_u8(VL53L1X_ADDR, FIRMWARE__SYSTEM_STATUS, &system_status) != I2C_OK || ((system_status & 0x01) == 0))
    {
        if (isTimeoutExpired())
        {
            /// Handle timeout with handshake 
            return false;
        }
        
    }
    
    if (io_2v8)
    {
        uint8_t io_voltage; // 2.8V
        sensor_read_reg_u8(VL53L1X_ADDR, PAD_I2C_HV__EXTSUP_CONFIG, &io_voltage);
        io_voltage |= 0x01;
        sensor_write_reg_u8(VL53L1X_ADDR, PAD_I2C_HV__EXTSUP_CONFIG, io_voltage);
    }

    sensor_read_reg_u16(VL53L1X_ADDR, OSC_MEASURED__FAST_OSC__FREQUENCY, &fast_osc_frequency);
    sensor_read_reg_u16(VL53L1X_ADDR, RESULT__OSC_CALIBRATE_VAL, &osc_calibrate_val);

    sensor_write_reg_u16(VL53L1X_ADDR, DSS_CONFIG__TARGET_TOTAL_RATE_MCPS, TargetRate);
    sensor_write_reg_u8(VL53L1X_ADDR, GPIO__TIO_HV_STATUS, 0x02); 
    sensor_write_reg_u8(VL53L1X_ADDR, SIGMA_ESTIMATOR__EFFECTIVE_PULSE_WIDTH_NS, 8);
    sensor_write_reg_u8(VL53L1X_ADDR, SIGMA_ESTIMATOR__EFFECTIVE_AMBIENT_WIDTH_NS, 16);
    sensor_write_reg_u8(VL53L1X_ADDR, ALGO__CROSSTALK_COMPENSATION_VALID_HEIGHT_MM, 0x01);
    sensor_write_reg_u8(VL53L1X_ADDR, ALGO__RANGE_IGNORE_VALID_HEIGHT_MM, 0xFF);
    sensor_write_reg_u8(VL53L1X_ADDR, ALGO__RANGE_MIN_CLIP, 0x00);
    sensor_write_reg_u8(VL53L1X_ADDR, ALGO__CONSISTENCY_CHECK__TOLERANCE, 2);

    sensor_write_reg_u16(VL53L1X_ADDR, SYSTEM__THRESH_RATE_HIGH, 0x0000);
    sensor_write_reg_u16(VL53L1X_ADDR, SYSTEM__THRESH_RATE_LOW, 0x0000);
    sensor_write_reg_u8(VL53L1X_ADDR, DSS_CONFIG__APERTURE_ATTENUATION, 0x38);

    sensor_write_reg_u16(VL53L1X_ADDR, RANGE_CONFIG__SIGMA_THRESH, 360);
    sensor_write_reg_u16(VL53L1X_ADDR, RANGE_CONFIG__MIN_COUNT_RATE_RTN_LIMIT_MCPS, 192);

    sensor_write_reg_u8(VL53L1X_ADDR, SYSTEM__GROUPED_PARAMETER_HOLD_0, 0x01);
    sensor_write_reg_u8(VL53L1X_ADDR, SYSTEM__GROUPED_PARAMETER_HOLD_1, 0x01);
    sensor_write_reg_u8(VL53L1X_ADDR, SD_CONFIG__QUANTIFIER, 2);

    sensor_write_reg_u8(VL53L1X_ADDR, SYSTEM__GROUPED_PARAMETER_HOLD, 0x00);
    sensor_write_reg_u8(VL53L1X_ADDR, SYSTEM__SEED_CONFIG, 0x01);

    sensor_write_reg_u8(VL53L1X_ADDR, SYSTEM__SEQUENCE_CONFIG, 0x8B);
    sensor_write_reg_u16(VL53L1X_ADDR, DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT, 200 << 8);
    sensor_write_reg_u8(VL53L1X_ADDR, DSS_CONFIG__ROI_MODE_CONTROL, 2);
    
    setDistanceMode(MEDIUM_MODE);
    setMeasurementTimingBudget(50000);  // 50ms

    uint16_t MM;
    sensor_read_reg_u16(VL53L1X_ADDR, MM_CONFIG__OUTER_OFFSET_MM, &MM);
    sensor_write_reg_u16(VL53L1X_ADDR, ALGO__PART_TO_PART_RANGE_OFFSET_MM, MM * 4);

    return true;
}

bool setDistanceMode(DistanceMode mode)
{
    uint8_t timing_budget;
    switch (mode)
    {
        case SHORT_MODE:
            sensor_write_reg_u8(VL53L1X_ADDR, RANGE_CONFIG__VCSEL_PERIOD_A, 0x07);
            sensor_write_reg_u8(VL53L1X_ADDR, RANGE_CONFIG__VCSEL_PERIOD_B, 0x05);
            sensor_write_reg_u8(VL53L1X_ADDR, RANGE_CONFIG__VALID_PHASE_HIGH, 0x38);

            sensor_write_reg_u8(VL53L1X_ADDR, SD_CONFIG__WOI_SD0, 0x07);
            sensor_write_reg_u8(VL53L1X_ADDR, SD_CONFIG__WOI_SD1, 0x05);
            sensor_write_reg_u8(VL53L1X_ADDR, SD_CONFIG__INITIAL_PHASE_SD0, 6);
            sensor_write_reg_u8(VL53L1X_ADDR, SD_CONFIG__INITIAL_PHASE_SD1, 6);
            break;

        case MEDIUM_MODE:
            sensor_write_reg_u8(VL53L1X_ADDR, RANGE_CONFIG__VCSEL_PERIOD_A, 0x0B);
            sensor_write_reg_u8(VL53L1X_ADDR, RANGE_CONFIG__VCSEL_PERIOD_B, 0x09);
            sensor_write_reg_u8(VL53L1X_ADDR, RANGE_CONFIG__VALID_PHASE_HIGH, 0x78);

            sensor_write_reg_u8(VL53L1X_ADDR, SD_CONFIG__WOI_SD0, 0x0B);
            sensor_write_reg_u8(VL53L1X_ADDR, SD_CONFIG__WOI_SD1, 0x09);
            sensor_write_reg_u8(VL53L1X_ADDR, SD_CONFIG__INITIAL_PHASE_SD0, 10);
            sensor_write_reg_u8(VL53L1X_ADDR, SD_CONFIG__INITIAL_PHASE_SD1, 10);
            break;

        case LONG_MODE:
            sensor_write_reg_u8(VL53L1X_ADDR, RANGE_CONFIG__VCSEL_PERIOD_A, 0x0F);
            sensor_write_reg_u8(VL53L1X_ADDR, RANGE_CONFIG__VCSEL_PERIOD_B, 0x0D);
            sensor_write_reg_u8(VL53L1X_ADDR, RANGE_CONFIG__VALID_PHASE_HIGH, 0xB8);

            sensor_write_reg_u8(VL53L1X_ADDR, SD_CONFIG__WOI_SD0, 0x0F);
            sensor_write_reg_u8(VL53L1X_ADDR, SD_CONFIG__WOI_SD1, 0x0D);
            sensor_write_reg_u8(VL53L1X_ADDR, SD_CONFIG__INITIAL_PHASE_SD0, 14);
            sensor_write_reg_u8(VL53L1X_ADDR, SD_CONFIG__INITIAL_PHASE_SD1, 14);
            break;

        default:
            return false; 
    }
    return true;
}

bool setMeasurementTimingBudget(uint32_t budget_us)
{
    if (budget_us <= TimingGuard) { return false; }  
    uint32_t final_budget = budget_us - TimingGuard;
    if (final_budget > 1100000){ return false; }

    final_budget /= 2; 
    uint8_t vcsel_period;
    sensor_read_reg_u8(VL53L1X_ADDR, RANGE_CONFIG__VCSEL_PERIOD_A, &vcsel_period);
    uint32_t macro_period_us = calcMacroPeriod(vcsel_period);
    uint32_t phasecal_timeout_mclks = timeoutMicrosecondsToMclks(1000, macro_period_us);

    sensor_write_reg_u16(VL53L1X_ADDR, MM_CONFIG__TIMEOUT_MACROP_A, encodeTimeout(timeoutMicrosecondsToMclks(1, macro_period_us)));
    sensor_write_reg_u16(VL53L1X_ADDR, RANGE_CONFIG__TIMEOUT_MACROP_A, encodeTimeout(timeoutMicrosecondsToMclks(final_budget, macro_period_us)));

    sensor_read_reg_u8(VL53L1X_ADDR, RANGE_CONFIG__VCSEL_PERIOD_B, &vcsel_period);
    macro_period_us = calcMacroPeriod(vcsel_period);

    sensor_write_reg_u16(VL53L1X_ADDR, MM_CONFIG__TIMEOUT_MACROP_B, encodeTimeout(timeoutMicrosecondsToMclks(1, macro_period_us)));
    sensor_write_reg_u16(VL53L1X_ADDR, RANGE_CONFIG__TIMEOUT_MACROP_B, encodeTimeout(timeoutMicrosecondsToMclks(final_budget, macro_period_us)));
    return true;
}

void setROISize(uint8_t width, uint8_t height)
{
    if ( width > 16) {  width = 16; }
    if (height > 16) { height = 16; }

    if (width > 10 || height > 10)
    {
        sensor_write_reg_u16(VL53L1X_ADDR, ROI_CONFIG__USER_ROI_CENTRE_SPAD, 199);
    }

    sensor_write_reg_u16(VL53L1X_ADDR, ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE, (height - 1) << 4 | (width - 1));
}

void startTimeout()
{
    timeout_deadline = sys_timepoint_calc(K_MSEC(timeout_value));
}

bool isTimeoutExpired()
{
    return sys_timepoint_expired(timeout_deadline);
}

uint32_t calcMacroPeriod(uint8_t vcsel_period)
{
    uint32_t pll_period_us = ((uint32_t)0x01 << 30) / fast_osc_frequency;

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

bool tof_data_ready(void)
{
    uint8_t status = 0;
    if (sensor_read_reg_u8(VL53L1X_ADDR, GPIO__TIO_HV_STATUS, &status) != I2C_OK) {
        return false;
    }

    return (status & 0x01) == 0;
}

bool tof_read_results(void)
{
    uint8_t buf[17];

    if (sensor_read_reg(VL53L1X_ADDR, RESULT__RANGE_STATUS, buf, sizeof(buf)) != I2C_OK) {
        return false;
    }

    results.range_status = buf[0];
    results.stream_count = buf[2];

    results.dss_actual_effective_spads_sd0 =
        ((uint16_t)buf[3] << 8) | buf[4];

    results.ambient_count_rate_mcps_sd0 =
        ((uint16_t)buf[7] << 8) | buf[8];

    results.final_crosstalk_corrected_range_mm_sd0 =
        ((uint16_t)buf[13] << 8) | buf[14];

    results.peak_signal_count_rate_crosstalk_corrected_mcps_sd0 =
        ((uint16_t)buf[15] << 8) | buf[16];

    return true;
}

static float count_rate_fixed_to_float(uint16_t count_rate_fixed)
{
    return (float)count_rate_fixed / 128.0f;
}

void tof_get_Reading_data(void)
{
    uint16_t range = results.final_crosstalk_corrected_range_mm_sd0;

    Reading_data.range_mm = ((uint32_t)range * 2011 + 0x0400) / 0x0800;

    switch (results.range_status) {
    case 17:
    case 2:
    case 1:
    case 3:
        Reading_data.range_status = VL53L1_RANGESTATUS_HARDWARE_FAIL;
        break;

    case 13:
        Reading_data.range_status = VL53L1_RANGESTATUS_MIN_RANGE_FAIL;
        break;

    case 18:
        Reading_data.range_status = VL53L1_RANGESTATUS_SYNCHRONIZATION_INT;
        break;

    case 5:
        Reading_data.range_status = VL53L1_RANGESTATUS_OUT_OF_BOUNDS_FAIL;
        break;

    case 4:
        Reading_data.range_status = VL53L1_RANGESTATUS_SIGNAL_FAIL;
        break;

    case 6:
        Reading_data.range_status = VL53L1_RANGESTATUS_SIGMA_FAIL;
        break;

    case 7:
        Reading_data.range_status = VL53L1_RANGESTATUS_WRAP_TARGET_FAIL;
        break;

    case 12:
        Reading_data.range_status = VL53L1_RANGESTATUS_XTALK_SIGNAL_FAIL;
        break;

    case 8:
        Reading_data.range_status = VL53L1_RANGESTATUS_RANGE_VALID_MIN_RANGE_CLIPPED;
        break;

    case 9:
        if (results.stream_count == 0) {
            Reading_data.range_status = VL53L1_RANGESTATUS_RANGE_VALID_NO_WRAP_CHECK_FAIL;
        } else {
            Reading_data.range_status = VL53L1_RANGESTATUS_RANGE_VALID;
        }
        break;

    default:
        Reading_data.range_status = VL53L1_RANGESTATUS_NONE;
        break;
    }

    Reading_data.signal_count_rate = count_rate_fixed_to_float(results.peak_signal_count_rate_crosstalk_corrected_mcps_sd0);
    Reading_data.ambient_count_rate = count_rate_fixed_to_float(results.ambient_count_rate_mcps_sd0);
}

bool tof_update_dss(void)
{
    uint16_t spad_count = results.dss_actual_effective_spads_sd0;
    if (spad_count != 0) {
        uint32_t total_rate_per_spad = (uint32_t)results.peak_signal_count_rate_crosstalk_corrected_mcps_sd0 + results.ambient_count_rate_mcps_sd0;

        if (total_rate_per_spad > 0xFFFF) {
            total_rate_per_spad = 0xFFFF;
        }

        total_rate_per_spad <<= 16;
        total_rate_per_spad /= spad_count;

        if (total_rate_per_spad != 0) {
            uint32_t required_spads = ((uint32_t)TargetRate << 16) / total_rate_per_spad;
            if (required_spads > 0xFFFF) {
                required_spads = 0xFFFF;
            }

            return sensor_write_reg_u16(VL53L1X_ADDR,DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT,(uint16_t)required_spads) == I2C_OK;
        }
    }

    return sensor_write_reg_u16(VL53L1X_ADDR, DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT, 0x8000) == I2C_OK;
}

uint16_t tof_read(bool blocking, const Thresholds thresholds)
{
    if (blocking) {
        setTimeout(READ_TIMEOUT_MS);
        startTimeout();

        while (!tof_data_ready()) {
            if (isTimeoutExpired()) {
                did_timeout = true;
                return 0;
            }

            k_sleep(K_MSEC(1));
        }
    }

    if (!tof_read_results()) { return 0; }
    if (!tof_update_dss()) { return 0; }

    tof_get_Reading_data();
    sensor_write_reg_u8(VL53L1X_ADDR, SYSTEM__INTERRUPT_CLEAR, 0x01);

    return Reading_data.range_mm;
}

bool avgSampleReading(uint16_t *avg_mm, uint16_t samples_window_timeout_ms, const Thresholds thresholds)
{
    uint32_t sum = 0;
    uint16_t valid_samples = 0;

    did_timeout = false;
    if (samples_window_timeout_ms <= READ_TIMEOUT_MS + 100)
    {
        printk("Sample window timeout must be greater than read timeout + 100ms\n");
        return false;
    }
    
    setTimeout(samples_window_timeout_ms);
    startTimeout();

    while (!isTimeoutExpired())
    {
        uint16_t distance = tof_read(true, thresholds); 
        if (!did_timeout && distance > 0 && Reading_data.range_status == VL53L1_RANGESTATUS_RANGE_VALID)
        {
            sum += distance;
            valid_samples++;
        }
    }

    if (valid_samples == 0){ return false; }
    *avg_mm = sum / valid_samples;

    return true;
}

bool tof_start_continuous(uint32_t period_ms)
{
    if (sensor_write_reg_u32(VL53L1X_ADDR, SYSTEM__INTERMEASUREMENT_PERIOD, period_ms * osc_calibrate_val) != I2C_OK){
        return false;
    }

    if (sensor_write_reg_u8(VL53L1X_ADDR, SYSTEM__INTERRUPT_CLEAR, 0x01) != I2C_OK ){
        return false;
    }
  
    return sensor_write_reg_u8(VL53L1X_ADDR, SYSTEM__MODE_START, 0x40) == I2C_OK;  
}

bool tof_set_distance_threshold_interrupt(uint16_t threshold_mm)
{
    if (sensor_write_reg_u16(VL53L1X_ADDR, SYSTEM__THRESH_LOW, threshold_mm) != I2C_OK) {
        return false;
    }

    if (sensor_write_reg_u16(VL53L1X_ADDR, SYSTEM__THRESH_HIGH, 0xFFFF) != I2C_OK) {
        return false;
    }

    if (sensor_write_reg_u8(VL53L1X_ADDR, SYSTEM__INTERRUPT_CONFIG_GPIO, 0x01) != I2C_OK) {
        return false;
    }

    return true;
}

