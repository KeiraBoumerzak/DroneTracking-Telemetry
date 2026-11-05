#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "button_handler.h"
#include "brightness_thread.h"
#include "fsm.h"
#include "ldr.h"
#include "rgb.h"

int main(void)
{
    int ret;

    printk("\n[INIT] Brightness monitoring system starting\n");

    /*
     * Initialize the LDR / ADC module.
     */
    ret = ldr_init();

    if (ret < 0) {
        printk("[ERROR] LDR initialization failed: %d\n", ret);
        return ret;
    }

    /*
     * Initialize the RGB LED.
     */
    ret = rgb_init();

    if (ret < 0) {
        printk("[ERROR] RGB initialization failed: %d\n", ret);
        return ret;
    }

    /*
     * Initialize the brightness monitoring module.
     */
    ret = brightness_thread_init();

    if (ret < 0) {
        printk("[ERROR] Brightness thread initialization failed: %d\n", ret);
        return ret;
    }

    /*
     * Initialize B2 button handling.
     */
    ret = button_system_init();

    if (ret < 0) {
        printk("[ERROR] Button initialization failed: %d\n", ret);
        return ret;
    }

    /*
     * Initialize the FSM only after all hardware modules are ready.
     *
     * The initial mode is NORMAL, so this also starts
     * periodic brightness sampling.
     */
    ret = fsm_init();

    if (ret < 0) {
        printk("[ERROR] FSM initialization failed: %d\n", ret);
        return ret;
    }

    printk("[INIT] System initialized successfully\n");

    return 0;
}