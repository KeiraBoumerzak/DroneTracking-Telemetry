#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "button_handler.h"
#include <zephyr/sys/printk.h>
#include "fsm.h"
#include "brightness_thread.h"
#include "rgb.h"

#define FSM_STACK_SIZE 1024
#define FSM_PRIORITY   7

static state_t current_state = STATE_TEST;

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