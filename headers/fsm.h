#ifndef FSM_H
#define FSM_H

typedef enum {
    STATE_OFF,
    STATE_NORMAL,
    STATE_BLUE
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