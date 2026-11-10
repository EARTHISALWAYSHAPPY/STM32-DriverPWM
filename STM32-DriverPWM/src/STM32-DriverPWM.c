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
#include "Tm1638\Tm1638.h"

// Global Variables
float T2 = 0.0f;
float r_sensor;
float t_kelvin;

bool Debug = false; // for Debug sim logic only not cal from temp sensor

void input_temp_cal(void)
{
    ADC1->CR2 |= (0x01 << 30);
    while (!(ADC1->SR & (0x01 << 1)))
    {
    }

    uint32_t adc_val = ADC1->DR;

    float v_adc = 3.3f * ((float)adc_val / 4095.0f);

    // Divider with R10K(Pull-Up)
    r_sensor = 10000.0f * ((float)v_adc / (3.3f - (float)v_adc));

    t_kelvin = 1.0f / ((1.0f / (15.0f + 273.15f)) + (1.0f / 3820.0f) * log(r_sensor / 2341.0f));
    T2 = t_kelvin - 273.15f;
}

int main(void)
{
    // Initialize Systems using Libraries
    Clk_24Mhz();
    reg_setting();
    serial_uart_init(9600);
    systick_init();
    TM1638_Init(); // Initialize TM1638 Display and Buttons

    TIM2->EGR |= (0x01 << 0);

    // Scaled setpoint (2500 = 25.00 C)
    int settemp_scaled = 2500;
    float initial_setpoint = (float)settemp_scaled / 100.0f;

    float temp_step = 0.75; // Debug loop step
    if (Debug)
    {
        T2 = 30.0; // comment this line if use sensor get temp
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
        // ---------------- 1. TM1638 Button Scan & Display Task (Every 50ms) ----------------
        if ((ms_ticks - last_display_update) >= 50)
        {
            last_display_update = ms_ticks;

            uint8_t keys = TM1638_ReadKeys();

            // Edge detection for S1 (+0.5) and S2 (-0.5)
            if (keys != last_keys)
            {
                if (keys & (1 << 0)) { settemp_scaled += 50; } // S1: +0.50 C
                if (keys & (1 << 1)) { settemp_scaled -= 50; } // S2: -0.50 C

                // Clamp boundaries (0.00 C to 99.99 C)
                if (settemp_scaled > 9999) settemp_scaled = 9999;
                if (settemp_scaled < 0)    settemp_scaled = 0;

                // Update initial_setpoint used in fan control logic
                initial_setpoint = (float)settemp_scaled / 100.0f;
                last_keys = keys;

                // sprintf(uart_buf, "[SETPOINT CHANGED] New Target: %.2f C\r\n", initial_setpoint);
                // uart_print(uart_buf);
            }

            // Update TM1638 7-Segment Display (Grid 5, 6, 7, 8)
            uint8_t tens_val  = (settemp_scaled / 1000) % 10; // Tens digit
            uint8_t units_val = (settemp_scaled / 100) % 10;  // Units digit
            uint8_t dec1_val  = (settemp_scaled / 10) % 10;   // 1st decimal place
            uint8_t dec2_val  = settemp_scaled % 10;          // 2nd decimal place

            // Blank out leading zero in tens digit position
            uint8_t tens_display = (tens_val == 0) ? 0x00 : font7seg[tens_val];

            TM1638_SendData(8,  tens_display);                     // Grid 5 (0x08): Tens
            TM1638_SendData(10, font7seg[units_val] | 0x80);       // Grid 6 (0x0A): Units + Decimal Point
            TM1638_SendData(12, font7seg[dec1_val]);               // Grid 7 (0x0C): 1st decimal digit
            TM1638_SendData(14, font7seg[dec2_val]);               // Grid 8 (0x0E): 2nd decimal digit
        }

        // ---------------- 2. Temperature Sensing Task (Every 1000ms) ----------------
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

            sprintf(uart_buf, "SetTempPoint = %.3f | [Temp] T2: %.3f C | (T_Diff: %.3f) | Resister = %.3f \r\n", initial_setpoint, T2, T2 - initial_setpoint, r_sensor);
            uart_print(uart_buf);
        }

        // ---------------- 3. PWM Fan Control Logic ----------------
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

        // Apply PWM update after 15-second delay stage
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