#include "stm32f446xx.h"
#include <stdint.h>

void Clk_24Mhz(void)
{
    // HSI (16 MHz) -> 24 Mhz by use PLL
    RCC->CR |= (0x01 << 0);
    while (!(RCC->CR & (0x01 << 1)))
        ;

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
        ;

    RCC->CFGR &= ~(0x03 << 0);
    RCC->CFGR |= (0x02 << 0);

    while ((RCC->CFGR & (3 << 2)) != (2 << 2))
        ;
}

void reg_setting()
{
    // Set : Reset and Clk Ctrl
    RCC->AHB1ENR |= (0x01 << 0); // Use GPIOA : Clk Enable
    RCC->APB1ENR |= (0x01 << 0); // Use TIM2 : Clk Enable
    RCC->APB2ENR |= (0x01 << 8); // Use ADC1 : Clk Enabled

    // Set GPIO : Use PA0(analog input), PA5(PWM driver)
    GPIOA->MODER |= (0x03 << 0);                                              // PA0 Analog mode
    GPIOA->MODER = (GPIOA->MODER & ~(0x03 << (5 * 2))) | (0x02 << (5 * 2));   // PA5 Alternate func. mode
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0x0F << (5 * 4))) | (0x01 << (5 * 4)); // PA5 : AF1 (TIM1/TIM2)

    // Set : ADC1
    // ADC1->CR1 = (0x00 << 0);  // ADC input Ch.0
    ADC1->CR2 = (0x01 << 0);     // Enable ADC
    ADC1->SQR1 &= ~(0x0F << 20); // Use 1 Conversion (PA0)
    ADC1->SQR3 &= ~(0x1F << 0);  // Regular Ch.

    // Set : Timer1
    TIM2->PSC = 23;                          // PSC
    TIM2->ARR = 2499;                        // AR
    TIM2->CCMR1 = (0x06 << 4) | (0x01 << 3); // PWM mode 1, Pre-load Enable
    TIM2->CCER |= (0x01 << 0);
    TIM2->CR1 |= (0x01 << 0);
}

/*
==========================================================
Note :

ADC : ADC value = (Vin / Vref) * 2^12

Mapping ADC wtih PWM : PWM = ( DC value * ARR) / ADC MAX

F_PWM = Fclk (24 MHz) / (PSC + 1) * (ARR + 1)
-> (PSC + 1) * (ARR + 1) = Fclk(24 MHz) / F_PWM(400 Hz)
-> Def. PSC = 23 so; ARR = 2500 - 1 = 2499
==========================================================
*/

int main(void)
{
    Clk_24Mhz();
    reg_setting();

    // while (1)
    // {
    //     ADC1->CR2 |= (0x01 << 30); // Start conversion.

    //     while (!(ADC1->SR & (0x01 << 1)));

    //     uint32_t adc_val = ADC1->DR;

    //     TIM2->CCR1 = (adc_val * 2499) / 4095;
    // }

    int duty_cycle = 0;
    int step = 25;

    while (1)
    {

        TIM2->CCR1 = duty_cycle;

        duty_cycle = duty_cycle + step;

        if (duty_cycle >= 2499 || duty_cycle <= 0)
        {
            step = -step;
        }

        for (volatile uint32_t i = 0; i < 20000; i++)
        {
            // dummy loop
        }
    }
}

