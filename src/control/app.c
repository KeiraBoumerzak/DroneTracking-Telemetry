/* app.c - runs every 100 ms.
 * TODO: we read the shared data, decide the state, set the LED,
 * print the output for TEST / NORMAL / SIMU.
 * In SIMU only the frame is printed. */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "button_handler.h"
#include <zephyr/sys/printk.h>
#include "app.h"

#define FSM_STACK_SIZE 1024
#define FSM_PRIORITY   7

typedef enum {
    STATE_TEST,
    STATE_NORMAL,
    STATE_SIMU
} state_t;

static state_t current_state = STATE_TEST;

static TelemetryPacket data;

K_THREAD_DEFINE(fsm_tid,
                FSM_STACK_SIZE,
                fsm_thread,
                NULL, NULL, NULL,
                FSM_PRIORITY,
                0,
                0);
                
int fsm_init(void)
{
    current_state = STATE_TEST;

    brightness_sampling_start();

    //printk("[MODE] System TEST\n");

    return 0;
}

static bool fsm_state_compare(state_t next_state)
{
    if(current_state == next_state)
    {
        return true;
    } else
    {
        return false;
    }
}

static void fsm_start_test(void)
{
}

static void fsm_start_normal(void)
{
}

static void fsm_start_simu(void)
{
}

static void fsm_change_state()
{
    state_t next_state = current_state;

    switch (current_state)
    {
        case STATE_TEST:
            //printk("[FSM] TEST --> NORMAL\n");
            next_state = STATE_NORMAL;
            if(!fsm_state_compare(next_state))
            {
                current_state = next_state;
                fsm_start_normal();
            }
            break;

        case STATE_NORMAL:
            //printk("[FSM] NORMAL --> SIMU\n");
            next_state = STATE_SIMU;
            if(!fsm_state_compare(next_state))
            {
                current_state = next_state;
                fsm_start_simu();
            }
        break;
            
        case STATE_SIMU:
            //printk("[FSM] SIMU --> TEST\n");
            next_state = STATE_TEST;
            if(!fsm_state_compare(next_state))
            {
                current_state = next_state;
                fsm_start_test();
            }
    }


}

void fsm_thread(void *p1, void *p2, void *p3) {
    
    
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1)
    {
    k_sem_take(&button_sem,K_FOREVER);

    fsm_change_state();
    }
}

void fsm_update_gps_data(uint8_t fix, uint8_t sats, double lat, double lon, float alt) {
    if (k_mutex_lock(&fsm_mutex, K_FOREVER) == 0)
    {
        data.fix = fix;
        data.sats = sats;
        if (fix == 1)
        {
            data.lat = lat;
            data.lon = lon;
            data.alt = alt;
            data.flags &= ~0x0002; 
        } else
        {
            data.flags |= 0x0002;
        }
        k_mutex_unlock(&fsm_mutex);
    }
}

void fsm_update_environment_data(float light, float temp, float hum) {
    if (k_mutex_lock(&fsm_mutex, K_FOREVER) == 0) {
        data.light_pct = light;
        data.temperature = temp;
        data.humidity = hum;
        
        data.flags &= ~0x0004;
        data.flags &= ~0x0008;
        
        k_mutex_unlock(&fsm_mutex);
    }
}

void fsm_update_imu_data(float ax, float ay, float az, float gx, float gy, float gz) {
    if (k_mutex_lock(&fsm_mutex, K_FOREVER) == 0) {
        data.ax = ax;
        data.ay = ay;
        data.az = az;
        
        data.gx = gx;
        data.gy = gy;
        data.gz = gz;
        
        data.flags &= ~0x0001;
        
        k_mutex_unlock(&fsm_mutex);
    }
}