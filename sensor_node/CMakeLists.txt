cmake_minimum_required(VERSION 3.20.0)

# 1. Bypass the Kconfig "Aborting" behavior
set(KCONFIG_ERROR_ON_WARNING OFF CACHE BOOL "" FORCE)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND ZEPHYR_LIBRARY_EXTRA_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/inc)

# 2. Load Zephyr
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_node)

# 3. MANUALLY add your drivers folder
# This tells CMake to go into 'drivers/' and look for another CMakeLists.txt
#add_subdirectory(drivers)

# Add source files
FILE(GLOB SRC_FILES "src/*.c")
target_sources(app PRIVATE ../coms/coms.c ${SRC_FILES})
target_include_directories(app PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR}/inc)