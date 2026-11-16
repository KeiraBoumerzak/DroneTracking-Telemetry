#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include "sensors.h"
#include "app.h"   

/* the I2C bus, from the overlay alias "sensor-i2c" (MPU6050 + Si7021) */
const struct device *i2c = DEVICE_DT_GET(DT_ALIAS(sensor_i2c));

float gyro_bias[3] = {0, 0, 0};   /* gyro offset, measured at start-up */

/*                          MPU6050                              */
/* ============================================================ */
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
	i2c_reg_write_byte(i2c, MPU_ADDR, 0x6B, 0x01);  /* 0x6B power: wake up, gyro clock */
	k_msleep(10);                                    /* wait for wake up */
	i2c_reg_write_byte(i2c, MPU_ADDR, 0x1A, 0x04);  /* 0x1A CONFIG:filter: about 21 Hz */
	i2c_reg_write_byte(i2c, MPU_ADDR, 0x19, 19);    /*  0x19 SMPLRT_DIV:sample rate: 1000/(1+19) = 50 Hz */
	i2c_reg_write_byte(i2c, MPU_ADDR, 0x1B, 0x08);  /* 0x1B GYRO_CONFIG:gyro range: +-500 deg/s */
	i2c_reg_write_byte(i2c, MPU_ADDR, 0x1C, 0x08);  /* 0x1C ACCEL_CONFIG:accel range: +-4 g */
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
		int16_t a = (raw[2 * i] << 8) | raw[2 * i + 1];      /* bytes 0-5: join high + low */
		int16_t g = (raw[8 + 2 * i] << 8) | raw[9 + 2 * i];  /* bytes 8-13 (6-7 = temp, skipped) */
		acc[i]  = a / 8192.0f * 9.81f;   /* +-4 g: 8192 per g -> m/s2 */
		gyro[i] = g / 65.5f;             /* +-500 deg/s: 65.5 per deg/s */
	}
	acc[2] = acc[2] - ACC_Z_OFFSET;      /* remove the measured Z offset */
	return true;
}


/*                    Si7021 (temp + humidity)                   */
/* ============================================================ */

/* step 1: send 0xF5 = "start measuring humidity" */
bool si_start(void)
{
	uint8_t cmd = 0xF5;
	return i2c_write(i2c, &cmd, 1, SI_ADDR) == 0;
}

/* step 2: collect humidity, then ask for the temperature (0xE0) */
bool si_read(float *temp, float *hum)
{
	uint8_t buf[2];
	uint8_t cmd = 0xE0;

	/* humidity: the sensor sends 2 bytes (high, low) */
	if (i2c_read(i2c, buf, 2, SI_ADDR) != 0) {
		return false;                            /* no answer */
	}
	uint16_t code = (buf[0] << 8) | buf[1];      /* join the 2 bytes */
	*hum = 125.0f * code / 65536.0f - 6.0f;      /* raw -> % (datasheet) */
	if (*hum < 0)   *hum = 0;                    /* keep it in 0..100 % */
	if (*hum > 100) *hum = 100;

	/* temperature: send 0xE0, the sensor answers 2 bytes */
	if (i2c_write_read(i2c, SI_ADDR, &cmd, 1, buf, 2) != 0) {
		return false;
	}
	code = (buf[0] << 8) | buf[1];
	*temp = 175.72f * code / 65536.0f - 46.85f;  /* raw -> deg C (datasheet) */
	return true;
}

/*                        LDR (light)                            */
/* ============================================================ */

const struct adc_dt_spec ldr = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));
int16_t adc_buf;                          /* the ADC writes the result here */
struct adc_sequence adc_seq = {
	.buffer      = &adc_buf,
	.buffer_size = sizeof(adc_buf),
};

/* prepare the ADC channel. Returns true if OK. */
bool ldr_init(void)
{
	if (!adc_is_ready_dt(&ldr)) {
		return false;
	}
	if (adc_channel_setup_dt(&ldr) != 0) {
		return false;
	}
	if (adc_sequence_init_dt(&ldr, &adc_seq) != 0) {
		return false;
	}
	return true;
}

/* read the light: raw (0..4095) and light (0..100 %). Returns true if OK. */
bool ldr_read(int *raw, float *light)
{
	int sum = 0;

	/* 4 readings and average, so one noisy value does not matter */
	for (int i = 0; i < 4; i++) {
		if (adc_read_dt(&ldr, &adc_seq) != 0) {
			return false;
		}
		sum = sum + adc_buf;
	}
	*raw = sum / 4;

	/* two-point calibration: LDR_DARK -> 0 %, LDR_BRIGHT -> 100 %
	 * (works also if the module gives a lower value with more light) */
	*light = (*raw - LDR_DARK) * 100.0f / (LDR_BRIGHT - LDR_DARK);
	if (*light < 0)   *light = 0;          /* keep it in 0..100 % */
	if (*light > 100) *light = 100;

	/* raw stuck at 0 or 4095 = wire probably disconnected or shorted */
	return *raw > 5 && *raw < 4090;
}

/* gyro calibration: 100 samples, 20 ms apart = 2 s, board must be still.
 * Always at 20 ms whatever the mode: in TEST the IMU thread runs only every 2 s,
 * so calibrating at that speed would take 200 s. Returns false if the MPU fails. */
bool imu_calibrate(void)
{
	float acc[3], gyro[3];
	float sum[3] = {0, 0, 0};

	for (int n = 0; n < CAL_LOOPS; n++) {
		if (!imu_read(acc, gyro)) {
			return false;                      /* MPU failed during calibration */
		}
		for (int i = 0; i < 3; i++) {
			sum[i] = sum[i] + gyro[i];
		}
		k_msleep(IMU_PERIOD_MS);               /* 20 ms between samples */
	}
	for (int i = 0; i < 3; i++) {
		gyro_bias[i] = sum[i] / CAL_LOOPS;     /* average = gyro offset */
	}
	SENSOR_DBG("gyro calibrated: %+.2f %+.2f %+.2f deg/s\n",
	           (double)gyro_bias[0], (double)gyro_bias[1], (double)gyro_bias[2]);
	return true;
}
/*                    IMU thread (MPU6050)                       */
/* ============================================================ */
/* period depends on the mode (professor's feedback):
 *   TEST         -> every 2 s, like all the sensors
 *   NORMAL, SIMU -> every 20 ms (50 Hz, spec 20-50 Hz)
 * highest priority: strictest deadline */
void imu_thread(void)
{
	if (!device_is_ready(i2c)) {          /* is the I2C bus working? */
		SENSOR_DBG("I2C bus not ready\n");
	}

	/* MPU6050 */
	float acc[3] = {0}, gyro[3] = {0};    /* latest IMU values */
	bool imu_ok = imu_check();            /* is the MPU6050 there? */
	if (imu_ok) {
		imu_setup();
		imu_ok = imu_calibrate();          /* 2 s, board still */
	}

	int64_t last_retry = 0;               /* time of the last MPU retry */
	int64_t last_print = 0;               /* time of the last test print */

	/* tested: setup (+ now calibration) takes time, so wake_time is taken
	 * after it. Otherwise the first loops would run with no wait. */
	int64_t wake_time = k_uptime_get();   /* time now, in ms */

	while (1) {
		/* ask the app which mode we are in, and choose the period */
		int period = (fsm_get_state() == STATE_TEST) ? IMU_TEST_PERIOD_MS : IMU_PERIOD_MS;
		wake_time = wake_time + period;          /* next wake-up */
		k_sleep(K_TIMEOUT_ABS_MS(wake_time));    /* sleep until that time */

		/* MPU lost? try again once per second */
		if (!imu_ok && k_uptime_get() - last_retry >= 1000) {
			last_retry = k_uptime_get();
			if (imu_check()) {
				imu_setup();
				imu_ok = imu_calibrate();      /* sensor restarted: calibrate again */
				wake_time = k_uptime_get();    /* calibration took 2 s: restart the timing */
			}
		}

		if (imu_ok) {
			imu_ok = imu_read(acc, gyro);
		}

		if (imu_ok) {
			/* remove the gyro offset measured at calibration */
			for (int i = 0; i < 3; i++) {
				gyro[i] = gyro[i] - gyro_bias[i];
			}
			/* send the IMU values to the app */
			fsm_update_imu_data(acc[0], acc[1], acc[2],
			                    gyro[0], gyro[1], gyro[2]);
		}

		/* test print: every 1 s (in TEST: every loop, 2 s) */
		if (k_uptime_get() - last_print >= 1000) {
			last_print = k_uptime_get();
			if (!imu_ok) {
				SENSOR_DBG(" IMU   : not working, retrying...\n");
			} else {
				SENSOR_DBG(" acc   [m/s2]  X: %+6.2f | Y: %+6.2f | Z: %+6.2f\n",
				           (double)acc[0], (double)acc[1], (double)acc[2]);
				SENSOR_DBG(" gyro  [deg/s] X: %+6.1f | Y: %+6.1f | Z: %+6.1f\n",
				           (double)gyro[0], (double)gyro[1], (double)gyro[2]);
			}
		}
	}   /* end of while */
}       /* end of imu_thread */


/*              environment thread (Si7021 + LDR)                */
/* ============================================================ */

void env_thread(void)
{
	/* Si7021 */
	float temp = 0, hum = 0;              /* last good values */
	bool si_ok = false;                   /* Si7021 working? */

	/* LDR */
	int ldr_raw = 0;                      /* last ADC value (0..4095) */
	float light = 0;                      /* last light value (0..100 %) */
	bool ldr_ok = false;                  /* LDR value valid? */
	bool ldr_ready = ldr_init();          /* ADC set up? */
	if (!ldr_ready) {
		SENSOR_DBG("LDR: ADC not ready\n");
	}

	int64_t wake_time = k_uptime_get();   /* time now, in ms */

	while (1) {
		/* ------------------ Si7021 ------------------ */
		/* order a measurement, wait ~20 ms, collect it.
		 * Waiting is fine here: the IMU runs in its own thread, so it keeps its 20 ms rhythm.
		 * If it fails: si_ok = false and the last good values are kept (spec 2.4.9). */
		if (si_start()) {
			k_msleep(25);                       /* a measurement takes ~20 ms */
			si_ok = si_read(&temp, &hum);
		} else {
			si_ok = false;                      /* no answer: keep the last good values */
		}

		/* ------------------ LDR ------------------ */
		/* the ADC is fast (much less than 1 ms), so we read it directly */
		if (ldr_ready) {
			ldr_ok = ldr_read(&ldr_raw, &light);
		}

		/* send temp, hum and light to the app every 2 s */
		fsm_update_environment_data(light, temp, hum);

		/* test print: every 2 s */
		SENSOR_DBG(" temp  : %+5.1f C    hum: %5.1f %%    ok = %d\n",
		           (double)temp, (double)hum, si_ok);
		SENSOR_DBG(" light : %5.1f %%     raw: %4d       ok = %d\n",
		           (double)light, ldr_raw, ldr_ok);


		wake_time = wake_time + ENV_PERIOD_MS;   /* next run: 2 s later */
		k_sleep(K_TIMEOUT_ABS_MS(wake_time));    /* sleep until that time */
	}   /* end of while */
}       /* end of env_thread */

/* start the two threads, 100 ms after boot:
 * environment: stack 2048, priority 4 (slow, can wait)
 * both use the same I2C bus: Zephyr's I2C driver locks it for each transfer */
K_THREAD_DEFINE(imu_id, 2048, imu_thread, NULL, NULL, NULL, 2, 0, 100);
K_THREAD_DEFINE(env_id, 2048, env_thread, NULL, NULL, NULL, 4, 0, 100);