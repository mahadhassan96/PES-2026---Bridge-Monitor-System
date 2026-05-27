cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(base_station_test)

target_sources(app PRIVATE
    src/test_main.c
    ../../base_station/src/base_station.c
    ../../coms/coms.c
    ../../base_station/drivers/sensor_node/sensor_node.c
)

target_include_directories(app PRIVATE
    ../../include
    ../../base_station
)