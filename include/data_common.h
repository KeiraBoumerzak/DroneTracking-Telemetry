/* common.h - things we both use.*/

#ifndef DATA_COMMON_H
#define DATA_COMMON_H

#include <stdbool.h>
#include <stdint.h>

/* how often the threads run */
#define IMU_PERIOD_MS  20      /* sensors thread   */
#define APP_PERIOD_MS  100     /* app thread   */

/* modes (B1 changes them) */
#define MODE_TEST    0
#define MODE_NORMAL  1
#define MODE_SIMU    2

/* drone states */
#define ST_IDLE     0
#define ST_FLYING   1
#define ST_PROBLEM  2

/* shared data: sensors and GPS to write here, the app reads it */
struct data {
	float ax, ay, az;          /* acceleration, m/s2 */
	float gx, gy, gz;          /* rotation, deg/s    */
	float roll, pitch, yaw;    /* angles, deg        */
	float x, y, z;             /* position, m        */
	float vx, vy, vz;          /* speed, m/s         */
	float temp, hum, light;    /* C, %, %            */
	bool imu_ok, si_ok, ldr_ok;
	bool fix;                  /* GPS */
	int sats;
	double lat, lon;
	float alt;
	char utc[9];
};

extern struct data d;
void data_lock(void);          /* always lock before using d */
void data_unlock(void);
int get_mode(void);


/* printk that stays quiet in SIMU mode */
#define DBG(...) do { if (get_mode() != MODE_SIMU) printk(__VA_ARGS__); } while (0)

#endif