/* app.c - runs every 100 ms.
 * TODO: we read the shared data, decide the state, set the LED,
 * print the output for TEST / NORMAL / SIMU.
 * In SIMU only the frame is printed. */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "common.h"