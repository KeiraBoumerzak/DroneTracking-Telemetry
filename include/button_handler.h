#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <zephyr/kernel.h>

int button_system_init(void);

extern struct k_sem button_sem;

#endif