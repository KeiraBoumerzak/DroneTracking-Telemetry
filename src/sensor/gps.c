/* gps.c - GPS on USART1, no thread.
 * TODO: the interrupt collects a line, a queue + work item read it,
 * puting position and fix in the shared data. */
#include <zephyr/kernel.h>
#include "common.h"

/* start the GPS (empty for now) */
int gps_init(void)
{
	return 0;
}