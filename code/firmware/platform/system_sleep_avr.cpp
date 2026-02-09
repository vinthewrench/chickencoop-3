/*
 * system_sleep_avr.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Low-power sleep implementation for AVR firmware
 *
 * Wake source:
 *   RTC INT → PD2 (INT0)
 *
 * Design:
 *  - No policy
 *  - No scheduling
 *  - No RTC interaction
 *  - No logging
 *
 * Updated: 2026-02-08
 */

#include "system_sleep.h"
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>


/*
 * Initialize RTC wake line (PD2 / INT0).
 *
 * LOW-level trigger required for wake from PWR_DOWN.
 */
void system_sleep_init(void)
{
    /* PD2 input */
    DDRD  &= (uint8_t)~(1u << PD2);

    /* External pull-up exists on board — leave internal off */
    PORTD &= (uint8_t)~(1u << PD2);

    /* LOW-level trigger: ISC01=0, ISC00=0 */
    EICRA &= (uint8_t)~((1u << ISC01) | (1u << ISC00));

    /* Clear pending flag */
    EIFR  |= (uint8_t)(1u << INTF0);

    /* Enable INT0 */
    EIMSK |= (uint8_t)(1u << INT0);
}

/*
 * Enter PWR_DOWN until interrupt occurs.
 */
void system_sleep_until(uint16_t minute)
{
    (void)minute;

    cli();

    /* Clear any stale INT0 flag */
    EIFR |= (uint8_t)(1u << INTF0);

    /*
     * Guard:
     * If RTC INT already asserted (low),
     * do NOT enter sleep.
     */
    if ((PIND & (1u << PD2)) == 0u) {
        sei();
        return;
    }

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();

    sei();
    sleep_cpu();

    /* Execution resumes here */

    cli();
    sleep_disable();
    sei();
}
