
#pragma once   /* include only once, avoids double-definition errors */
#include <zephyr/sys/printk.h>

/* ================= timing ================= */
#define IMU_PERIOD_MS  20          /*IMU in NORMAL / SIMU: every 20 ms (50 Hz) */
#define IMU_TEST_PERIOD_MS  2000   /* IMU in TEST: every 2 s */
#define ENV_PERIOD_MS  2000   /* environment thread: Si7021 + LDR every 2 s */

/* ================= MPU6050 ================= */
/* WHO_AM_I on our board: 0x68*/
/* Z offset (flip test): chip up Z = +8.00, chip down Z = -11.6
 *   -> offset = (8.00 - 11.6) / 2 = -1.80 */
#define MPU_ADDR       0x68       
#define ACC_Z_OFFSET   (-1.80f)    
#define CAL_LOOPS      100         

/* ================= Si7021 ================= */
#define SI_ADDR        0x40        /* I2C address (fixed) */

/* ================= LDR ================= */
/* measured on our board: lights off raw 3900, room 1070, phone torch 120 */
#define LDR_DARK       3900           /* raw value covered -> 0 %   */
#define LDR_BRIGHT     120        /* raw value torch   -> 100 %  */

/* ================= test prints ================= */
#define SENSOR_DEBUG   0           /* 1 = on, 0 = off (final version) */
#define SENSOR_DBG(...) do { if (SENSOR_DEBUG) printk(__VA_ARGS__); } while (0)