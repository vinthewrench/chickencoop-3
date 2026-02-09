#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * led_state_machine.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Door status LED state machine
 *
 * Modes:
 *  - LED_OFF   : LED off
 *  - LED_ON    : Solid on
 *  - LED_BLINK : Square-wave blink
 *  - LED_PULSE : Apple-style breathing envelope
 *
 * Notes:
 *  - Platform-agnostic
 *  - All timing handled internally
 *  - Hardware access via door_led_* functions
 *  - Must be serviced periodically via tick()
 */

/* LED presentation modes */
typedef enum {
    LED_OFF = 0,
    LED_ON,
    LED_BLINK,
    LED_PULSE
} led_mode_t;

/* LED color selection */
typedef enum {
    LED_GREEN = 0,
    LED_RED
} led_color_t;

/*
 * Initialize LED state machine.
 *
 * - Resets internal state
 * - Initializes LED hardware
 * - Leaves LED off
 */
void led_state_machine_init(void);

/*
 * Request a new LED mode and color.
 *
 * - Takes effect immediately
 * - Resets any internal timing
 */
void led_state_machine_set(led_mode_t mode,
    led_color_t color,
    uint16_t cycles = 0   // 0 = infinite
);

/*
 * Periodic service function.
 *
 * Parameters:
 *  - now_ms: current system time in milliseconds
 *
 * Must be called regularly to advance timing-based behavior.
 */
void led_state_machine_tick(uint32_t now_ms);

/*
 * Coarse query: is LED currently driven on?
 *
 * Returns:
 *  - true if LED output is active
 *  - false otherwise
 */
bool led_state_machine_is_on(void);
