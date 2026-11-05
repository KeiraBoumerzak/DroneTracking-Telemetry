#ifndef LDR_H
#define LDR_H

#include <stdint.h>

int ldr_init(void);
int ldr_read_raw(uint16_t *raw_value);

#endif