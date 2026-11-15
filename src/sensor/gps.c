/* gps.c - GPS on USART1, no thread.
 * TODO: the interrupt collects a line, a queue + work item read it,
 * puting position and fix in the shared data. */
#include <zephyr/kernel.h>
#include "data_common.h"
#include "gps.h"

#define UART_NODE DT_NODELABEL(usart1)

K_SEM_DEFINE(gps_sem, 0, 1);

K_TIMER_DEFINE(gps_timer, gps_timer_cb, NULL);

static const struct device *uart = DEVICE_DT_GET(UART_NODE);
static volatile uint8_t rx_char;
static volatile bool rx_ready;

static void gps_timer_cb(struct k_timer *timer)
{
    k_sem_give(&gps_sem);
}

static void uart_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);
	uart_irq_update(dev);

	while (uart_irq_rx_ready(dev))
	{
		if (uart_fifo_read(dev, (uint8_t *)&rx_char, 1) == 1)
		{
			rx_ready = true; // Keep ISR processing short
		}
	}
}

void gps_thread_entry(void *p1, void *p2, void *p3)
{
	//UART init and callback
	if (!device_is_ready(uart)) return -ENODEV;
	uart_irq_callback_user_data_set(uart, uart_isr, NULL);
	uart_irq_rx_enable(uart);

	k_timer_start(&gps_timer, K_MSEC(1000), K_MSEC(1000));

	while (1)
	{
		k_sem_take(&gps_sem, K_FOREVER)
	}
}
