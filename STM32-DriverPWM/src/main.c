#include "stm32f446xx.h"
#include <stdint.h>
#include <stdio.h>

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
Note :

ADC : ADC value = (Vin / Vref) * 2^12

Mapping ADC wtih PWM : PWM = ( DC value * ARR) / ADC MAX

F_PWM = Fclk (24 MHz) / (PSC + 1) * (ARR + 1)
-> (PSC + 1) * (ARR + 1) = Fclk(24 MHz) / F_PWM(400 Hz)
-> Def. PSC = 23 so; ARR = 2500 - 1 = 2499
*/

void serial_uart_init(uint32_t baudrate)
{
    RCC->APB1ENR |= (0x01 << 17);
    RCC->AHB1ENR |= (0x01 << 0);

    GPIOA->MODER = (GPIOA->MODER & ~((0x03 << 4) | (0x03 << 6))) | ((0x02 << 4) | (0x02 << 6));
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~((0x0F << 8) | (0x0F << 12))) | ((0x07 << 8) | (0x07 << 12));

    USART2->BRR = (24000000 + (baudrate / 2)) / baudrate;
    USART2->CR1 = (0x01 << 13) | (0x01 << 3) | (0x01 << 2);
}

void uart_write(char ch)
{
    while (!(USART2->SR & (0x01 << 7)))
        ;
    USART2->DR = (ch & 0xFF);
}

void uart_print(const char *str)
{
    while (*str)
    {
        uart_write(*str++);
    }
}

int main(void)
{
    Clk_24Mhz();
    reg_setting();
    serial_uart_init(9600);
    systick_init();

    TIM2->EGR |= (0x01 << 0);

    float initial_setpoint = 23.0;
    float T2 = 30.0;

    uint32_t last_stage_time = 0;
    uint32_t last_temp_update = 0;
    float current_pwm = 0;
    float percent_pwm = 0;
    float temp_step = 0.75f;

    char uart_buf[100];

    uart_print("\r\n--- START FAN CONTROL SIMULATION ---\r\n");
    sprintf(uart_buf, "Target Setpoint Initially Set to: %.0f C\r\n\n", initial_setpoint);
    uart_print(uart_buf);

    while (1)
    {
        if ((ms_ticks - last_temp_update) >= 1000)
        {
            T2 += temp_step;
            if (T2 >= 32.0f || T2 <= 20.0f)
            {
                temp_step = -temp_step;
            }
            last_temp_update = ms_ticks;

            sprintf(uart_buf, "[Temp] T2: %.1f C (T_Diff: %.1f)\r\n", T2, T2 - initial_setpoint);
            uart_print(uart_buf);
        }

        float t_diff = T2 - initial_setpoint;

        int target_pwm = 0;
        int target_rpm = 0;

        if (t_diff > 4.0f)
        {
            target_pwm = 2499;
            target_rpm = 5000;
        }
        else if (t_diff >= 3.0f)
        {
            target_pwm = 1999;
            target_rpm = 4000;
        }
        else if (t_diff >= 2.0f)
        {
            target_pwm = 1499;
            target_rpm = 3000;
        }
        else if (t_diff >= 1.0f)
        {
            target_pwm = 999;
            target_rpm = 2000;
        }
        else if (t_diff > -1.0f)
        {
            target_pwm = 499;
            target_rpm = 1000;
        }
        else
        {
            target_pwm = 0;
            target_rpm = 0;
        }

        if (target_pwm != current_pwm)
        {
            if ((ms_ticks - last_stage_time) >= 15000)
            {
                current_pwm = target_pwm;
                percent_pwm = (current_pwm / 2499) * 100;
                TIM2->CCR1 = current_pwm;
                last_stage_time = ms_ticks;

                sprintf(uart_buf, ">>> [15s DELAY PASSED] FAN UPDATE: PWM = %.0f %% (%d RPM)\r\n\n", percent_pwm, target_rpm);
                uart_print(uart_buf);
            }
        }

        for (volatile uint32_t i = 0; i < 5000; i++)
            ;
    }
}