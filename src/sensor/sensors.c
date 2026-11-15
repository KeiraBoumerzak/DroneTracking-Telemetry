#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/i2c.h>                 
#include "sensors.h"

#define MPU_ADDR  0x68                         

/* the I2C bus, from the overlay alias "sensor-i2c" */
const struct device *i2c = DEVICE_DT_GET(DT_ALIAS(sensor_i2c));

/* check the MPU6050 answers. Returns true if found. */
bool imu_check(void)
{
	uint8_t id = 0;

	/* read 1 byte from register 0x75 (WHO_AM_I) */
	if (i2c_reg_read_byte(i2c, MPU_ADDR, 0x75, &id) != 0) {
		SENSOR_DBG("M
            PU6050: no answer on I2C\n");
		return false;
	}
	SENSOR_DBG("MPU6050 WHO_AM_I = 0x%02X\n", id);
	return id == 0x68;                          
}

/* write 5 registers: wake up, filter, sample rate, gyro range, accel range */
void imu_setup(void)
{
	i2c_reg_write_byte(i2c, MPU_ADDR, 0x6B, 0x01);  /* power: wake up, gyro clock */
	k_msleep(10);                                    /* give it time to wake up */
	i2c_reg_write_byte(i2c, MPU_ADDR, 0x1A, 0x04);  /* filter: about 21 Hz */
	i2c_reg_write_byte(i2c, MPU_ADDR, 0x19, 19);    /* sample rate: 1000/(1+19) = 50 Hz */
	i2c_reg_write_byte(i2c, MPU_ADDR, 0x1B, 0x08);  /* gyro range: +-500 deg/s */
	i2c_reg_write_byte(i2c, MPU_ADDR, 0x1C, 0x08);  /* accel range: +-4 g */
}

/* read acceleration (m/s2) and rotation (deg/s). Returns true if OK. */
bool imu_read(float acc[3], float gyro[3])
{
	uint8_t reg = 0x3B;      /* first data register */
	uint8_t raw[14];         /* 14 bytes: accel x,y,z / temp / gyro x,y,z */

	if (i2c_write_read(i2c, MPU_ADDR, &reg, 1, raw, 14) != 0) {
		return false;        /* read failed */
	}
	for (int i = 0; i < 3; i++) {
		int16_t a = (raw[2 * i] << 8) | raw[2 * i + 1];      /* bytes 0-5 */
		int16_t g = (raw[8 + 2 * i] << 8) | raw[9 + 2 * i];  /* bytes 8-13 */
		acc[i]  = a / 8192.0f * 9.81f;   /* +-4 g: 8192 per g */
		gyro[i] = g / 65.5f;             /* +-500 deg/s: 65.5 per deg/s */
	}
	return true;
}

void sensors_thread(void)
{
	int64_t wake_time = k_uptime_get();   /* time now, in ms */
	int loops = 0;                        /* how many times we ran */
	float acc[3] = {0}, gyro[3] = {0};    /* latest values */

	if (!device_is_ready(i2c)) {          /* is the I2C bus working? */
		SENSOR_DBG("I2C bus not ready\n");
	}
	bool imu_ok = imu_check();            /* is the MPU6050 there? */
	if (imu_ok) {
		imu_setup();
	}

	while (1) {
		wake_time = wake_time + 20;              /* next wake-up: 20 ms later */
		k_sleep(K_TIMEOUT_ABS_MS(wake_time));    /* sleep until that time */
		loops = loops + 1;

		if (imu_ok) {
			imu_ok = imu_read(acc, gyro);
		}

		if (loops % 50 == 0) {                   /* every 1 s */
			SENSOR_DBG("\n---------------- IMU ----------------\n");
			SENSOR_DBG(" acc  [m/s2]   X: %+07.2f | Y: %+07.2f | Z: %+07.2f\n",
			    (double)acc[0], (double)acc[1], (double)acc[2]);
			SENSOR_DBG(" gyro [deg/s]  X: %+08.1f | Y: %+08.1f | Z: %+08.1f\n",
			    (double)gyro[0], (double)gyro[1], (double)gyro[2]);
		}
	}   /* end of while */
}       /* end of sensors_thread */

/* start the thread: stack 2048, priority 2, starts 100 ms after boot */
K_THREAD_DEFINE(sensors_id, 2048, sensors_thread, NULL, NULL, NULL, 2, 0, 100);