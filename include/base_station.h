#ifndef BASE_STATION_H
#define BASE_STATION_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "readings.h"

#define WORK_INTERVAL_S 5

#define STACK_SIZE 500
#define QUEUE_SIZE 16
#define UPDATE_PRIO -1
#define WORKER_PRIO 1

typedef enum
{
    NORMAL,
    ALERT,
    BOOT,
    ERROR
} base_station_state_t;

typedef enum
{
    RESET_PRESSED,
    LOGGING_PRESSED,
    BOOT_COMPLETE,
    ANOMALY_DETECTED,
    ANOMALY_CLEARED,
    ERROR_OCCURRED
} base_station_event_t;

typedef enum {
    STATE_SIGNAL,
    TIMER_SIGNAL
} poll_signal_t;

typedef struct 
{
    sensor_reading_t sensor_readings;
    sensor_states_t sensor_states;
    base_station_state_t curr_state;
    bool logging;
} base_station_t;

void init(base_station_t* bs);

void turn_on_leds(bool force, bool dist, bool accel);
void turn_off_leds();

void normal_handler(base_station_t *bs, bool state_change);
void alert_handler(base_station_t *bs, bool state_change);
void boot_handler(base_station_t *bs, bool state_change);
void error_handler(base_station_t *bs, bool state_change);

void print_data();
void request_data();
void sensor_handshake();
void init_sensors();

void reset_system_cb();
void update_logging_cb();
void alert_cb();
void get_fresh_data_cb();

void reset_btn_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void logging_btn_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void init_btn(const struct gpio_dt_spec *spec, gpio_callback_handler_t callback, struct gpio_callback *callback_data);

void timer_handler(struct k_timer *t);

base_station_event_t get_next_state(base_station_state_t curr_state, base_station_event_t ev);

void log_event(base_station_t* bs, base_station_event_t ev);
void log_state(base_station_t* bs);

void process_packet();

#endif /* BASE_STATION_H */