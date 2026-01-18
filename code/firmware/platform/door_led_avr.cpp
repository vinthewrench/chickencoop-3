/*
 * door_led.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door status LED hardware driver (AVR)
 *
 * Hardware model (LOCKED, V3.0):
 *  - Indicator is a BIPOLAR (anti-parallel) two-lead LED.
 *  - Color is determined solely by CURRENT DIRECTION,
 *    not by individual LED pins.
 *
 * Drive topology:
 *  - LED is driven by a DRV8212DRLR H-bridge.
 *  - PB5 -> DRV8212 IN1
 *  - PB6 -> DRV8212 IN2
 *
 * Electrical behavior:
 *  - IN1=1, IN2=0  → current flows IN1 → IN2 → LED shows RED
 *  - IN1=0, IN2=1  → current flows IN2 → IN1 → LED shows GREEN
 *  - IN1=0, IN2=0  → LED off (coast)
 *  - IN1=1, IN2=1  → brake (MUST NOT be used)
 *
 * Software contract:
 *  - This module maps CURRENT DIRECTION → COLOR.
 *  - Public API semantics are:
 *
 *        door_led_green_pwm() → GREEN indication
 *        door_led_red_pwm()   → RED indication
 *
 *  - Any physical polarity, wiring orientation, or LED package
 *    details are abstracted HERE and nowhere else.
 *
 * Design rules:
 *  - Hardware-only layer (no timing or animation logic)
 *  - Only one H-bridge direction active at a time
 *  - Outputs always driven to a safe, non-brake state
 *  - Calls are idempotent and side-effect free
 *
 * Updated: 2026-01-16
 */

#include "door_led.h"

#include <avr/io.h>
#include <stdint.h>

#include "gpio_avr.h"


/* --------------------------------------------------------------------------
 * Pin definitions (PHYSICAL REALITY)
 * -------------------------------------------------------------------------- */

/*
 * PB5 / IN1 lights RED
 * PB6 / IN2 lights GREEN
 *
 * Do NOT change these names unless hardware changes.
 */
#define LED_IN1   PB5   /* PHYSICAL: RED */
#define LED_IN2   PB6   /* PHYSICAL: GREEN */

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

static void pwm_disable_all(void)
{
    /*
     * Fully disconnect OC1A / OC1B.
     * Clear BOTH COM bits to avoid ghost drive or brake states.
     */
    TCCR1A &= ~(
        (1 << COM1A1) | (1 << COM1A0) |
        (1 << COM1B1) | (1 << COM1B0)
    );

    /* Force both driver inputs low (coast / LEDs off) */
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

    /*
     * Timer1 configuration:
     *  - Fast PWM, 8-bit
     *  - Clock prescaler = 8
     *
     * NOTE:
     *  PWM output routing (OC1A / OC1B) is used only as
     *  a duty generator. Logical color mapping is handled
     *  explicitly below.
     */
    TCCR1A = (1 << WGM10);              /* WGM10=1, WGM11=0 */
    TCCR1B = (1 << WGM12) | (1 << CS11);

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

/*
 * GREEN LED control
 *
 * PHYSICAL REALITY:
 *  - GREEN LED is driven by IN2 (PB6)
 *  - Uses OCR1B / OC1B
 */
void door_led_green_pwm(uint8_t duty)
{
    pwm_init_once();

    /* Disable RED channel (IN1 / OC1A) */
    TCCR1A &= ~((1 << COM1A1) | (1 << COM1A0));
    PORTB  &= ~(1 << LED_IN1);

    /* Set GREEN duty */
    OCR1B = duty;

    /* Enable GREEN PWM (OC1B) */
    TCCR1A |= (1 << COM1B1);
}

/*
 * RED LED control
 *
 * PHYSICAL REALITY:
 *  - RED LED is driven by IN1 (PB5)
 *  - Uses OCR1A / OC1A
 */
void door_led_red_pwm(uint8_t duty)
{
    pwm_init_once();

    /* Disable GREEN channel (IN2 / OC1B) */
    TCCR1A &= ~((1 << COM1B1) | (1 << COM1B0));
    PORTB  &= ~(1 << LED_IN2);

    /* Set RED duty */
    OCR1A = duty;

    /* Enable RED PWM (OC1A) */
    TCCR1A |= (1 << COM1A1);
}
