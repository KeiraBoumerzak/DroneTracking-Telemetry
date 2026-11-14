# Drone Tracker

The board reads the sensors (MPU6050, Si7021, LDR, GPS), finds if the drone
is idle, flying or has a problem, and sends the data in TEST, NORMAL or SIMU mode.

Threads: we use two, sensors (20 ms) and app (100 ms). The GPS uses an interrupt and a work queue.

## Kira
- MPU6050: read acceleration and rotation every 20 ms
- Si7021: temperature and humidity every 2 s
- LDR: light in %, with calibration
- Angles and position from the MPU6050 (util.c)
- Put all the values in the shared struct

## Adrian
- Button B1: change the mode
- GPS: read the position, the satellites and the fix
- App thread: decide IDLE / FLYING / PROBLEM, set the LED colour
- Output: TEST report, NORMAL report + stats, SIMU frame for the simulator