/*
 * Minimal UART test for ATmega32U4 - internal 8 MHz
 * Target: 38400 baud, 8N1, normal speed
 * Output: "Test 12345\r\n" every ~1 second on PD3 (TX)
 */

#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>

#define BAUD    9600UL
#define UBRR    ((F_CPU + (BAUD * 8UL)) / (BAUD * 16UL) - 1)  // rounded correctly

static inline void uart_init(void)
{
    UBRR1H = (uint8_t)(UBRR >> 8);
    UBRR1L = (uint8_t)UBRR;

    UCSR1A = 0;                         // normal speed (U2X=0)
    UCSR1B = (1<<TXEN1);                // TX only (simpler)
    UCSR1C = (1<<UCSZ11) | (1<<UCSZ10); // 8N1
}

static inline void uart_putc(char c)
{
    while (!(UCSR1A & (1<<UDRE1))) {}
    UDR1 = c;
}

static inline void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

int main(void)
{
    uart_init();

    uint8_t counter = 0;

    while (1)
    {
        uart_puts("Test ");
        uart_putc('0' + (counter / 10));
        uart_putc('0' + (counter % 10));
        uart_puts("\r\n");

        if (++counter >= 100) counter = 0;

        _delay_ms(980);   // ~1 second
    }
}
