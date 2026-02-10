/*
 * mini_printf.cpp
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

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include "uart.h"
#include "mini_printf.h"

static  void console_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

/* print unsigned int with optional zero padding */
static void put_uint_pad(unsigned int v, unsigned int width, char pad)
{
    char buf[10];
    unsigned int i = 0;

    if (v == 0) {
        buf[i++] = '0';
    } else {
        while (v > 0) {
            buf[i++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }

    while (i < width)
        buf[i++] = pad;

    while (i--)
        uart_putc(buf[i]);
}

static void put_int_pad(int v, unsigned int width, char pad)
{
    if (v < 0) {
        uart_putc('-');
        put_uint_pad((unsigned int)(-v), width ? width - 1 : 0, pad);
    } else {
        put_uint_pad((unsigned int)v, width, pad);
    }
}

static void put_latlon_e4(int32_t v)
{
    if (v < 0) {
        uart_putc('-');
        v = -v;
    }

    int32_t deg = v / 10000;
    int32_t frac = v % 10000;

    put_int_pad(deg, 0, ' ');
    uart_putc('.');
    put_uint_pad((unsigned int)frac, 4, '0');
}

/* print unsigned long (32-bit) with optional zero padding */
static void put_ulong_pad(uint32_t v, unsigned int width, char pad)
{
    char buf[10];
    unsigned int i = 0;

    if (v == 0) {
        buf[i++] = '0';
    } else {
        while (v > 0) {
            buf[i++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }

    while (i < width)
        buf[i++] = pad;

    while (i--)
        uart_putc(buf[i]);
}

static void put_long_pad(int32_t v, unsigned int width, char pad)
{
    if (v < 0) {
        uart_putc('-');
        put_ulong_pad((uint32_t)(-v), width ? width - 1 : 0, pad);
    } else {
        put_ulong_pad((uint32_t)v, width, pad);
    }
}

static void put_hex8(uint8_t v)
{
    const char *hex = "0123456789ABCDEF";
    uart_putc(hex[(v >> 4) & 0x0F]);
    uart_putc(hex[v & 0x0F]);
}

void mini_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {

        if (*fmt != '%') {
            uart_putc(*fmt++);
            continue;
        }

        fmt++; /* skip '%' */

        /* parse zero pad */
        char pad = ' ';
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }

        /* parse width */
        unsigned int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (unsigned int)(*fmt - '0');
            fmt++;
        }

        /* parse optional long modifier */
        bool long_flag = false;
        if (*fmt == 'l') {
            long_flag = true;
            fmt++;
        }

        switch (*fmt) {

        case 's':
            console_puts(va_arg(ap, const char *));
            break;

        case 'c':
            uart_putc((char)va_arg(ap, int));
            break;

        case 'u':
            if (long_flag)
                put_ulong_pad(va_arg(ap, uint32_t), width, pad);
            else
                put_uint_pad(va_arg(ap, unsigned int), width, pad);
            break;

        case 'd':
            if (long_flag)
                put_long_pad(va_arg(ap, int32_t), width, pad);
            else
                put_int_pad(va_arg(ap, int), width, pad);
            break;

        case 'L':
            put_latlon_e4(va_arg(ap, int32_t));
            break;

        case 'x':
        case 'X':
            put_hex8((uint8_t)va_arg(ap, unsigned int));
            break;

        case '%':
            uart_putc('%');
            break;

        default:
            uart_putc('?');
            break;
        }

        fmt++;
    }

    va_end(ap);
}
