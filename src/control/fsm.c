#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "button_handler.h"
#include <zephyr/sys/printk.h>
#include "fsm.h"
#include "brightness_thread.h"
#include "rgb.h"

#define FSM_STACK_SIZE 1024
#define FSM_PRIORITY   7

typedef enum {
    EVENT_SHORT_PRESS,
    EVENT_LONG_PRESS
} fsm_event_t;
/*
 * Current operating mode.
 *
 * The system starts in NORMAL mode.
 */
static state_t current_state = STATE_NORMAL;

/*
 * FSM thread.
 */
K_THREAD_DEFINE(fsm_tid,
                FSM_STACK_SIZE,
                fsm_thread,
                NULL, NULL, NULL,
                FSM_PRIORITY,
                0,
                0);

/*its better to give FSM an intialization function*/
/*
 * Initialize the FSM in NORMAL mode.
 */
int fsm_init(void)
{
    current_state = STATE_NORMAL;

    brightness_sampling_start();

    printk("[MODE] System NORMAL\n");

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

static void fsm_start_normal(void)
{
    brightness_sampling_start();
}

static void fsm_start_blue(void)
{
    brightness_sampling_stop();
    rgb_set(0,0,1);
}

static void fsm_start_off(void)
{
    brightness_sampling_stop();
    rgb_set(0,0,0);
}

static void fsm_change_state(fsm_event_t event)
{
    state_t next_state = current_state;

    switch (current_state)
    {
        case STATE_OFF:
            if (event == EVENT_SHORT_PRESS)
            {
                printk("[FSM] OFF --> NORMAL\n");
                next_state = STATE_NORMAL;
                if(!fsm_state_compare(next_state))
                {
                    current_state = next_state;
                    fsm_start_normal();
                }
            }
            break;
        case STATE_NORMAL:
            if (event == EVENT_SHORT_PRESS)
            {
                printk("[FSM] NORMAL --> BLUE\n");
                next_state = STATE_BLUE;
                if(!fsm_state_compare(next_state))
                {
                    current_state = next_state;
                    fsm_start_blue();
                }
            } else if (event == EVENT_LONG_PRESS)
            {
                printk("[FSM] NORMAL --> OFF\n");
                next_state = STATE_OFF;
                if(!fsm_state_compare(next_state))
                {
                    current_state = next_state;
                    fsm_start_off();
                }
            }
            break;
            
        case STATE_BLUE:
            if (event == EVENT_SHORT_PRESS)
            {
                printk("[FSM] BLUE --> NORMAL\n");
                next_state = STATE_NORMAL;
                if(!fsm_state_compare(next_state))
                {
                    current_state = next_state;
                    fsm_start_normal();
                }

            } else if (event == EVENT_LONG_PRESS)
            {
                printk("[FSM] BLUE --> OFF\n");
                next_state = STATE_OFF;
                if(!fsm_state_compare(next_state))
                {
                    current_state = next_state;
                    fsm_start_off();
                }
            }
            break;
    }


}

void fsm_thread(void *p1, void *p2, void *p3) {
    
    
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    button_event_t button_event;

    while (1)
    {
        /*
     * Wait until the button module sends an event.
     */
    k_msgq_get(&button_msgq,
               &button_event,
               K_FOREVER);

    if (button_event == BUTTON_EVENT_SHORT_PRESS)
    {
        printk("[BUTTON] Short press\n");
        fsm_change_state(EVENT_SHORT_PRESS);
    }
    else if (button_event == BUTTON_EVENT_LONG_PRESS)
    {
        printk("[BUTTON] Long press\n");
        fsm_change_state(EVENT_LONG_PRESS);
    }
    }
}