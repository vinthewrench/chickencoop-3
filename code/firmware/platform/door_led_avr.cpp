/*
 * door_led.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door status LED hardware driver (AVR)
 *
 * Hardware assumptions (LOCKED, V3.0):
 *  - LED driven via DRV8212DRLR
 *  - PB5 -> DRV8212 IN1 (GREEN / OC1A)
 *  - PB6 -> DRV8212 IN2 (RED   / OC1B)
 *
 * Notes:
 *  - Hardware-only layer
 *  - No timing or animation logic
 *  - PWM on one channel at a time
 *  - Safe, idempotent calls
 *
 * Updated: 2026-01-07
 */

#include "door_led.h"

#include <avr/io.h>
#include <stdint.h>

/* --------------------------------------------------------------------------
 * Pin definitions
 * -------------------------------------------------------------------------- */

#define LED_IN1   PB5   /* GREEN / OC1A */
#define LED_IN2   PB6   /* RED   / OC1B */

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

static void pwm_disable_all(void)
{
    /* Disconnect OC1A / OC1B */
    TCCR1A &= ~((1 << COM1A1) | (1 << COM1B1));

    /* Force outputs low */
    PORTB &= ~((1 << LED_IN1) | (1 << LED_IN2));
}

static void pwm_init_once(void)
{
    static uint8_t initialized = 0;
    if (initialized)
        return;

    initialized = 1;

    /* PB5 / PB6 as outputs */
    DDRB |= (1 << LED_IN1) | (1 << LED_IN2);

    /* Timer1: Fast PWM, 8-bit */
    TCCR1A =
        (1 << WGM10);            /* WGM10=1, WGM11=0 */

    TCCR1B =
        (1 << WGM12) |           /* WGM12=1 => Fast PWM 8-bit */
        (1 << CS11);             /* prescaler = 8 */

    /* Start fully off */
    OCR1A = 0;
    OCR1B = 0;

    pwm_disable_all();
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void door_led_init(void)
{
    pwm_init_once();
    pwm_disable_all();
}

void door_led_off(void)
{
    pwm_disable_all();
}

void door_led_green_pwm(uint8_t duty)
{
    pwm_init_once();

    /* Disable RED */
    TCCR1A &= ~(1 << COM1B1);
    PORTB  &= ~(1 << LED_IN2);

    /* Set duty */
    OCR1A = duty;

    /* Enable GREEN PWM */
    TCCR1A |= (1 << COM1A1);
}

void door_led_red_pwm(uint8_t duty)
{
    pwm_init_once();

    /* Disable GREEN */
    TCCR1A &= ~(1 << COM1A1);
    PORTB  &= ~(1 << LED_IN1);

    /* Set duty */
    OCR1B = duty;

    /* Enable RED PWM */
    TCCR1A |= (1 << COM1B1);
}
