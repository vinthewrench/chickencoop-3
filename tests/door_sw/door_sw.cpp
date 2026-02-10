/*
 * door_sw_led_test.c
 *
 * Minimal door switch test using front panel LEDs.
 * No RTC. No sleep. No UART. No I2C.
 *
 * Expected:
 *  - Door switch released: GREEN on
 *  - Door switch pressed/closed to GND: RED on
 */

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define LED_IN1_BIT   PA1   /* RED   */
#define LED_IN2_BIT   PA0   /* GREEN */


#define DOOR_SW_BIT     PD3



static inline void leds_init(void)
{
    /* PA0/PA1 outputs */
    DDRA |= (uint8_t)((1u << LED_IN1_BIT) | (1u << LED_IN2_BIT));

    /* Start with both OFF (assuming active-high LED inputs to external driver) */
    PORTA &= (uint8_t)~((1u << LED_IN1_BIT) | (1u << LED_IN2_BIT));
}

static inline void door_sw_init(void)
{
    /* PD3 input */
    DDRD &= (uint8_t)~(1u << DOOR_SW_BIT);

    /* Enable internal pull-up (switch pulls to GND when asserted) */
    PORTD |= (uint8_t)(1u << DOOR_SW_BIT);
}

static inline uint8_t door_sw_raw(void)
{
    /* 1 = high (released), 0 = low (asserted) */
    return (PIND & (uint8_t)(1u << DOOR_SW_BIT)) ? 1u : 0u;
}

static inline uint8_t door_sw_is_asserted(void)
{
    /* active-low */
    return door_sw_raw() == 0u;
}

static inline void led_red_on_green_off(void)
{
    PORTA |=  (uint8_t)(1u << LED_IN1_BIT);
    PORTA &= (uint8_t)~(1u << LED_IN2_BIT);
}

static inline void led_green_on_red_off(void)
{
    PORTA |=  (uint8_t)(1u << LED_IN2_BIT);
    PORTA &= (uint8_t)~(1u << LED_IN1_BIT);
}

int main(void)
{
    leds_init();
    door_sw_init();

    /* Show we’re alive: brief blink both */
    PORTA |= (uint8_t)((1u << LED_IN1_BIT) | (1u << LED_IN2_BIT));
    _delay_ms(150);
    PORTA &= (uint8_t)~((1u << LED_IN1_BIT) | (1u << LED_IN2_BIT));
    _delay_ms(150);

    /* Simple debounce: require 5 consecutive identical reads */
    uint8_t stable = door_sw_raw();
    uint8_t same_count = 0;

    while (1) {
        uint8_t r = door_sw_raw();

        if (r == stable) {
            if (same_count < 5) same_count++;
        } else {
            stable = r;
            same_count = 0;
        }

        if (same_count == 5) {
            if (door_sw_is_asserted()) {
                led_red_on_green_off();
            } else {
                led_green_on_red_off();
            }
        }

        _delay_ms(10);
    }
}
