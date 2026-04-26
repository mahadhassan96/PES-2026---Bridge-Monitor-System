#define DT_DRV_COMPAT fsr_4xx
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/sensor.h>

#ifndef SENSOR_CHAN_FORCE
#define SENSOR_CHAN_FORCE SENSOR_CHAN_PRIV_START
#endif

#define RM 10000
#define SUPPLY_V 3300

LOG_MODULE_REGISTER(FSR_4XX, CONFIG_SENSOR_LOG_LEVEL);

typedef struct FvR
{
    uint32_t resistance;
    uint16_t force;
} FvR;

static const FvR fvr[10] = {
	{250, 10000},{300, 7000},{450, 4000},{750, 2000},{1200, 1000},
	{2000, 500},{3500, 250},{6000, 100},{10000, 50},{30000, 20}
};

static int32_t find_force(int32_t resistance){
	int max = 9;
	int min = 0;
	int counter = 0;
	int mid;
	while(min <= max){
		mid = min + (max - min) / 2;
		if(resistance < fvr[mid].resistance){
			max = mid - 1;
		}
		if(resistance > fvr[mid].resistance){
			min = mid + 1;
		}
		if((resistance == fvr[mid].resistance) || (counter >= 10)){
			return mid;
		}
		counter++;
	}
	return fvr[mid].force;
}

/**	-------------------- FSR 4XX API	-------------------------------			 */

struct fsr_4xx_data {
    uint16_t raw;      /* Buffer for ADC reading */
};

struct fsr_4xx_config {
	const struct device *adc;
	uint8_t adc_channel;
	struct adc_sequence adc_seq;
	struct adc_channel_cfg ch_cfg;
};

static int fsr_4xx_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	if (chan != SENSOR_CHAN_FORCE && chan != SENSOR_CHAN_ALL) {
		return -ENOTSUP;
	}

    const struct fsr_4xx_config *cfg = dev->config;

    /* Start the ADC conversion */
    return adc_read(cfg->adc, &cfg->adc_seq);
}

static int fsr_4xx_channel_get(const struct device *dev, enum sensor_channel chan,
                               struct sensor_value *val)
{
    if (chan != SENSOR_CHAN_FORCE && chan != SENSOR_CHAN_ALL) {
        return -ENOTSUP;
    }

	int err;
	struct fsr_4xx_data *data = dev->data;
	const struct fsr_4xx_config *cfg = dev->config;
	int32_t mv = data->raw;

	err = adc_raw_to_millivolts(adc_ref_internal(cfg->adc), cfg->ch_cfg.gain,
				    cfg->adc_seq.resolution, &mv);
	if (err) {
		return err;
	}

	int32_t resistance = abs(((RM * mv) / SUPPLY_V) - RM);
	int32_t force = find_force(resistance);
    val->val1 = force;
    val->val2 = 0;

    return 0;
}

static int fsr_4xx_init(const struct device *dev){
    const struct fsr_4xx_config *cfg = dev->config;
		if (!device_is_ready(cfg->adc)) {
		LOG_ERR("ADC device is not ready.");
		return -EINVAL;
	}
	return adc_channel_setup(cfg->adc, &cfg->ch_cfg);
}


static const struct sensor_driver_api fsr_4xx_api = {
    .sample_fetch = fsr_4xx_sample_fetch,
    .channel_get = fsr_4xx_channel_get,
};

#define FSR_4XX_DEFINE(inst)                                        \
    static struct fsr_4xx_data fsr_4xx_data_##inst;                 \
                                                                    \
    static const struct fsr_4xx_config fsr_4xx_cfg_##inst = {    \
        .adc = DEVICE_DT_GET(DT_INST_IO_CHANNELS_CTLR(inst)),		\
		.adc_channel = DT_INST_IO_CHANNELS_INPUT(inst),				\
		.adc_seq =													\
			{														\
				.channels = BIT(DT_INST_IO_CHANNELS_INPUT(inst)),	\
				.buffer = &fsr_4xx_data_##inst.raw,					\
				.buffer_size = sizeof(fsr_4xx_data_##inst.raw),		\
				.resolution = DT_INST_PROP(inst, resolution),		\
			},   													\
		.ch_cfg = 													\
			{														\
			.gain = ADC_GAIN_1,										\
			.reference = ADC_REF_INTERNAL,									\
			.acquisition_time = ADC_ACQ_TIME_DEFAULT,				\
			.channel_id = DT_INST_IO_CHANNELS_INPUT(inst),			\
			}														\
	};                                                              \
                                                                    \
    SENSOR_DEVICE_DT_INST_DEFINE(inst,                              \
                          fsr_4xx_init,                             \
                          NULL,                                     \
                          &fsr_4xx_data_##inst,                     \
                          &fsr_4xx_cfg_##inst,                   \
                          POST_KERNEL,                              \
                          CONFIG_SENSOR_INIT_PRIORITY,              \
                          &fsr_4xx_api);

DT_INST_FOREACH_STATUS_OKAY(FSR_4XX_DEFINE)