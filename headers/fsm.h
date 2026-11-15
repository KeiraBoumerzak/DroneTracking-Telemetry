#ifndef FSM_H
#define FSM_H

typedef enum {
    STATE_TEST,
    STATE_NORMAL,
    STATE_SIMU
} state_t;
/*
 * Initialize the FSM.
 */
int fsm_init(void);

/*
 * FSM thread entry function.
 */
void fsm_thread(void *p1, void *p2, void *p3);

#endif