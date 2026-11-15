#include "stm32f446xx.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "Clk\Clk.h"
#include "Reg\Reg.h"
#include "SysTicker\SysTicker.h"
#include "Uart\Uart.h"
#include "Tm1638\Tm1638.h"

float T2 = 0.0f;
float r_sensor;
float t_kelvin;

bool Debug = false; // true = simulate temperature without NTC sensor

void input_temp_cal(void)
{
    ADC1->CR2 |= (0x01 << 30); // Start ADC1 conversion
    while (!(ADC1->SR & (0x01 << 1)));

    uint32_t adc_val = ADC1->DR;
    float v_adc = 3.3f * ((float)adc_val / 4095.0f);

    // Calculate NTC resistance (10k pull-up) and convert to Celsius (Steinhart-Hart)
    r_sensor = 10000.0f * (v_adc / (3.3f - v_adc));
    t_kelvin = 1.0f / ((1.0f / (15.0f + 273.15f)) + (1.0f / 3820.0f) * log(r_sensor / 2341.0f));
    T2 = t_kelvin - 273.15f;
}

int main(void)
{
    Clk_24Mhz();
    reg_setting();
    serial_uart_init(9600);
    systick_init();
    TM1638_Init();

    TIM2->EGR |= (0x01 << 0);

    int settemp_scaled = 0; // Range: -3000 (-30.0 C) to +2000 (+20.0 C)
    float initial_setpoint = 0.0f;
    bool show_t2 = false;   // false = show Settemp, true = show T2

    TM1638_SendData(15, 0x00); // LED 8 initially OFF

    float temp_step = 0.75f;
    if (Debug)
    {
        T2 = 30.0f;
    }

    uint32_t last_stage_time = 0;
    uint32_t last_temp_update = 0;
    uint32_t last_display_update = 0;

    uint8_t last_keys = 0;
    float current_pwm = 0;
    float percent_pwm = 0;
    char uart_buf[100];

    uart_print("\r\n--- START FAN CONTROL SYSTEM ---\r\n");
    sprintf(uart_buf, "Target Setpoint Initially Set to: %.2f C\r\n\n", initial_setpoint);
    uart_print(uart_buf);

    while (1)
    {
        // 1. Scan keys and update TM1638 display (every 50ms)
        if ((ms_ticks - last_display_update) >= 50)
        {
            last_display_update = ms_ticks;

            uint8_t keys = TM1638_ReadKeys();
            uint8_t pressed_keys = keys & ~last_keys;

            if (pressed_keys != 0)
            {
                if (pressed_keys & (1 << 0)) settemp_scaled += 50; // S1: +0.5 C
                if (pressed_keys & (1 << 1)) settemp_scaled -= 50; // S2: -0.5 C
                if (pressed_keys & (1 << 7)) show_t2 = !show_t2;   // S8: Toggle Settemp / T2

                if (settemp_scaled > 2000)  settemp_scaled = 2000;
                if (settemp_scaled < -3000) settemp_scaled = -3000;

                initial_setpoint = (float)settemp_scaled / 100.0f;
            }
            last_keys = keys;

            int display_tenths;
            if (show_t2)
            {
                display_tenths = (int)roundf(T2 * 10.0f);
                TM1638_SendData(15, 0x01); // LED 8 ON
            }
            else
            {
                display_tenths = settemp_scaled / 10;
                TM1638_SendData(15, 0x00); // LED 8 OFF
            }

            int abs_tenths = (display_tenths < 0) ? -display_tenths : display_tenths;
            uint8_t tens_val  = (abs_tenths / 100) % 10;
            uint8_t units_val = (abs_tenths / 10) % 10;
            uint8_t dec1_val  = abs_tenths % 10;

            // Right-aligned 1-decimal display (Grid 5: minus sign, Grid 6-8: digits)
            TM1638_SendData(6,  0x00);
            TM1638_SendData(8,  (display_tenths < 0) ? 0x40 : 0x00);
            TM1638_SendData(10, (tens_val == 0) ? 0x00 : font7seg[tens_val]);
            TM1638_SendData(12, font7seg[units_val] | 0x80);
            TM1638_SendData(14, font7seg[dec1_val]);
        }

        // 2. Read temperature and print via UART (every 1000ms)
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

            sprintf(uart_buf, "SetTempPoint = %.3f | [Temp] T2: %.3f C | (T_Diff: %.3f) | Resister = %.3f | PWM = %.0f %% \r\n",
                    initial_setpoint, T2, T2 - initial_setpoint, r_sensor, percent_pwm);
            uart_print(uart_buf);
        }

        // 3. PWM fan control (15s delay before stage change)
        float t_diff = T2 - initial_setpoint;
        int target_pwm = 0;
        int target_rpm = 0;

        if (t_diff > 4.0f)       { target_pwm = 2499; target_rpm = 5000; }
        else if (t_diff >= 3.0f) { target_pwm = 1999; target_rpm = 4000; }
        else if (t_diff >= 2.0f) { target_pwm = 1499; target_rpm = 3000; }
        else if (t_diff >= 1.0f) { target_pwm = 999;  target_rpm = 2000; }
        else if (t_diff > -1.0f) { target_pwm = 499;  target_rpm = 1000; }
        else                     { target_pwm = 0;    target_rpm = 0;    }

        if (target_pwm != current_pwm)
        {
            if ((ms_ticks - last_stage_time) >= 15000)
            {
                current_pwm = target_pwm;
                percent_pwm = (current_pwm / 2499.0f) * 100.0f;
                TIM2->CCR1 = (uint32_t)current_pwm;
                last_stage_time = ms_ticks;

                sprintf(uart_buf, ">>> [15s DELAY PASSED] FAN UPDATE: PWM = %.0f %% (%d RPM)\r\n\n", percent_pwm, target_rpm);
                uart_print(uart_buf);
            }
        }
    }
}