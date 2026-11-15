/* sensors.c - reads the sensors every 20 ms
 * TODO:we read the MPU6050 every time, the Si7021 and the LDR every 2 s,
 * convert to real units, put the values in the shared data. */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "data_common.h"

void sensors_thread(void)
{
	int64_t wake_time = k_uptime_get();   /* time now, in ms */
	int loops = 0;                        /* how many times we ran */

	while (1) {
		wake_time = wake_time + 20;              /* next wake-up: 20 ms later */
		k_sleep(K_TIMEOUT_ABS_MS(wake_time));    /* sleep until that time */

		loops = loops + 1;

		if (loops % 50 == 0) {                   /* every 50 loops = every 1 second */
			DBG("tick %d time=%lld ms\n", loops,k_uptime_get());
		}
	}
}

/* start the thread: stack 2048, priority 2, starts 100 ms after boot */
K_THREAD_DEFINE(sensors_id, 2048, sensors_thread, NULL, NULL, NULL, 2, 0, 100);