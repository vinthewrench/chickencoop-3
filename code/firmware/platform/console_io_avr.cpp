/*
 * console_io_avr.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Console I/O backend (AVR UART)
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *
 * Updated: 2025-12-29
 */

#include "console/console_io.h"
#include "uart.h"

int console_getc(void)
{
    return uart_getc();
}

void console_putc(char c)
{
    uart_putc(c);
}

void console_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}


void console_terminal_init(void){
    uart_init();
}
