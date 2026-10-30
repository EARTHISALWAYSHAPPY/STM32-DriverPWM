#include "Uart.h"
#include "stm32f446xx.h"

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
    while (!(USART2->SR & (0x01 << 7)));
    USART2->DR = (ch & 0xFF);
}

void uart_print(const char *str)
{
    while (*str)
    {
        uart_write(*str++);
    }
}