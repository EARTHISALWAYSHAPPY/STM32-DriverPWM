#ifndef UART_H
#define UART_H

#include <stdint.h>

void serial_uart_init(uint32_t baudrate);
void uart_write(char ch);
void uart_print(const char *str);

#endif /* UART_H */