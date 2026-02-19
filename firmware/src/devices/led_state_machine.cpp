/*
 * led_state_machine.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door status LED state machine
 *
 * Notes:
 *  - Non-blocking at the state-machine level
 *  - Software PWM carrier is driven by repeated door_led_tick() calls
 *  - Pulse envelope is rate-limited for smooth breathing
 *
 * Extended:
 *  - Blink/Pulse may run finite number of cycles
 *  - count = 0 → infinite
 *  - count > 0 → run exactly N cycles then auto-off
 */

#include "led_state_machine.h"
#include "door_led.h"

#include <stdint.h>
#include <stdbool.h>

/* --------------------------------------------------------------------------
 * Timing
 * -------------------------------------------------------------------------- */

#define BLINK_PERIOD_MS   250u
#define PULSE_PERIOD_MS  2800u
#define PWM_TICKS_PER_MS  128u

/* --------------------------------------------------------------------------
 * Internal state
 * -------------------------------------------------------------------------- */

static led_mode_t  g_mode  = LED_OFF;
static led_color_t g_color = LED_GREEN;

/* Optional finite-cycle support */
static uint16_t g_cycles_remaining = 0;   /* 0 = infinite */
static uint16_t g_cycle_counter    = 0;   /* counts completed cycles */

static uint32_t g_blink_t0_ms = 0;
static bool     g_led_on      = false;

/* Pulse timing (in PWM ticks) */
static uint32_t g_pulse_last_ticks = 0;
static uint8_t  g_pulse_step       = 0;
static uint32_t g_pwm_ticks        = 0;
static uint32_t g_pulse_err        = 0;

static int8_t g_pulse_dir = -1;   /* -1 = decaying, +1 = rising */

/* --------------------------------------------------------------------------
 * Perceptual breathing envelopes
 * -------------------------------------------------------------------------- */

 static const uint8_t pulse_lut_green[] = {
       1,  1,  2,  3,  5,  8, 12, 17,
      23, 30, 38, 47, 57, 68, 80, 93,
     107,122,138,155,173,192,212,233,255,
     233,212,192,173,155,138,122,107,
      93, 80, 68, 57, 47, 38, 30, 23,
      17, 12,  8,  5,  3,  2,  1
 };

 static const uint8_t pulse_lut_red[] = {
       1,  2,  4,  7, 11, 16, 22, 29,
      37, 46, 56, 67, 79, 92,106,121,
     137,154,172,191,211,232,248,255,
     248,232,211,191,172,154,137,121,
     106, 92, 79, 67, 56, 46, 37, 29,
      22, 16, 11,  7,  4,  2,  1,
 };

#define PULSE_STEPS_GREEN ((uint8_t)sizeof(pulse_lut_green))
#define PULSE_STEPS_RED   ((uint8_t)sizeof(pulse_lut_red))

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

static inline void led_apply(bool on, uint8_t duty)
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

static void door_led_pwm_service(uint32_t now_ms)
{
    static uint32_t last_ms = 0;

    uint32_t elapsed = now_ms - last_ms;
    if (elapsed == 0)
        return;

    last_ms = now_ms;

    uint32_t ticks = elapsed * PWM_TICKS_PER_MS;

    const uint32_t MAX_TICKS = 10u * PWM_TICKS_PER_MS;
    if (ticks > MAX_TICKS)
        ticks = MAX_TICKS;

    while (ticks--) {
        door_led_tick();
        g_pwm_ticks++;
    }
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief Initialize LED state machine.
 */
void led_state_machine_init(void)
{
    g_mode              = LED_OFF;
    g_color             = LED_GREEN;
    g_cycles_remaining  = 0;
    g_cycle_counter     = 0;

    g_blink_t0_ms       = 0;
    g_led_on            = false;

    g_pulse_last_ticks  = 0;
    g_pulse_step        = 0;
    g_pwm_ticks         = 0;
    g_pulse_err         = 0;

    door_led_init();
    door_led_off();
}


/**
 * @brief Set LED mode with optional finite cycle count.
 *
 * @param mode   LED mode
 * @param color  LED color
 * @param count  Number of cycles (0 = infinite)
 */
 void led_state_machine_set(led_mode_t mode,
                            led_color_t color,
                            uint16_t count)
 {
     g_mode             = mode;
     g_color            = color;
     g_cycles_remaining = count;
     g_cycle_counter    = 0;

     g_blink_t0_ms      = 0;
     g_led_on           = false;

     g_pulse_last_ticks = 0;
     g_pulse_step       = 0;
     g_pulse_err        = 0;
     g_pulse_dir        = -1;   /* default */

     if (mode == LED_OFF) {
         door_led_off();
         return;
     }

     if (mode == LED_ON) {
         g_led_on = true;
         led_apply(true, 255);
         return;
     }

     if (mode == LED_PULSE) {

         g_led_on = true;

         /* Start at peak and decay */
         if (color == LED_GREEN)
             g_pulse_step = PULSE_STEPS_GREEN - 1;
         else
             g_pulse_step = PULSE_STEPS_RED - 1;

         g_pulse_last_ticks = g_pwm_ticks;
         g_pulse_err        = 0;
         g_pulse_dir        = -1;   /* decay first */
     }
 }

/**
 * @brief Backward-compatible overload (infinite cycles).
 */
void led_state_machine_set(led_mode_t mode,
                           led_color_t color)
{
    led_state_machine_set(mode, color, 0);
}

bool led_state_machine_is_on(void)
{
    return g_led_on;
}

/**
 * @brief Service state machine.
 *
 * Must be called periodically.
 */
 void led_state_machine_tick(uint32_t now_ms)
 {
     door_led_pwm_service(now_ms);

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

         if (g_blink_t0_ms == 0)
             g_blink_t0_ms = now_ms;

         if ((uint32_t)(now_ms - g_blink_t0_ms) >= BLINK_PERIOD_MS) {

             g_led_on = !g_led_on;
             g_blink_t0_ms = now_ms;

             /* Count full cycle on falling edge (ON->OFF) */
             if (!g_led_on && g_cycles_remaining > 0) {

                 g_cycle_counter++;

                 if (g_cycle_counter >= g_cycles_remaining) {
                     g_mode = LED_OFF;
                     door_led_off();
                     return;
                 }
             }
         }

         led_apply(g_led_on, 255);
         break;

     case LED_PULSE: {

         const uint8_t *lut;
         uint8_t steps;

         if (g_color == LED_GREEN) {
             lut   = pulse_lut_green;
             steps = PULSE_STEPS_GREEN;
         } else {
             lut   = pulse_lut_red;
             steps = PULSE_STEPS_RED;
         }

         const uint32_t period_ticks =
             (uint32_t)PULSE_PERIOD_MS * PWM_TICKS_PER_MS;

         const uint32_t base_step_ticks = period_ticks / steps;
         const uint32_t rem_step_ticks  = period_ticks % steps;

         if (g_pulse_last_ticks == 0) {
             g_pulse_last_ticks = g_pwm_ticks;
             g_pulse_step       = 0;
             g_pulse_err        = 0;
             g_pulse_dir        = 1;
         }

         for (;;) {

             uint32_t elapsed =
                 (uint32_t)(g_pwm_ticks - g_pulse_last_ticks);

             uint32_t step_ticks = base_step_ticks;

             g_pulse_err += rem_step_ticks;
             if (g_pulse_err >= steps) {
                 g_pulse_err -= steps;
                 step_ticks += 1u;
             }

             if (elapsed < step_ticks)
                 break;

             g_pulse_last_ticks += step_ticks;

             /* Move in current direction */
             g_pulse_step += g_pulse_dir;

             /* Bounce at ends instead of wrap */
             if (g_pulse_step == 0 || g_pulse_step == steps - 1) {

                 g_pulse_dir = -g_pulse_dir;

                 /* Count full cycle when we hit bottom */
                 if (g_pulse_step == 0 && g_cycles_remaining > 0) {

                     g_cycle_counter++;

                     if (g_cycle_counter >= g_cycles_remaining) {
                         g_mode = LED_OFF;
                         door_led_off();
                         return;
                     }
                 }
             }
         }

         g_led_on = true;
         led_apply(true, lut[g_pulse_step]);
         break;
     }

     default:
         break;
     }
 }
