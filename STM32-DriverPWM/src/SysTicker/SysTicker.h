#ifndef SYSTICKER_H
#define SYSTICKER_H

#include <stdint.h>

extern volatile uint32_t ms_ticks;

void systick_init(void);

#endif /* SYSTICKER_H */