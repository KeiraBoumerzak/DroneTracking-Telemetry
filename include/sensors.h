#pragma once                      /* read this file only once */
#include <zephyr/sys/printk.h>

/* test prints: 1 = on, 0 = off (final version) */
#define SENSOR_DEBUG 1

/* print only when SENSOR_DEBUG is 1 */
#define SENSOR_DBG(...) do { if (SENSOR_DEBUG) printk(__VA_ARGS__); } while (0)