cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_node_test)

target_sources(app PRIVATE 
    ${SRC_FILES}
    src/test_main.c
    ../../sensor_node/src/sensor_node.c
    ../../coms/coms.c
    # ../../sensor_node/drivers/adxl_313/adxl_313.c
    # ../../sensor_node/drivers/fsr_4xx/fsr_4xx.c
    # ../../sensor_node/drivers/vl53l1x/vl53l1x.c
)

target_include_directories(app PRIVATE
    ../../include
)