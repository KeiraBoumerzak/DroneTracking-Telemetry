#ifndef STATS_H
#define STATS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float min;
    float max;
    float mean;
} LightStats;

typedef struct {
    float min;
    float max;
    float mean;
} TemperatureStats;

typedef struct {
    float min;
    float max;
    float mean;
} HumidityStats;

typedef struct {
    float min;
    float max;
    float mean;
} AccelerationStats;

typedef struct {
    float min;
    float max;
    float mean;
} AngularVelocityStats;

typedef struct {
    LightStats light;
    TemperatureStats temp;
    HumidityStats hum;
    AccelerationStats accel;
    AngularVelocityStats angVel;
} Stats;

typedef struct {
    float min;
    float max;
    double sum;
    uint32_t count;
} StatAccumulator;

void math_add_imu_sample(float ax, float ay, float az,
                         float gx, float gy, float gz);

void math_add_env_sample(float light, float hum, float temp);

bool math_get_stats(Stats *out);

#endif