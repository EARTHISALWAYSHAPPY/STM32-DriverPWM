#ifndef TM1638_H
#define TM1638_H

#include "stm32f446xx.h"
#include <stdint.h>

// Pin definitions (GPIOA)
#define STB_PIN 7
#define CLK_PIN 8
#define DIO_PIN 9

// 7-segment digit font table (0-9)
extern const uint8_t font7seg[];

// Function prototypes
void delay_us(uint32_t delay);
void TM1638_Init(void);
void TM1638_Write(uint8_t data);
uint8_t TM1638_Read(void);
void TM1638_SendCommand(uint8_t cmd);
void TM1638_SendData(uint8_t addr, uint8_t data);
uint8_t TM1638_ReadKeys(void);

#endif /* TM1638_H */