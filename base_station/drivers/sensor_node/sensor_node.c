#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/device.h>
#include "sensor_node.h"
#include "../../../include/coms.h"
#include "../../../include/base_station.h"

/* Semaphore: packet_handler_task gives it, fetch takes it */
K_SEM_DEFINE(response_sem, 0, 1);

/* Single snapshot of latest sensor data — overwritten on every fetch */
static sensor_node_data_t driver_data = {
    .dist            = 0,
    .force           = 0,
    .accel_x         = 0,
    .accel_y         = 0,
    .accel_z         = 0,
    .last_fetch_time = 0,
};

void sensor_node_store_response(int dist, int force, int accel_x, int accel_y, int accel_z)
{
    driver_data.dist            = dist;
    driver_data.force           = force;
    driver_data.accel_x         = accel_x;
    driver_data.accel_y         = accel_y;
    driver_data.accel_z         = accel_z;
    driver_data.last_fetch_time = k_uptime_get();
    k_sem_give(&response_sem);
}

/* ── Zephyr Sensor API: fetch ───────────────────────────────────────────── */
static int sensor_node_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    /* Send REQUEST packet over UART */
    request_data();

    /* Block until packet_handler_task signals response is ready */
    int ret = k_sem_take(&response_sem, K_MSEC(2000));
    if (ret != 0) {
        printk("[DRIVER] fetch timeout — no response from sensor node\n");
        return -ETIMEDOUT;
    }

    return 0;
}

/* ── Zephyr Sensor API: get ─────────────────────────────────────────────── */
static int sensor_node_channel_get(const struct device *dev,
                                   enum sensor_channel chan,
                                   struct sensor_value *val)
{
    if (driver_data.last_fetch_time == 0) {
        printk("[DRIVER] get called before fetch\n");
        return -ENODATA;
    }

    switch (chan) {
    case SENSOR_CHAN_DISTANCE:
        val->val1 = driver_data.dist;
        val->val2 = 0;
        break;

    case SENSOR_CHAN_FORCE_N:
        val->val1 = driver_data.force;
        val->val2 = 0;
        break;

    case SENSOR_CHAN_ACCEL_X:
        val->val1 = driver_data.accel_x;
        val->val2 = 0;
        break;

    case SENSOR_CHAN_ACCEL_Y:
        val->val1 = driver_data.accel_y;
        val->val2 = 0;
        break;

    case SENSOR_CHAN_ACCEL_Z:
        val->val1 = driver_data.accel_z;
        val->val2 = 0;
        break;

    default:
        return -ENOTSUP;
    }

    return 0;
}

/* ── Driver API struct — plugs your functions into Zephyr's interface ───── */
static const struct sensor_driver_api sensor_node_api = {
    .sample_fetch = sensor_node_sample_fetch,
    .channel_get  = sensor_node_channel_get,
};

/* ── Init function — called once at boot by Zephyr ─────────────────────── */
static int sensor_node_init(const struct device *dev)
{
    printk("[DRIVER] sensor_node initialized\n");
    return 0;
}

/* ── Register driver as a Zephyr device ─────────────────────────────────── */
DEVICE_DEFINE(sensor_node_sensor,           /* name used in DEVICE_GET */
              "SN_sensor",         /* string name              */
              sensor_node_init,      /* init function            */
              NULL,                /* power management         */
              &driver_data,        /* our data struct          */
              NULL,                /* config (none)            */
              POST_KERNEL,         /* init level               */
              CONFIG_SENSOR_INIT_PRIORITY,
              &sensor_node_api);     /* our API struct           */
