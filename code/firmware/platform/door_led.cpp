/*
 * door_led.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door switch LED control (firmware implementation)
 *
 * Responsibilities:
 *  - Drive bi-color illuminated door switch LED
 *  - Provide simple semantic LED modes (steady, pulse, blink)
 *  - Own all GPIO interaction for the door LED
 *
 * Notes:
 *  - Firmware-only implementation
 *  - Timing behavior is fixed and internal
 *  - No policy decisions in this module
 *  - Pulse modes are one-shot and self-terminate to OFF
 *  - Host provides a no-op / logging implementation
 *
 * Hardware assumptions (LOCKED, V3.0):
 *  - LED driven via DRV8212DRLR
 *  - PB5 -> DRV8212 IN1 (GREEN)
 *  - PB6 -> DRV8212 IN2 (RED)
 *  - LED polarity controlled by H-bridge direction
 *
 * Updated: 2026-01-05
 */

 #include "gpio_avr.h"

#include "door_led.h"

#include <avr/io.h>

#define LED_PULSE_MS   800
#define LED_BLINK_ON   400
#define LED_BLINK_OFF  400

static door_led_mode_t current = DOOR_LED_OFF;
static uint32_t        t0 = 0;

/* --- low-level drive --- */

static inline void led_off(void)
{
    PORTB &= ~((1 << LED_IN1) | (1 << LED_IN2));
}

static inline void led_green(void)
{
    PORTB |=  (1 << LED_IN1);
    PORTB &= ~(1 << LED_IN2);
}

static inline void led_red(void)
{
    PORTB |=  (1 << LED_IN2);
    PORTB &= ~(1 << LED_IN1);
}

/* --- public API --- */

void door_led_init(void)
{
    DDRB |= (1 << LED_IN1) | (1 << LED_IN2);
    led_off();
    current = DOOR_LED_OFF;
    t0 = 0;
}

void door_led_set(door_led_mode_t mode)
{
    if (mode == current)
        return;

    current = mode;
    t0 = 0;

    /* Immediate visual update */
    switch (current) {
    case DOOR_LED_OFF:
        led_off();
        break;

    case DOOR_LED_RED:
        led_red();
        break;

    case DOOR_LED_GREEN:
        led_green();
        break;

    case DOOR_LED_PULSE_RED:
    case DOOR_LED_PULSE_GREEN:
    case DOOR_LED_BLINK_RED:
    case DOOR_LED_BLINK_GREEN:
        led_off();
        break;
    }
}

void door_led_tick(uint32_t now_ms)
{
    uint32_t dt = now_ms - t0;

    switch (current) {

    /* ---------- PULSE (one-shot) ---------- */

    case DOOR_LED_PULSE_GREEN:
        if (dt < LED_PULSE_MS) {
            led_green();
        } else {
            led_off();
            current = DOOR_LED_OFF;
        }
        break;

    case DOOR_LED_PULSE_RED:
        if (dt < LED_PULSE_MS) {
            led_red();
        } else {
            led_off();
            current = DOOR_LED_OFF;
        }
        break;

    /* ---------- BLINK ---------- */

    case DOOR_LED_BLINK_GREEN:
        if (dt >= (LED_BLINK_ON + LED_BLINK_OFF))
            t0 = now_ms;

        if (dt < LED_BLINK_ON)
            led_green();
        else
            led_off();
        break;

    case DOOR_LED_BLINK_RED:
        if (dt >= (LED_BLINK_ON + LED_BLINK_OFF))
            t0 = now_ms;

        if (dt < LED_BLINK_ON)
            led_red();
        else
            led_off();
        break;

    default:
        break;
    }
}
