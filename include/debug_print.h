#pragma once

#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#define BS_TAG "BASE_STATION"
#define SN_TAG "SENSOR_NODE"
#define DRVR_TAG "DRIVER"
#define PACKET_TAG "PACKET"

#define ERROR_TAG "ERROR"
#define DEBUG_TAG "DEBUG"

/*
 * Usage:
 *
 * APP_PRINT("BS", "DEBUG", "hello %d\n", x);
 * APP_PRINT("SN", "ERROR", "bad sensor read\n");
 */

#if IS_ENABLED(CONFIG_ZTEST)

#define APP_PRINT(module, level, fmt, ...)

#else

#define APP_PRINT(module, level, fmt, ...) \
    printk("[%s::%s] " fmt "\n", module, level, ##__VA_ARGS__)

#endif

#if IS_ENABLED(CONFIG_ZTEST)

#define PACKET_PRINT(fmt, ...)

#else

#define PACKET_PRINT(fmt, ...) \
    printk(fmt, ##__VA_ARGS__);
#endif