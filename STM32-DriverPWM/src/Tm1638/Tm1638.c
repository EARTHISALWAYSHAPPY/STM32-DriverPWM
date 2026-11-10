#include "tm1638.h"

// Atomic pin control macros using GPIOA BSRR
#define STB_HIGH() (GPIOA->BSRR = (1 << STB_PIN))
#define STB_LOW() (GPIOA->BSRR = (1 << (STB_PIN + 16)))
#define CLK_HIGH() (GPIOA->BSRR = (1 << CLK_PIN))
#define CLK_LOW() (GPIOA->BSRR = (1 << (CLK_PIN + 16)))
#define DIO_HIGH() (GPIOA->BSRR = (1 << DIO_PIN))
#define DIO_LOW() (GPIOA->BSRR = (1 << (DIO_PIN + 16)))
#define DIO_READ() ((GPIOA->IDR & (1 << DIO_PIN)) ? 1 : 0)

// Direction control macros for DIO pin (PA9)
#define DIO_IN()                               \
    {                                          \
        GPIOA->MODER &= ~(3 << (DIO_PIN * 2)); \
    }
#define DIO_OUT()                                                                     \
    {                                                                                 \
        GPIOA->MODER = (GPIOA->MODER & ~(3 << (DIO_PIN * 2))) | (1 << (DIO_PIN * 2)); \
    }

// 7-segment digit font table (0-9) for Common Cathode
const uint8_t font7seg[] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F // 0-9
};

// Software blocking delay
void delay_us(uint32_t delay)
{
    for (volatile uint32_t i = 0; i < (delay * 16); i++)
        ;
}

// Write 1 byte to TM1638 (LSB first, latched on rising edge)
void TM1638_Write(uint8_t data)
{
    for (int i = 0; i < 8; i++)
    {
        CLK_LOW();
        if (data & 0x01)
            DIO_HIGH();
        else
            DIO_LOW();
        data >>= 1;
        CLK_HIGH();
    }
}

// Read 1 byte from TM1638 (LSB first)
uint8_t TM1638_Read(void)
{
    uint8_t data = 0;
    DIO_IN(); // Switch DIO pin to Input mode
    for (int i = 0; i < 8; i++)
    {
        CLK_LOW();
        delay_us(2);
        CLK_HIGH();
        delay_us(2);
        data >>= 1;
        if (DIO_READ())
        {
            data |= 0x80;
        }
    }
    DIO_OUT(); // Revert DIO pin back to Output mode
    return data;
}

// Send a single command byte
void TM1638_SendCommand(uint8_t cmd)
{
    STB_LOW();
    TM1638_Write(cmd);
    STB_HIGH();
}

// Send data byte to a specific display address
void TM1638_SendData(uint8_t addr, uint8_t data)
{
    TM1638_SendCommand(0x44); // Fixed address mode
    STB_LOW();
    TM1638_Write(0xC0 | addr);
    TM1638_Write(data);
    STB_HIGH();
}

// Initialize GPIO pins and TM1638 module
void TM1638_Init(void)
{
    // Enable GPIOA peripheral clock
    RCC->AHB1ENR |= (1 << 0);

    // Configure PA7, PA8, PA9 as General Purpose Output
    GPIOA->MODER &= ~((3 << (STB_PIN * 2)) | (3 << (CLK_PIN * 2)) | (3 << (DIO_PIN * 2)));
    GPIOA->MODER |= ((1 << (STB_PIN * 2)) | (1 << (CLK_PIN * 2)) | (1 << (DIO_PIN * 2)));

    // Set PA9 (DIO) to Open-Drain mode for safe bidirectional bus control
    GPIOA->OTYPER |= (1 << DIO_PIN);

    STB_HIGH();
    CLK_HIGH();
    DIO_HIGH();

    // Clear all display registers (0x00 to addresses 0x00 - 0x0F)
    TM1638_SendCommand(0x40); // Auto-increment address mode
    STB_LOW();
    TM1638_Write(0xC0);
    for (int i = 0; i < 16; i++)
    {
        TM1638_Write(0x00);
    }
    STB_HIGH();

    // Turn on display with maximum brightness (0x8F)
    TM1638_SendCommand(0x8F);
}

// Read and decode buttons S1 to S8 from 4 key-scan bytes
uint8_t TM1638_ReadKeys(void)
{
    uint8_t keys = 0;
    uint8_t bytes[4];

    STB_LOW();
    TM1638_Write(0x42); // Read key-scan command
    delay_us(2);

    for (int i = 0; i < 4; i++)
    {
        bytes[i] = TM1638_Read();
    }
    STB_HIGH();

    // Map raw byte data to bit positions 0-7
    if (bytes[0] & 0x01)
        keys |= (1 << 0); // S1
    if (bytes[1] & 0x01)
        keys |= (1 << 1); // S2
    if (bytes[2] & 0x01)
        keys |= (1 << 2); // S3
    if (bytes[3] & 0x01)
        keys |= (1 << 3); // S4
    if (bytes[0] & 0x10)
        keys |= (1 << 4); // S5
    if (bytes[1] & 0x10)
        keys |= (1 << 5); // S6
    if (bytes[2] & 0x10)
        keys |= (1 << 6); // S7
    if (bytes[3] & 0x10)
        keys |= (1 << 7); // S8

    return keys;
}