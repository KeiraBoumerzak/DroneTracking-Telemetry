/* sensors.c - reads the sensors every 20 ms
 * TODO:we read the MPU6050 every time, the Si7021 and the LDR every 2 s,
 * convert to real units, put the values in the shared data. */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/i2c.h>                 /* I2C functions */
#include "data_common.h"

#define MPU_ADDR  0x68                          /*MPU6050 I2C address */

/*  the I2C bus, from the overlay alias "sensor-i2c" */
const struct device *i2c = DEVICE_DT_GET(DT_ALIAS(sensor_i2c));

/* check the MPU6050 answers. Returns true if found. */
bool imu_check(void)
{
	uint8_t id = 0;

	/* read 1 byte from register 0x75 (WHO_AM_I) */
	if (i2c_reg_read_byte(i2c, MPU_ADDR, 0x75, &id) != 0) {
		DBG("MPU6050: no answer on I2C\n");
		return false;
	}
	DBG("MPU6050 WHO_AM_I = 0x%02X\n", id);
	return id == 0x68;                          /* 0x68 = it's an MPU6050 */
}
void sensors_thread(void)
{
	int64_t wake_time = k_uptime_get();   /* time now, in ms */
	int loops = 0;                        /* how many times we ran */
    if (!device_is_ready(i2c)) {          /* is the I2C bus working? */
		DBG("I2C bus not ready\n");
	}
	bool imu_ok = imu_check();            /* is the MPU6050 there? */

	while (1) {
		wake_time = wake_time + 20;              /*  next wake-up: 20 ms later */
		k_sleep(K_TIMEOUT_ABS_MS(wake_time));    /* sleep until that time */

		loops = loops + 1;

		if (loops % 50 == 0) {                   /* every 50 loops = every 1 second */
			DBG("tick %d time=%lld ms  imu_ok=%d\n", loops,k_uptime_get(),imu_ok);
		}
	}
}

/* start the thread: stack 2048, priority 2, starts 100 ms after boot */
K_THREAD_DEFINE(sensors_id, 2048, sensors_thread, NULL, NULL, NULL, 2, 0, 100);