#include <zephyr/kernel.h>
#include "app.h"

#define TX_STACK_SIZE 1024
#define TX_PRIORITY   8

#define TEST_PERIOD_MS   2000
#define NORMAL_PERIOD_MS 5000
#define SIMU_PERIOD_MS    100

static void send_test_data(const TelemetryPacket *packet)
{
    ARG_UNUSED(packet);
}

static void send_normal_data(const TelemetryPacket *packet)
{
    ARG_UNUSED(packet);
}

static void send_simu_data(const TelemetryPacket *packet)
{
    ARG_UNUSED(packet);
}

static void data_tx_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    TelemetryPacket packet;
    uint32_t period_ms;

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

        if (fsm_get_telemetry(&packet))
        {
            switch (mode)
            {
                case TX_MODE_TEST:
                    send_test_data(&packet);
                    break;
                case TX_MODE_NORMAL:
                    send_normal_data(&packet);
                    break;
                case TX_MODE_SIMU:
                    send_simu_data(&packet);
                    break;
                default:
                    break;
            }
        }
    }
}

K_THREAD_DEFINE(data_tx_tid,
                TX_STACK_SIZE,
                data_tx_thread,
                NULL, NULL, NULL,
                TX_PRIORITY,
                0,
                0);