#ifndef BASE_STATION_H
#define BASE_STATION_H

#include "stdint.h"
#include "readings.h"

#define SLEEP_DURATION_MS 1000

typedef enum
{
    NORMAL,
    LOGGING,
    ALERT,
    BOOT,
    ERROR
} base_station_state_t;

typedef struct 
{
    sensor_reading_t sensor_readings;
    sensor_states_t sensor_states;
    base_station_state_t curr_state;
    bool anomaly_detected;
    bool logging;
    bool boot_complete;
} base_station_t;

void init(base_station_t* bs);
void update(base_station_t* bs);

void turn_on_leds();
void turn_off_leds();

void print_data();
void get_all_data(base_station_t* bs);
void sensor_handshake();
void init_sensors();

void reset_system_cb();
void update_logging_cb();
void alert_cb();
void get_fresh_data_cb();

void process_packet();

#endif /* BASE_STATION_H */