/*
 * uptime.cpp
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

#include "uptime.h"

#include <avr/interrupt.h>
#include <stdint.h>

// 1 kHz tick using Timer0 in CTC mode.
// F_CPU = 8 MHz, prescaler = 64 -> 125 kHz timer clock.
// OCR0A = 124 gives 1000 Hz.

static volatile uint32_t g_millis = 0;

ISR(TIMER0_COMPA_vect)
{
    g_millis++;
}

void uptime_init(void)
{
    // CTC mode
    TCCR0A = (1 << WGM01);
    // prescaler 64
    TCCR0B = (1 << CS01) | (1 << CS00);
    // compare for 1 ms
    OCR0A = 124;
    // enable compare match interrupt
    TIMSK0 |= (1 << OCIE0A);

    sei();
}

uint32_t uptime_millis(void)
{
    uint32_t ms;
    uint8_t sreg = SREG;
    cli();
    ms = g_millis;
    SREG = sreg;
    return ms;
}

uint32_t uptime_seconds(void)
{
    return uptime_millis() / 1000;
}
