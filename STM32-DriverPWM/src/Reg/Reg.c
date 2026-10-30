#include "Reg.h"
#include "stm32f446xx.h"

void reg_setting(void)
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
    ADC1->CR2 = (0x01 << 0);     // Enable ADC
    ADC1->SQR1 &= ~(0x0F << 20); // Use 1 Conversion (PA0)
    ADC1->SQR3 &= ~(0x1F << 0);  // Regular Ch.

    // Set : Timer1 (TIM2)
    TIM2->PSC = 23;                          // PSC
    TIM2->ARR = 2499;                        // AR
    TIM2->CCMR1 = (0x06 << 4) | (0x01 << 3); // PWM mode 1, Pre-load Enable
    TIM2->CCER |= (0x01 << 0);
    TIM2->CR1 |= (0x01 << 0);
}