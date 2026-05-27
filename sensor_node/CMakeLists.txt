cmake_minimum_required(VERSION 3.20.0)

set(KCONFIG_ERROR_ON_WARNING OFF CACHE BOOL "" FORCE)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND ZEPHYR_LIBRARY_EXTRA_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/inc)

# 2. Load Zephyr
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_node)

# Add source files
FILE(GLOB SRC_FILES "src/*.c" "ISR/*.c")

target_sources(app PRIVATE 
    ${SRC_FILES}
    src/sensor_node.c
    ../coms/coms.c
)

target_include_directories(app PRIVATE
    ../include
)