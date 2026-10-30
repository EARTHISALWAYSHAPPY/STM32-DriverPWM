#include "SysTicker.h"
#include "stm32f446xx.h"

volatile uint32_t ms_ticks = 0;

void SysTick_Handler(void)
{
    ms_ticks++;
}

void systick_init(void)
{
    SysTick->LOAD = (24000000 / 1000) - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = (1 << 2) | (1 << 1) | (1 << 0);
}