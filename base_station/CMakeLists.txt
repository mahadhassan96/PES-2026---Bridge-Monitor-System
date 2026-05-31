cmake_minimum_required(VERSION 3.20.0)
set(BOARD rpi_pico2/rp2350a/m33)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lab2_part1)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(OPENOCD "openocd")
set(OPENOCD_DEFAULT_PATH "/usr/local/share/openocd/scripts")
target_include_directories(app PRIVATE src/inc .)
FILE(GLOB SRC_FILES "src/*.c")
target_sources(app PRIVATE 
    ${SRC_FILES}
    drivers/sensor_node/sensor_node.c
    src/base_station.c
    ../coms/coms.c
)

target_include_directories(app PRIVATE
    ../include
    drivers/sensor_node
)