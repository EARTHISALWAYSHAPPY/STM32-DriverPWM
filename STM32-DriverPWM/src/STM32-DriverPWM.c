#include "stm32f446xx.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

// Include custom libraries
#include "Clk\Clk.h"
#include "Reg\Reg.h"
#include "SysTicker\SysTicker.h"
#include "Uart\Uart.h"

// Global Variables
float T2 = 0.0f;
bool Debug = true; // for Debug sim logic only not cal from temp sensor

void input_temp_cal(void)
{
    ADC1->CR2 |= (0x01 << 30);
    while (!(ADC1->SR & (0x01 << 1)));

    uint32_t adc_val = ADC1->DR;

    float v_adc = 3.3f * ((float)adc_val / 4095.0f);
    float r_sensor = v_adc / 0.0001f;

    float t_kelvin = 1.0f / ((1.0f / (15.0f + 273.15f)) + (1.0f / 3820.0f) * log(r_sensor / 2341.0f));
    T2 = t_kelvin - 273.15f;
}

int main(void)
{
    // Initialize Systems using Libraries
    Clk_24Mhz();
    reg_setting();
    serial_uart_init(9600);
    systick_init();

    TIM2->EGR |= (0x01 << 0);

    float initial_setpoint = 23.0;
    float temp_step = 0.75;
    float T2 = 30.0; // comment this line if use sensor get temp

    uint32_t last_stage_time = 0;
    uint32_t last_temp_update = 0;
    float current_pwm = 0;
    float percent_pwm = 0;
    char uart_buf[100];

    uart_print("\r\n--- START FAN CONTROL SYSTEM ---\r\n");
    sprintf(uart_buf, "Target Setpoint Initially Set to: %.0f C\r\n\n", initial_setpoint);
    uart_print(uart_buf);

    while (1)
    {
        // ms_ticks is now imported from SysTicker.h
        if ((ms_ticks - last_temp_update) >= 1000)
        {
            if (!Debug)
            {
                input_temp_cal();
            }
            else
            {
                T2 += temp_step;
                if (T2 >= 32.0f || T2 <= 20.0f)
                {
                    temp_step = -temp_step;
                }
            }
            last_temp_update = ms_ticks;

            sprintf(uart_buf, "[Temp] T2: %.3f C (T_Diff: %.3f)\r\n", T2, T2 - initial_setpoint);
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
        else if (t_diff >= 3.0)
        {
            target_pwm = 1999;
            target_rpm = 4000;
        }
        else if (t_diff >= 2.0)
        {
            target_pwm = 1499;
            target_rpm = 3000;
        }
        else if (t_diff >= 1.0)
        {
            target_pwm = 999;
            target_rpm = 2000;
        }
        else if (t_diff > -1.0)
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
    }
}