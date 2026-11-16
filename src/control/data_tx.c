/* data_tx.c - sends / prints the telemetry depending on the mode (Adrian):
 * TEST every 2 s, NORMAL every 5 s, SIMU every 100 ms (frames for the simulator). */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "app.h"

#define TX_STACK_SIZE 2048      /* printk with floats needs more stack than 1024 */
#define TX_PRIORITY   8

#define TEST_PERIOD_MS   2000
#define NORMAL_PERIOD_MS 5000
#define SIMU_PERIOD_MS    100

/* print all values of the packet (used by TEST and NORMAL) */
static void print_packet(const TelemetryPacket *p)
{
    printk(" GPS  : fix %u | sats %u | lat %.6f | lon %.6f | alt %.1f m\n",
           p->fix, p->sats, p->lat, p->lon, (double)p->alt);
    printk(" acc  [m/s2]  X: %+6.2f | Y: %+6.2f | Z: %+6.2f\n",
           (double)p->ax, (double)p->ay, (double)p->az);
    printk(" gyro [deg/s] X: %+6.1f | Y: %+6.1f | Z: %+6.1f\n",
           (double)p->gx, (double)p->gy, (double)p->gz);
    printk(" temp : %+5.1f C | hum: %5.1f %% | light: %5.1f %%\n",
           (double)p->temp, (double)p->hum, (double)p->light);
    printk(" flags: 0x%04X\n", p->flags);
}

static void send_test_data(const TelemetryPacket *packet)
{
    printk("\n=========== TEST ===========\n");
    print_packet(packet);
}

static void send_normal_data(const TelemetryPacket *packet)
{
    printk("\n=========== NORMAL ===========\n");
    print_packet(packet);
    /* TODO: 30 s statistics (mean / min / max) */
}

static void send_simu_data(const TelemetryPacket *packet)
{
    /* TODO: frame for the Drone Flight Simulator (format from the teachers' PDF).
     * GR5: in SIMU only the frames may be printed, nothing else. */
    ARG_UNUSED(packet);
}

static void data_tx_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    TelemetryPacket packet;
    uint32_t period_ms = TEST_PERIOD_MS;    /* start value, so it is never empty */

    while (1)
    {
        state_t state = fsm_get_state();

        switch (state) {
        case STATE_TEST:
            period_ms = TEST_PERIOD_MS;
            break;
        case STATE_NORMAL:
            period_ms = NORMAL_PERIOD_MS;
            break;
        case STATE_SIMU:
            period_ms = SIMU_PERIOD_MS;
            break;
        default:
            break;
        }

        if (fsm_get_telemetry(&packet))     /* copy of the shared packet (mutex inside) */
        {
            switch (state)
            {
                case STATE_TEST:
                    send_test_data(&packet);
                    break;
                case STATE_NORMAL:
                    send_normal_data(&packet);
                    break;
                case STATE_SIMU:
                    send_simu_data(&packet);
                    break;
                default:
                    break;
            }
        }

        k_msleep(period_ms);    /* sleep: the CPU is free for the other threads (GR6) */
    }
}

K_THREAD_DEFINE(data_tx_tid,
                TX_STACK_SIZE,
                data_tx_thread,
                NULL, NULL, NULL,
                TX_PRIORITY,
                0,
                0);