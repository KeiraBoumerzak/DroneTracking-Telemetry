#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include "button_handler.h"

#define USER_BUTTON DT_ALIAS(sw1)

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(USER_BUTTON, gpios);
static struct gpio_callback button_cb;

K_SEM_DEFINE(button_sem, 0, 1);

static void button_isr(const struct device *port,
                       struct gpio_callback *cb,
                       gpio_port_pins_t pins)
{
    ARG_UNUSED(port);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    //printk("[BUTTON] Press\n");
    k_sem_give(&button_sem);
    
}

int button_system_init(void)
{ 
    int ret;

    if (!gpio_is_ready_dt(&button)) {
        printk("[ERROR] Button GPIO device not ready\n");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);

    if (ret < 0) {
        printk("[ERROR] Failed to configure button GPIO: %d\n", ret);
        return ret;
    }

    gpio_init_callback(&button_cb,
                       button_isr,
                       BIT(button.pin));

    ret = gpio_add_callback(button.port, &button_cb);

    if (ret < 0) {
        printk("[ERROR] Failed to add button callback: %d\n", ret);
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_INACTIVE);

    if (ret < 0) {
        printk("[ERROR] Failed to configure button interrupt: %d\n", ret);
        return ret;
    }

    printk("[INIT] Button handler initialized\n");

    return 0;
}