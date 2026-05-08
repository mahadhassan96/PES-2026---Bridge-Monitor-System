cmake_minimum_required(VERSION 3.20.0)

set(KCONFIG_ERROR_ON_WARNING OFF CACHE BOOL "" FORCE)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_node)

set(OPENOCD "openocd")
set(OPENOCD_DEFAULT_PATH "/usr/local/share/openocd/scripts")

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

file(GLOB SRC_FILES
    "src/*.c"
    "drivers/tof/*.c"
    "ISR/*.c"
)

target_sources(app PRIVATE ${SRC_FILES})