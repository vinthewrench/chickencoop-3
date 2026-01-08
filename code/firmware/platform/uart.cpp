/*
 * uart.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Source file
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *
 * Updated: 2025-12-29
 */

#include "uart.h"
#include <avr/io.h>

#define BAUD_RATE 115200UL
#define UBRR_VALUE ((F_CPU / (16UL * BAUD_RATE)) - 1)

void uart_init(void)
{
    // Set baud rate
    UBRR1H = (uint8_t)(UBRR_VALUE >> 8);
    UBRR1L = (uint8_t)(UBRR_VALUE & 0xFF);

    // Enable receiver and transmitter
    UCSR1B = (1 << RXEN1) | (1 << TXEN1);

    // 8 data bits, no parity, 1 stop bit
    UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
}

int uart_getc(void)
{
    // If no data available, return -1 (non-blocking)
    if (!(UCSR1A & (1 << RXC1)))
        return -1;

    return UDR1;
}

void uart_putc(char c)
{
    // Wait until transmit buffer is empty
    while (!(UCSR1A & (1 << UDRE1)))
        ;

    UDR1 = c;
}
