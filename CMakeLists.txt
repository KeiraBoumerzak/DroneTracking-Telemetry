cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(drone_tracker)

target_sources(app PRIVATE
    src/main.c
    src/sensor/sensors.c
    src/sensor/gps.c
    src/control/app.c
    src/control/util.c
    src/control/data_tx.c
    src/actuator/rgb.c
    src/control/button_handler.c
)

target_include_directories(app PRIVATE include)