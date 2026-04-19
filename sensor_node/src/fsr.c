#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include "fsr.h"

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

static uint16_t buf;    
static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
			     DT_SPEC_AND_COMMA)
};

struct adc_sequence sequence = {
	.buffer = &buf,
	/* buffer size in bytes, not number of samples */
	.buffer_size = sizeof(buf),
};

int config_adc(){
    /* Configure channels individually prior to sampling. */
	for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		if (!adc_is_ready_dt(&adc_channels[i])) {
			return -1;
		}
		adc_channel_setup_dt(&adc_channels[i]);
	}
	return 0;
}

int read_adc(int32_t* val_mv){
    (void)adc_sequence_init_dt(&adc_channels[0], &sequence);
	int err = adc_read_dt(&adc_channels[0], &sequence);
	if (err < 0) {
		return -1;
	}
    int32_t adc_res = (int32_t)buf;
    err = adc_raw_to_millivolts_dt(&adc_channels[0], &adc_res);

	if (err < 0) {
		return -1;
	}
	*val_mv = adc_res;
	return 0;
}