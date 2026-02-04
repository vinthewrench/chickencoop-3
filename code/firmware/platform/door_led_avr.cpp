/*
 * door_led.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door status LED driver (software PWM)
 *
 * Hardware:
 *  - LED1 (RED)   -> PA0
 *  - LED2 (GREEN) -> PA1
 *
 * Notes:
 *  - Software PWM
 *  - No timers owned
 *  - Deterministic, non-blocking
 */

#include <avr/io.h>
#include <stdint.h>

#include "door_led.h"
#include "gpio_avr.h"

/* --------------------------------------------------------------------------
 * PWM state
 * -------------------------------------------------------------------------- */

static uint8_t pwm_red   = 0;
static uint8_t pwm_green = 0;
static uint8_t pwm_phase = 0;

/* --------------------------------------------------------------------------
 * Init
 * -------------------------------------------------------------------------- */

void door_led_init(void)
{
    /* PA0 / PA1 outputs */
    DDRA |= (1u << LED_IN1_BIT) | (1u << LED_IN2_BIT);

    /* LEDs off */
    PORTA &= ~((1u << LED_IN1_BIT) | (1u << LED_IN2_BIT));

    pwm_red   = 0;
    pwm_green = 0;
    pwm_phase = 0;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void door_led_off(void)
{
    pwm_red   = 0;
    pwm_green = 0;

    PORTA &= ~((1u << LED_IN1_BIT) | (1u << LED_IN2_BIT));
}

void door_led_red_pwm(uint8_t duty)
{
    pwm_red   = duty;
    pwm_green = 0;
}

void door_led_green_pwm(uint8_t duty)
{
    pwm_green = duty;
    pwm_red   = 0;
}

/* --------------------------------------------------------------------------
 * PWM tick (call at fixed rate, e.g. 1 kHz)
 * -------------------------------------------------------------------------- */

void door_led_tick(void)
{
    pwm_phase++;

    /* RED */
    if (pwm_red && pwm_phase < pwm_red)
        PORTA |=  (1u << LED_IN1_BIT);
    else
        PORTA &= ~(1u << LED_IN1_BIT);

    /* GREEN */
    if (pwm_green && pwm_phase < pwm_green)
        PORTA |=  (1u << LED_IN2_BIT);
    else
        PORTA &= ~(1u << LED_IN2_BIT);
}
