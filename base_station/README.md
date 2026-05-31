## Overview
The base station operates an internal state machine that is driven by an event queue. Specific actions place event objects into the queue which update the state accordingly. 

The base station communicates with the sensor node via a UART connection, which also queues messages to be processed.

## Zephyr Build Tree Note
Currently the project must be built inside of the `zephyrproject` build tree. After pulling the latest zephyr project, I cloned the repo to `~/zephyrproject/zephyr/PES-2026--Bridge-Monitor-System` to build in-tree.

## Configure Python Virtual Environment
Setup a zephyr virtual environment, on Mac this looks like:
```bash
    source ~/zephyrproject/.venv/bin/activate
```

## Build Instructions
Once the following requirements have been met, build the base station with:
```bash
    cd /path/to/repo/
    west build -b rpi_pico2/rp2350a/m33 base_station -p
```

*NOTE:* emitting the `-p` flag stops a pristine build

## Copy .uf2 file to Raspberry Pi Pico
Ensure the Pico is in bootloader mode and identify where the device mounts on your computer. On Mac this is at or around `Volumes/RP2350`. Copy the fresh build .uf2 over to the Pico with:
```bash
    cp build/zephyr/zephyr.uf2 /Volumes/RP2350
```

## USB Monitoring
Identify the device in `/dev/tty/` and connect to the Pico via `screen` or a similar serial port monitor with:
```bash
    ls /dev/tty.*
    screen /dev/tty.<device_name> 115200
```

## Pin Assignments
As of now, the following pins are assigned on the base station Pico:
| Pin       | Type     | Usage                  |
|-----------|----------|------------------------|
| GP20      | btn      | Reset                  |
| GP21      | btn      | Emergency ack          |
| GP22      | btn      | Config sensitivity     |
|-----------|----------|------------------------|

## Testing
To run base station tests, build test suite .uf2 with:
```bash
cd /path/to/repo/
rm -rf build/pico-test
west build -b rpi_pico2/rp2350a/m33 testing/base_station
```
Then copy the .uf2 file to the base station pico (in BOOTSEL mode) with
```bash
    cp build/zephyr/zephyr.uf2 /Volumes/RP2350
```
Inspect the result of the tests in the serial monitor, following the steps above.