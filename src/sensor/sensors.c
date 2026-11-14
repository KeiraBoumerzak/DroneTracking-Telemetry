/* sensors.c - runs every 20 ms.
 * TODO:we read the MPU6050 every time, the Si7021 and the LDR every 2 s,
 * convert to real units, put the values in the shared data. */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "data_common.h"
