#include "Clk.h"
#include "stm32f446xx.h"

void Clk_24Mhz(void)
{
    // HSI (16 MHz) -> 24 Mhz by use PLL
    RCC->CR |= (0x01 << 0);
    while (!(RCC->CR & (0x01 << 1)))
    {
    }

    FLASH->ACR = (0x01 << 8) | (0x01 << 9) | 0x00;

    RCC->CFGR &= ~(0x0F << 4);
    RCC->CFGR &= ~(0x07 << 10);
    RCC->CFGR &= ~(0x07 << 13);

    RCC->PLLCFGR = 0;
    RCC->PLLCFGR |= (0x08 << 0);
    RCC->PLLCFGR |= (0x60 << 6);
    RCC->PLLCFGR |= (0x03 << 16);

    RCC->CR |= (0x01 << 24);
    while (!(RCC->CR & (0x01 << 25)))
    {
    }

    RCC->CFGR &= ~(0x03 << 0);
    RCC->CFGR |= (0x02 << 0);

    while ((RCC->CFGR & (3 << 2)) != (2 << 2))
    {
    }
}