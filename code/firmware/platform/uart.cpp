/*
 * uart.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: UART driver (USART0)
 *
 * Configuration:
 *   F_CPU = 8 MHz (internal RC)
 *   Baud  = 38400
 *   Mode  = Normal speed (16x)
 *   Frame = 8N1
 */

#include "uart.h"
#include <avr/io.h>

#define BAUD_RATE 38400UL
#define UBRR_VALUE ((F_CPU / (16UL * BAUD_RATE)) - 1)

void uart_init(void)
{
    /* Normal speed (U2X0 = 0) */
    UCSR0A = 0;

    /* Set baud rate */
    UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
    UBRR0L = (uint8_t)(UBRR_VALUE & 0xFF);

    /* Enable RX and TX */
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);

    /* 8 data bits, no parity, 1 stop bit */
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

int uart_getc(void)
{
    if (!(UCSR0A & (1 << RXC0)))
        return -1;

    return UDR0;
}

void uart_putc(char c)
{
    if (c == '\n') {
        while (!(UCSR0A & (1 << UDRE0)))
            ;
        UDR0 = '\r';
    }

    while (!(UCSR0A & (1 << UDRE0)))
        ;

    UDR0 = c;
}
