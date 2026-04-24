#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include "fsr.h"

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

#define RM 10000
#define SUPPLY_V 3300

static uint16_t buf;   
typedef struct FvR
{
    uint32_t resistance;
    uint16_t force;
} FvR;

static const FvR fvr[10] = {
								{250, 10000},
								{300, 7000},
								{450, 4000},
								{750, 2000},
								{1200, 1000},
								{2000, 500},
								{3500, 250},
								{6000, 100},
								{10000, 50},
								{30000, 20},
};

static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
			     DT_SPEC_AND_COMMA)
};

struct adc_sequence sequence = {
	.buffer = &buf,
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

/*
	Reads ADC Code, returns error status
*/
int read_adc(int32_t* adc){
    (void)adc_sequence_init_dt(&adc_channels[0], &sequence);
	int err = adc_read_dt(&adc_channels[0], &sequence);
	if (err < 0) {
		return -1;
	}
	*adc = buf;
    return 0;
}

int get_voltage_trig(int32_t* voltage){
	read_adc(voltage);
    int err = adc_raw_to_millivolts_dt(&adc_channels[0], voltage);

	if ((err < 0) || (*voltage > SUPPLY_V)) {
		return -1;
	}

	return 0;
}

int get_voltage(int32_t* voltage){
    int err = adc_raw_to_millivolts_dt(&adc_channels[0], voltage);

	if ((err < 0) || (*voltage > SUPPLY_V)) {
		return -1;
	}

	return 0;
}

int find_fvr_index(int32_t resistance){
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
	return mid;
}

int get_force(int32_t* force, int32_t voltage_mv){
	uint32_t resistance = abs(((RM * voltage_mv) / SUPPLY_V) - RM); //((voltage / SUPPLY_V) - 1) * RM;
	printk("Resistance: %d\n", resistance);
	int index = find_fvr_index(resistance);
	if(abs(resistance - fvr[index].resistance) >= abs(resistance - fvr[index-1].resistance)){
		*force = fvr[index-1].force;
		return 0;
	}
	*force = fvr[index].force;
	return 0;
}

int get_force_trig(int32_t* force){
	int32_t voltage_mv = 0; 

	int err = get_voltage_trig(&voltage_mv);
	if(err < 0){
		return err;
	}	

	uint32_t resistance = abs(((RM * voltage_mv) / SUPPLY_V) - RM); //((voltage / SUPPLY_V) - 1) * RM;
	printk("Resistance: %d\n", resistance);
	int index = find_fvr_index(resistance);
	if(abs(resistance - fvr[index].resistance) >= abs(resistance - fvr[index-1].resistance)){
		*force = fvr[index].force;
		return 0;
	}
	*force = fvr[index].force;

	return 0;
}