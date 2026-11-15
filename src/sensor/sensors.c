#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include "sensors.h"

/* the I2C bus, from the overlay alias "sensor-i2c" */
const struct device *i2c = DEVICE_DT_GET(DT_ALIAS(sensor_i2c));

float gyro_bias[3] = {0, 0, 0};   /* gyro offset, measured at start-up */

/* check the MPU6050 answers. Returns true if found. */
bool imu_check(void)
{
	uint8_t id = 0;

	/* read 1 byte from register 0x75 (WHO_AM_I) */
	if (i2c_reg_read_byte(i2c, MPU_ADDR, 0x75, &id) != 0) {
		SENSOR_DBG("MPU6050: no answer on I2C\n");
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
	k_msleep(50);   /* tested: sometimes the first read right after setup
	                   returned only zeros (sample not ready yet), so the
	                   sensor was set up twice. Waiting 50 ms fixes it. */
}

/* read acceleration (m/s2) and rotation (deg/s). Returns true if OK. */
bool imu_read(float acc[3], float gyro[3])
{
	uint8_t reg = 0x3B;      /* first data register */
	uint8_t raw[14];         /* 14 bytes: accel x,y,z / temp / gyro x,y,z */

	if (i2c_write_read(i2c, MPU_ADDR, &reg, 1, raw, 14) != 0) {
		return false;        /* read failed */
	}

	/* tested: when a wire moved, the MPU6050 reset and answered only zeros */
	bool all_zero = true;
	for (int i = 0; i < 14; i++) {
		if (raw[i] != 0) {
			all_zero = false;
		}
	}
	if (all_zero) {
		return false;
	}

	for (int i = 0; i < 3; i++) {
		int16_t a = (raw[2 * i] << 8) | raw[2 * i + 1];      /* bytes 0-5 */
		int16_t g = (raw[8 + 2 * i] << 8) | raw[9 + 2 * i];  /* bytes 8-13 */
		acc[i]  = a / 8192.0f * 9.81f;   /* +-4 g: 8192 per g */
		gyro[i] = g / 65.5f;             /* +-500 deg/s: 65.5 per deg/s */
	}
	acc[2] = acc[2] - ACC_Z_OFFSET;      /* remove the measured Z offset */
	return true;
}

void sensors_thread(void)
{
	int64_t wake_time = k_uptime_get();   /* time now, in ms */
	int loops = 0;                        /* how many times we ran */
	int cal_count = 0;                    /* samples used for gyro calibration */
	float acc[3] = {0}, gyro[3] = {0};    /* latest values */

	if (!device_is_ready(i2c)) {          /* is the I2C bus working? */
		SENSOR_DBG("I2C bus not ready\n");
	}
	bool imu_ok = imu_check();            /* is the MPU6050 there? */
	if (imu_ok) {
		imu_setup();
	}

	while (1) {
		wake_time = wake_time + IMU_PERIOD_MS;   /* next wake-up: 20 ms later */
		k_sleep(K_TIMEOUT_ABS_MS(wake_time));    /* sleep until that time */
		loops = loops + 1;

		/* MPU lost? try again once per second */
		if (!imu_ok && loops % 50 == 0) {
			imu_ok = imu_check();
			if (imu_ok) {
				imu_setup();
				cal_count = 0;                   /* sensor restarted: calibrate again */
				for (int i = 0; i < 3; i++) {
					gyro_bias[i] = 0;
				}
			}
		}

		if (imu_ok) {
			imu_ok = imu_read(acc, gyro);
		}

		if (imu_ok) {
			if (cal_count < CAL_LOOPS) {
				/* first 2 s: board still, average the gyro */
				for (int i = 0; i < 3; i++) {
					gyro_bias[i] = gyro_bias[i] + gyro[i] / CAL_LOOPS;
				}
				cal_count = cal_count + 1;
				if (cal_count == CAL_LOOPS) {
					SENSOR_DBG("gyro calibrated: %+.2f %+.2f %+.2f deg/s\n",
					           (double)gyro_bias[0], (double)gyro_bias[1],
					           (double)gyro_bias[2]);
				}
			}
			if (cal_count >= CAL_LOOPS) {
				/* calibration done: remove the gyro offset */
				for (int i = 0; i < 3; i++) {
					gyro[i] = gyro[i] - gyro_bias[i];
				}
			}
		}   /* end of if (imu_ok) */

		if (loops % 50 == 0) {                   /* every 1 s */
			if (!imu_ok) {
				SENSOR_DBG("IMU: not working, retrying...\n");
			} else if (cal_count >= CAL_LOOPS) {  /* print only after calibration */
				SENSOR_DBG("\n---------------- IMU ----------------\n");
				SENSOR_DBG(" acc  [m/s2]   X: %+07.2f | Y: %+07.2f | Z: %+07.2f\n",
				           (double)acc[0], (double)acc[1], (double)acc[2]);
				SENSOR_DBG(" gyro [deg/s]  X: %+08.1f | Y: %+08.1f | Z: %+08.1f\n",
				           (double)gyro[0], (double)gyro[1], (double)gyro[2]);
			}
		}
	}   
}       

/* start the thread: stack 2048, priority 2, starts 100 ms after boot */
K_THREAD_DEFINE(sensors_id, 2048, sensors_thread, NULL, NULL, NULL, 2, 0, 100);
