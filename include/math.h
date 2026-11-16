#ifndef MATH_H
#define MATH_H

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

#endif