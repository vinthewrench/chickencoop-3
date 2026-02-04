/*
 * led_state_machine.cpp
 */

#include "led_state_machine.h"
#include "door_led.h"

#define BLINK_PERIOD_MS   250u
#define PULSE_PERIOD_MS  1200u   /* full breathe cycle */

/* --------------------------------------------------------------------------
 * Internal state
 * -------------------------------------------------------------------------- */

static led_mode_t  g_mode  = LED_OFF;
static led_color_t g_color = LED_GREEN;

static uint32_t g_t0_ms = 0;
static bool     g_led_on = false;

/* Apple-style breathing envelope (PWM duty) */
static const uint8_t pulse_lut[] = {
      0,  1,  2,  4,  7, 11, 16, 22,
     29, 37, 46, 56, 67, 79, 92,106,
    121,137,154,172,191,211,232,255,
    232,211,191,172,154,137,121,106,
     92, 79, 67, 56, 46, 37, 29, 22,
     16, 11,  7,  4,  2,  1
};

#define PULSE_STEPS (sizeof(pulse_lut))

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

static void led_apply(bool on, uint8_t duty)
{
    if (!on) {
        door_led_off();
        return;
    }

    if (g_color == LED_GREEN)
        door_led_green_pwm(duty);
    else
        door_led_red_pwm(duty);
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void led_state_machine_init(void)
{
    g_mode   = LED_OFF;
    g_color  = LED_GREEN;
    g_t0_ms  = 0;
    g_led_on = false;

    door_led_init();
    door_led_off();
}

void led_state_machine_set(led_mode_t mode, led_color_t color)
{
    g_mode   = mode;
    g_color  = color;
    g_t0_ms  = 0;
    g_led_on = false;
}

bool led_state_machine_is_on(void)
{
    return g_led_on;
}

static void door_led_tick_if_due(uint32_t now_ms)
{
    static uint32_t last_ms = 0;

    if ((uint32_t)(now_ms - last_ms) >= 1) {
        last_ms = now_ms;
        door_led_tick();
    }
}

void led_state_machine_tick(uint32_t now_ms)
{

    door_led_tick_if_due(now_ms);

    switch (g_mode) {

    case LED_OFF:
        g_led_on = false;
        led_apply(false, 0);
        break;

    case LED_ON:
        g_led_on = true;
        led_apply(true, 255);
        break;

    case LED_BLINK:
        if (g_t0_ms == 0)
            g_t0_ms = now_ms;

        if ((uint32_t)(now_ms - g_t0_ms) >= BLINK_PERIOD_MS) {
            g_led_on = !g_led_on;
            g_t0_ms = now_ms;
        }

        led_apply(g_led_on, 255);
        break;

    case LED_PULSE: {
        if (g_t0_ms == 0)
            g_t0_ms = now_ms;

        uint32_t elapsed = now_ms - g_t0_ms;
        uint32_t step = (elapsed * PULSE_STEPS) / PULSE_PERIOD_MS;
        step %= PULSE_STEPS;

        g_led_on = true;
        led_apply(true, pulse_lut[step]);
        break;
    }

    default:
        break;
    }
}
