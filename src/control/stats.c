#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "stats.h"

#define STATS_WINDOW_MS 30000

K_MUTEX_DEFINE(stats_mutex);
K_MUTEX_DEFINE(stats_imu_mutex);
K_MUTEX_DEFINE(stats_env_mutex);

K_SEM_DEFINE(stat_sem, 0, 1);

static Stats stats = {0};
static StatAccumulator lightAccumulator = {0};
static StatAccumulator tempAccumulator = {0};
static StatAccumulator humAccumulator = {0};
static StatAccumulator accelAccumulator = {0};
static StatAccumulator angVelAccumulator = {0};

static void stat_timer_cb(struct k_timer *timer)
{
    k_sem_give(&stat_sem);
}
K_TIMER_DEFINE(stat_timer, stat_timer_cb, NULL);

float calculate_acceleration(float ax, float ay, float az)
{
    return sqrtf(ax * ax + ay * ay + az * az);
}

float calculate_angular_velocity(float gx, float gy, float gz)
{
    return sqrtf(gx * gx + gy * gy + gz * gz);
}

float calculate_mean(StatAccumulator *in)
{
    return (in->sum/(float)in->count);
}



void math_add_imu_sample(float ax, float ay, float az,float gx, float gy, float gz)
{
    if (k_mutex_lock(&stats_imu_mutex, K_FOREVER) == 0)
    {
        float acceleration = calculate_acceleration(ax, ay, az);
        float angularVelocity = calculate_angular_velocity(gx, gy, gz);
        if(accelAccumulator.count == 0)
        {
            accelAccumulator.max = acceleration;
            accelAccumulator.min = acceleration;
        }else if(acceleration >= accelAccumulator.max ) {
            accelAccumulator.max = acceleration;
        } else accelAccumulator.min = acceleration;
        accelAccumulator.sum += (double)acceleration;
        accelAccumulator.count += 1;

        if(angVelAccumulator.count == 0)
        {
            angVelAccumulator.max = angularVelocity;
            angVelAccumulator.min = angularVelocity;
        }else if(angularVelocity >= angVelAccumulator.max ) {
            angVelAccumulator.max = angularVelocity;
        } else angVelAccumulator.min = angularVelocity;
        angVelAccumulator.sum += (double)angularVelocity;
        angVelAccumulator.count += 1;

        k_mutex_unlock(&stats_imu_mutex);
    }
}

void math_add_env_sample(float light, float hum, float temp)
{
    if (k_mutex_lock(&stats_env_mutex, K_FOREVER) == 0)
    {
        if(lightAccumulator.count == 0)
        {
            lightAccumulator.max = light;
            lightAccumulator.min = light;
        }else if(light >= lightAccumulator.max ) {
            lightAccumulator.max = light;
        } else lightAccumulator.min = light;

        lightAccumulator.sum += (double)light;
        lightAccumulator.count += 1;

        if(humAccumulator.count == 0)
        {
            humAccumulator.max = hum;
            humAccumulator.min = hum;
        }else if(hum >= humAccumulator.max ) {
            humAccumulator.max = hum;
        } else humAccumulator.min = hum;
        humAccumulator.sum += (double)hum;
        humAccumulator.count += 1;

        if(tempAccumulator.count == 0)
        {
            tempAccumulator.max = temp;
            tempAccumulator.min = temp;
        }else if(temp >= tempAccumulator.max ) {
            tempAccumulator.max = temp;
        } else tempAccumulator.min = temp;
        tempAccumulator.sum += (double)temp;
        tempAccumulator.count += 1;

        k_mutex_unlock(&stats_env_mutex);
    }

}

bool math_get_stats(Stats *out)
{
    if (k_mutex_lock(&stats_mutex, K_FOREVER) != 0) {
        return false;
    }

    out = &stats;

    k_mutex_unlock(&stats_mutex);
    return true;
}

static void stats_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    StatAccumulator aux_light, aux_temp, aux_hum, aux_accel, aux_angVel;

    while (1)
    {
        k_sem_take(&stat_sem);

        k_mutex_lock(&stats_imu_mutex, K_FOREVER);
        aux_accel = accelAccumulator;
        aux_angVel = angVelAccumulator;
        accelAccumulator = (StatAccumulator){0};
        angVelAccumulator = (StatAccumulator){0};
        k_mutex_unlock(&stats_imu_mutex);

        k_mutex_lock(&stats_env_mutex, K_FOREVER);
        aux_light = lightAccumulator;
        aux_hum = humAccumulator;
        aux_temp = tempAccumulator;
        lightAccumulator = (StatAccumulator){0};
        humAccumulator = (StatAccumulator){0};
        tempAccumulator = (StatAccumulator){0};
        k_mutex_unlock(&stats_env_mutex);

        k_mutex_lock(&stats_mutex, K_FOREVER);
        stats.accel.max = aux_accel.max;
        stats.accel.min = aux_accel.min;
        stats.accel.mean = calculate_mean(aux_accel);
        stats.angVel.max = aux_angVel.max;
        stats.angVel.min = aux_angVel.min;
        stats.angVel.mean = calculate_mean(aux_angVel);
        stats.light.max = aux_light.max;
        stats.light.min = aux_light.min;
        stats.light.mean = calculate_mean(aux_light);
        stats.hum.max = aux_hum.max;
        stats.hum.min = aux_hum.min;
        stats.hum.mean = calculate_mean(aux_hum);
        stats.temp.max = aux_temp.max;
        stats.temp.min = aux_temp.min;
        stats.temp.mean = calculate_mean(aux_temp);
        k_mutex_unlock(&stats_mutex);
    }
    
}

int stat_init(void)
{
    k_timer_start(&stat_timer, STATS_WINDOW_MS, STATS_WINDOW_MS);
}