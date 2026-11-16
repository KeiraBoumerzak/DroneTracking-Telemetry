/* main.c - starts the drone tracker */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "gps.h"
#include "rgb.h"
#include "button_handler.h"

/* runs once at start-up, then the threads keep working */
int main(void)
{
	printk("\nDrone tracker\n");
	gps_init();             /* GPS: UART interrupt + GPS thread (Adrian) */
	button_system_init();   /* button B1 -> mode change (Adrian) */
	rgb_init();             /* RGB LED (Adrian) */
	rgb_set(0, 0, 1);       /* we start in TEST blue: same colour as fsm_start_test */
	return 0;               /* sensors, FSM and send threads start on their own */
}