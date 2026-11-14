/* main.c - shared data, mode and start-up */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include "data_common.h"
#include "gps.h"

/* shared data: sensors and GPS write here, the app reads it */
struct data d;

/* only one thread can use d at a time */
K_MUTEX_DEFINE(data_mutex);

void data_lock(void)   { k_mutex_lock(&data_mutex, K_FOREVER); }   /* take it, wait if busy */
void data_unlock(void) { k_mutex_unlock(&data_mutex); }            /* give it back */


int get_mode(void)
{
	return MODE_TEST; 
}

/* runs once at start-up, then the threads keep working */
int main(void)
{
	strcpy(d.utc, "--:--:--");          /* no GPS time yet */
	printk("\nDrone tracker \n");
	gps_init();                         /* empty for now (Adrian) */
	return 0;
}

