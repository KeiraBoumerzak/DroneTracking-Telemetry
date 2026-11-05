#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "rgb.h"

/*
 * External RGB LED is common-anode.
 *
 * Devicetree uses GPIO_ACTIVE_LOW, therefore:
 * logical 1 -> channel ON
 * logical 0 -> channel OFF
 */
static const struct gpio_dt_spec red =
    GPIO_DT_SPEC_GET(DT_ALIAS(rgb_red), gpios);

static const struct gpio_dt_spec green =
    GPIO_DT_SPEC_GET(DT_ALIAS(rgb_green), gpios);

static const struct gpio_dt_spec blue =
    GPIO_DT_SPEC_GET(DT_ALIAS(rgb_blue), gpios);


int rgb_init(void)
{
    int ret;

    if (!gpio_is_ready_dt(&red) ||
        !gpio_is_ready_dt(&green) ||
        !gpio_is_ready_dt(&blue)) {

        printk("[ERROR] RGB GPIO device not ready\n");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("[ERROR] Failed to configure RED GPIO: %d\n", ret);
        return ret;
    }

    ret = gpio_pin_configure_dt(&green, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("[ERROR] Failed to configure GREEN GPIO: %d\n", ret);
        return ret;
    }

    ret = gpio_pin_configure_dt(&blue, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("[ERROR] Failed to configure BLUE GPIO: %d\n", ret);
        return ret;
    }

    return 0;
}


int rgb_set(int red_state, int green_state, int blue_state)
{
    int ret;

    ret = gpio_pin_set_dt(&red, red_state);
    if (ret < 0) {
        return ret;
    }

    ret = gpio_pin_set_dt(&green, green_state);
    if (ret < 0) {
        return ret;
    }

    return gpio_pin_set_dt(&blue, blue_state);
}