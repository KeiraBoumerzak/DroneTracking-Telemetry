# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blink_basic)

target_sources(app PRIVATE
    src/main.c
    src/sensor/ldr.c
    src/actuator/rgb.c
    src/actuator/button_handler.c
    src/control/brightness_thread.c
    src/control/fsm.c
)

target_include_directories(app PRIVATE headers)
