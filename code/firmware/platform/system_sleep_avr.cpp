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

#include "gpio_avr.h"

/*
 * Initialize RTC wake line (PD2 / INT0).
 *
 * LOW-level trigger required for wake from PWR_DOWN.
 */
 void system_sleep_init(void)
 {
     gpio_rtc_int_input_init();
     gpio_door_sw_input_init();

     /* LOW-level trigger required for PWR_DOWN wake */
     EICRA &= (uint8_t)~(
         (1u << ISC01) |
         (1u << ISC00) |
         (1u << ISC11) |
         (1u << ISC10)
     );

     EIFR  |= (uint8_t)((1u << INTF0) | (1u << INTF1));
     EIMSK |= (uint8_t)((1u << INT0) | (1u << INT1));
 }


/*
 * Enter PWR_DOWN until interrupt occurs.
 */
 void system_sleep_until(uint16_t minute)
 {
     (void)minute;

     cli();

     /* Clear stale flags */
     EIFR |= (1u << INTF0) | (1u << INTF1);

     /* Guard against active low lines */
     if (gpio_rtc_int_is_asserted() ||
         gpio_door_sw_is_asserted())
     {
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

     /* Leave INT0/INT1 masked.
        Higher-level code decides when to re-arm. */

     sei();
 }
