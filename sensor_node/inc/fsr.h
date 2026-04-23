#include <zephyr/kernel.h>

int config_adc();

int read_adc(int32_t* adc);

int get_voltage_trig(int32_t* voltage);

int get_voltage(int32_t* voltage);

int get_force(int32_t* force);