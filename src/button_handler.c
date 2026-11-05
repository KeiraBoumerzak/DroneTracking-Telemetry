#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include "button_handler.h"

#define LONG_PRESS_TIMEOUT_MS 1000

#define USER_BUTTON DT_ALIAS(sw1)

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(USER_BUTTON, gpios);
static struct gpio_callback button_cb;

static struct k_timer press_timer;

static bool long_press_triggered = false;

//this  global variable to be remove and replace it by msg queue 
/*int press_type = 0;
K_SEM_DEFINE(button_sem, 0, 1);    */

K_MSGQ_DEFINE(button_msgq,sizeof(button_event_t),4,4);

void long_press_timer_handler(struct k_timer *timer_id)
{
       /*long_press_triggered = true;
    press_type = 2;
    printk("Long press\n");
    k_sem_give(&button_sem);*/
// this one was replaced by this new  following code :
    ARG_UNUSED(timer_id);

    long_press_triggered = true;

    button_event_t event = BUTTON_EVENT_LONG_PRESS;

    /*
     * Send the LONG event to the FSM.
     *
     * K_NO_WAIT is used because a timer callback must not block.
     */
    k_msgq_put(&button_msgq, &event, K_NO_WAIT);
}

static void button_isr(const struct device *port,
                       struct gpio_callback *cb,
                       gpio_port_pins_t pins)
{
    ARG_UNUSED(port);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    
    int value = gpio_pin_get_dt(&button);

    if (value == 1)
    {
        long_press_triggered = false;
        k_timer_start(&press_timer, K_MSEC(LONG_PRESS_TIMEOUT_MS), K_FOREVER);
    } else if (value == 0)
    {
        k_timer_stop(&press_timer);
       
       /*if(!long_press_triggered)
        {
            press_type = 1;
            printk("Short Press\n");
            k_sem_give(&button_sem);
        }*/ // and this short press replaced by:
        if (!long_press_triggered)
{
    button_event_t event = BUTTON_EVENT_SHORT_PRESS;

    /*
     * Send the SHORT event to the FSM.
     * The ISR must not block, so K_NO_WAIT is used.
     */
    k_msgq_put(&button_msgq, &event, K_NO_WAIT);
}//So the ISR becomes responsible only for detecting and sending the event.
    }
    
}

int button_system_init(void)
{ /*this will checks all important GPIO setup calls 
    *and tells you exactly what failed*/
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

    ret = gpio_pin_interrupt_configure_dt(&button,
                                          GPIO_INT_EDGE_BOTH);

    if (ret < 0) {
        printk("[ERROR] Failed to configure button interrupt: %d\n", ret);
        return ret;
    }

    k_timer_init(&press_timer,
                 long_press_timer_handler,
                 NULL);

    printk("[INIT] Button handler initialized\n");

    return 0;
}