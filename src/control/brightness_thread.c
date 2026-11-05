#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/atomic.h>

#include "ldr.h"
#include "rgb.h"
#include "brightness_thread.h"

/* in our serial we noticed something liks this :
 * [FSM] NORMAL --> BLUE
 * [BRIGHTNESS] ... RGB=GREEN
 * [BRIGHTNESS] Sampling suspended
so that means sometimes:
 * one brightness reading still appeared after changing from NORMAL
 * to BLUE.
 * This happened because the timer had already released the semaphore,
 * so the brightness thread was already ready to run.
 * We added the atomic sampling_enabled flag so the thread checks
 * whether sampling is still allowed before reading the ADC.
 * 
 * When entering BLUE or OFF, the flag is cleared, so any old pending
 * event will not cause another ADC reading.
 */
 

static atomic_t sampling_enabled = ATOMIC_INIT(0);
/*
 * The ADC is configured with 12-bit resolution.
 * Therefore the valid full-scale range is 0 ... 4095.
 */
#define ADC_MAX_VALUE 4095U

/*
 * Brightness thread configuration.
 */
#define BRIGHTNESS_THREAD_STACK_SIZE 1024
#define BRIGHTNESS_THREAD_PRIORITY   5


/*
 * Semaphore used to wake the brightness thread.
 *
 * Initial count = 0:
 *   the thread starts blocked.
 *
 * Maximum count = 1:
 *   only one pending sampling event can be stored.
 */
K_SEM_DEFINE(brightness_sem, 0, 1);


/*
 * Timer callback.
 *
 * This function is called every 2 seconds while sampling is enabled.
 * It only gives the semaphore.
 *
 * ADC reading and RGB control are not performed inside the timer callback.
 * That work is done by the brightness thread.
 */
static void brightness_timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    k_sem_give(&brightness_sem);
}


/*
 * Periodic timer used to generate the 2-second sampling event.
 */
K_TIMER_DEFINE(brightness_timer,
               brightness_timer_handler,
               NULL);


/*
 * Entry function executed by the brightness monitoring thread.
 */
static void brightness_thread_entry(void *p1, void *p2, void *p3)
{
    /*
     * Zephyr thread entry functions receive three optional parameters.
     * This thread does not use them.
     */
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {

        /*
         * Wait until the timer gives the semaphore.
         *
         * K_FOREVER means the thread remains blocked efficiently
         * until a sampling event occurs.
         */
        k_sem_take(&brightness_sem, K_FOREVER);
/*
 * The timer may have given the semaphore just before the FSM
 * disabled brightness sampling.
 *
 * Check whether sampling is still allowed before reading the ADC.
 *
 * If sampling is disabled, go back to waiting for the next event.
 */
        if (!atomic_get(&sampling_enabled)) {
        continue;    }

        uint16_t raw_sample;
        int ret;
        /*
         * Read one raw ADC value from the LDR module.
         */
        ret = ldr_read_raw(&raw_sample);

        if (ret < 0) {
            printk("[ERROR] LDR read failed: %d\n", ret);
            continue;
        }

        /*
         * Convert the sample to 32 bits before doing the percentage
         * calculation.
         */
        uint32_t raw = (uint32_t)raw_sample;

        /*
         * Protect against an unexpected value above the valid
         * 12-bit ADC range.
         */
        if (raw > ADC_MAX_VALUE) {
            raw = ADC_MAX_VALUE;
        }

        /*
         * Current voltage-divider orientation:
         *
         * more light -> lower ADC value
         * less light -> higher ADC value
         *
         * Therefore the normalized ADC value is inverted so that:
         *
         * more light -> higher brightness %
         * less light -> lower brightness %
         */
        uint32_t brightness =
            100U -
            ((raw * 100U + ADC_MAX_VALUE / 2U) / ADC_MAX_VALUE);

        const char *color;

        /*
         * Required brightness-to-color mapping:
         *
         * brightness < 33%         -> RED
         * 33% <= brightness <= 66% -> YELLOW
         * brightness > 66%         -> GREEN
         */
        if (brightness < 33U) {

            ret = rgb_set(1, 0, 0);
            color = "RED";

        } else if (brightness <= 66U) {

            ret = rgb_set(1, 1, 0);
            color = "YELLOW";

        } else {

            ret = rgb_set(0, 1, 0);
            color = "GREEN";
        }

        /*
         * Report any RGB update error.
         */
        if (ret < 0) {
            printk("[ERROR] RGB update failed: %d\n", ret);
        } else {

            /*
             * Serial diagnostic output.
             *
             * The timestamp is useful for verifying that measurements
             * occur approximately every 2000 ms.
             */
            printk("[BRIGHTNESS] t=%lld ms | ADC=%u/4095 | Brightness=%u%% | RGB=%s\n",
                   k_uptime_get(), raw, brightness, color);
        }
    }
}


/*
 * Statically create the brightness monitoring thread.
 *
 * K_THREAD_DEFINE() creates the thread automatically during startup.
 *
 * Parameters:
 *   brightness_thread_id          -> thread identifier
 *   BRIGHTNESS_THREAD_STACK_SIZE  -> thread stack size
 *   brightness_thread_entry       -> function executed by the thread
 *   NULL, NULL, NULL              -> unused thread parameters
 *   BRIGHTNESS_THREAD_PRIORITY    -> scheduling priority
 *   0                             -> no special options
 *   0                             -> start immediately
 */
K_THREAD_DEFINE(brightness_thread_id,
                BRIGHTNESS_THREAD_STACK_SIZE,
                brightness_thread_entry,
                NULL,
                NULL,
                NULL,
                BRIGHTNESS_THREAD_PRIORITY,
                0,
                0);


/*
 * Initialize the brightness monitoring module.
 *
 * The thread itself is already created automatically by
 * K_THREAD_DEFINE().
 *
 * Sampling is not started here because the mode-control FSM
 * will eventually decide when brightness acquisition is active.
 */
int brightness_thread_init(void)
{
    printk("[INIT] Brightness thread ready\n");

    return 0;
}


/*
 * Start periodic brightness acquisition.
 *
 * The timer waits 2 seconds before the first sampling event,
 * then repeats every 2 seconds.
 *
 * Later, the FSM will call this function when entering NORMAL mode.
 */
void brightness_sampling_start(void)
{
     /*
     * Allow the brightness thread to perform ADC measurements.
     */
    atomic_set(&sampling_enabled, 1);

    /*
     * Start the periodic timer:
     * first event after 2 seconds,
     * then one event every 2 seconds.
     */
    k_timer_start(&brightness_timer,
                  K_SECONDS(2),
                  K_SECONDS(2));

    printk("[BRIGHTNESS] Sampling enabled\n");
}


/*
 * Stop reading the LDR and keep the brightness thread waiting
 * until sampling is started again.
 *
 * The timer is stopped so no new semaphore events are generated.
 *
 * The semaphore is reset so that a previously pending event does not
 * cause one extra ADC reading after sampling has been stopped.
 *
 * The thread then remains blocked on:
 *
 *     k_sem_take(&brightness_sem, K_FOREVER);
 *
 * Later, the FSM will call this function in BLUE and OFF modes.
 */
void brightness_sampling_stop(void)
{
      /*
     * Disable ADC sampling first.
     *
     * If the brightness thread was already awakened by an old
     * semaphore event, it will check this flag and skip the ADC read.
     */
    atomic_clear(&sampling_enabled);

    /*
     * Stop future periodic sampling events.
     */
    k_timer_stop(&brightness_timer);
 /*
     * Remove any pending semaphore event.
     */
    k_sem_reset(&brightness_sem);

    printk("[BRIGHTNESS] Sampling suspended\n");
}


/*
 * Previous design:
 *
 * brightness thread
 *     -> perform work
 *     -> k_sleep(2 seconds)
 *     -> perform work again
 *
 * Current design:
 *
 * timer
 *     -> gives semaphore
 *
 * brightness thread
 *     -> waits for semaphore
 *     -> wakes up
 *     -> performs one measurement
 *     -> waits again
 *
 * This separates timing from sensor processing and allows brightness
 * sampling to be suspended cleanly in BLUE and OFF modes.
 */