# See anatomy of a CMake file here: https://docs.zephyrproject.org/latest/develop/application/index.html#creating-an-application-by-hand
cmake_minimum_required(VERSION 3.13.1)

# Sets the variable BOARD, which tells build system which board to select if no board is specified
set(BOARD rpi_pico2/rp2350a/m33)

# Pulls in Zephyr build system
find_package(Zephyr)

# Defines application
project(Sender)

# Collects and adds sources to app target
file(GLOB SRC_FILES "src/*c")
target_sources(app PRIVATE ${SRC_FILES})
