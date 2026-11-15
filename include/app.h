#ifndef APP_H
#define APP_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TRACKER_STATE_IDLE,
    TRACKER_STATE_FLYING,
    TRACKER_STATE_PROBLEM
} tracker_state_t;

typedef struct {
    uint32_t seq;
    uint32_t timestamp_ms;
    uint8_t fix;                  /* GPS */
	uint8_t sats;
	double lat, lon;
	float alt;
    float x, y, z;             /* position, m        */
	float vx, vy, vz;          /* speed, m/s         */
    float roll, pitch, yaw;    /* angles, deg        */
	float ax, ay, az;          /* acceleration, m/s2 */
	float gx, gy, gz;          /* rotation, deg/s    */
	float temp, hum, light;    /* C, %, %            */
    tracker_state_t state;
    uint16_t flags;
}TelemetryPacket;

void fsm_update_gps_data(uint8_t fix, uint8_t sats, double lat, double lon, float alt);
void fsm_update_environment_data(float light, float temp, float hum);
void fsm_update_imu_data(float ax, float ay, float az, float gx, float gy, float gz);
bool fsm_get_telemetry(TelemetryPacket *out);

#endif