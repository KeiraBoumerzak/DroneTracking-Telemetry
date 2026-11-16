
#pragma once   /* include only once, avoids double-definition errors */
#include <zephyr/sys/printk.h>

/* ================= timing ================= */
#define IMU_PERIOD_MS  20          /* sensors thread: every 20 ms (50 Hz) */

/* ================= MPU6050 ================= */
/* WHO_AM_I on our board: 0x68*/
#define MPU_ADDR       0x68       
#define ACC_Z_OFFSET   (-1.80f)    
#define CAL_LOOPS      100         

/* ================= Si7021 ================= */
#define SI_ADDR        0x40        /* I2C address (fixed) */
#define ENV_LOOPS      100          /* environment (temp, hum, light): read every 100 loops = 2 s */

/* ================= LDR ================= */
#define LDR_DARK       3900           /* raw value covered -> 0 %   */
#define LDR_BRIGHT     120        /* raw value torch   -> 100 %  */

/* ================= test prints ================= */
#define SENSOR_DEBUG   1           /* 1 = on, 0 = off (final version) */
#define SENSOR_DBG(...) do { if (SENSOR_DEBUG) printk(__VA_ARGS__); } while (0)