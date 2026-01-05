/*
 * door_led_host.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door switch LED control (host implementation)
 *
 * Responsibilities:
 *  - Provide a host-side stand-in for door LED behavior
 *  - Emit concise console traces on LED state changes
 *
 * Notes:
 *  - Host build has no physical LED
 *  - Timing behavior is NOT emulated
 *  - Output occurs only on mode transitions
 *  - Intended strictly for visibility and debugging
 *
 * Updated: 2026-01-05
 */

#include "door_led.h"
#include <stdio.h>

static door_led_mode_t current = DOOR_LED_OFF;

static const char *name(door_led_mode_t m)
{
    switch (m) {
    case DOOR_LED_OFF:          return "OFF";
    case DOOR_LED_RED:          return "RED";
    case DOOR_LED_GREEN:        return "GREEN";
    case DOOR_LED_PULSE_RED:    return "PULSE_RED";
    case DOOR_LED_PULSE_GREEN:  return "PULSE_GREEN";
    case DOOR_LED_BLINK_RED:    return "BLINK_RED";
    case DOOR_LED_BLINK_GREEN:  return "BLINK_GREEN";
    default:                    return "UNKNOWN";
    }
}

void door_led_init(void)
{
    current = DOOR_LED_OFF;
}

void door_led_set(door_led_mode_t mode)
{
    if (mode == current)
        return;

    current = mode;
    printf("[LED] %s\n", name(mode));
}

void door_led_tick(uint32_t now_ms)
{
    (void)now_ms;
    /* no timing emulation on host */
}
