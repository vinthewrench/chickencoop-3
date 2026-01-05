#pragma once

#include <stdint.h>

/*
 * Door switch LED control
 *
 * This module provides a small, fixed vocabulary for driving the
 * bi-color door switch LED via platform-specific implementations.
 *
 * Semantics:
 *  - OFF            : LED off
 *  - RED / GREEN    : steady color
 *  - PULSE_*        : finite acknowledgment pulse, auto-returns to OFF
 *  - BLINK_*        : continuous attention indicator, runs until changed
 *
 * Timing and electrical behavior are platform-defined.
 * Callers express intent only.
 */

typedef enum {
    DOOR_LED_OFF = 0,

    DOOR_LED_RED,
    DOOR_LED_GREEN,

    DOOR_LED_PULSE_RED,
    DOOR_LED_PULSE_GREEN,

    DOOR_LED_BLINK_RED,
    DOOR_LED_BLINK_GREEN
} door_led_mode_t;

/* Initialize LED subsystem (idempotent) */
void door_led_init(void);

/* Set LED mode (idempotent) */
void door_led_set(door_led_mode_t mode);

/*
 * Periodic service function.
 * Firmware uses this to advance timing-based behavior.
 * Host implementation is a no-op.
 */
void door_led_tick(uint32_t now_ms);
