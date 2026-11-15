#pragma once   /* include only once, avoids double-definition errors */

#include <zephyr/sys/printk.h>

/* timing */
#define IMU_PERIOD_MS  20          /* sensors thread: every 20 ms (50 Hz) */

/* MPU6050 (WHO_AM_I read on our board: 0x68) */
#define MPU_ADDR       0x68        /* I2C address */

/* Z offset, measured with a flip test on our sensor:
 * chip up Z = +8.00, chip down Z = -11.6
 * offset = (8.00 + (-11.6)) / 2 = -1.80
 * after correction: flat chip up reads 9.6 - 9.8 m/s2 */
#define ACC_Z_OFFSET   (-1.80f)

/* gyro offset is measured at every start-up (board still for 2 s).
 * Our board, several runs (deg/s): X -2.5 to -2.8, Y -0.7 to -1.7, Z -1.7 to +0.6
 * (it drifts a little with warm-up, that's why we measure it each time) */
#define CAL_LOOPS      100         /* 100 x 20 ms = 2 s of gyro calibration */

/* test prints: 1 = on, 0 = off (final version) */
#define SENSOR_DEBUG   1
#define SENSOR_DBG(...) do { if (SENSOR_DEBUG) printk(__VA_ARGS__); } while (0)