cmake_minimum_required(VERSION 3.20.0)

# 1. Bypass the Kconfig "Aborting" behavior
set(KCONFIG_ERROR_ON_WARNING OFF CACHE BOOL "" FORCE)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# 2. Load Zephyr
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_node)

# 3. MANUALLY add your drivers folder
# This tells CMake to go into 'drivers/' and look for another CMakeLists.txt
#add_subdirectory(drivers)

# For using west flash with picoprobe
set(OPENOCD "openocd")
set(OPENOCD_DEFAULT_PATH "/usr/local/share/openocd/scripts")

#define project name
project(lab2_part1)

# Add source files
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
FILE(GLOB SRC_FILES "src/*.c")
target_sources(app PRIVATE src/main.c ${SRC_FILES})

# 4. Standard App Source Settings
target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE src inc)
