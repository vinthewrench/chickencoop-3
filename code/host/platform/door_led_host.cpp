/*
 * door_led_host.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door LED hardware stub (host implementation)
 *
 * Responsibilities:
 *  - Stand-in for LED hardware on host builds
 *  - Emit concise traces when LED output changes
 *
 * Notes:
 *  - No physical LED on host
 *  - No timing or animation emulation
 *  - State machine runs normally on host
 *  - This layer only reflects *final output intent*
 *
 * Updated: 2026-01-07
 */

#include "door_led.h"
#include <stdio.h>
#include <stdint.h>

/* Track last applied output for noise-free logging */
typedef enum {
    LED_HW_OFF = 0,
    LED_HW_GREEN,
    LED_HW_RED
} led_hw_state_t;

static led_hw_state_t current = LED_HW_OFF;
static uint8_t        current_duty = 0;

void door_led_init(void)
{
    current = LED_HW_OFF;
    current_duty = 0;
    printf("[LED] INIT\n");
}

void door_led_off(void)
{
    if (current == LED_HW_OFF)
        return;

    current = LED_HW_OFF;
    current_duty = 0;
 //   printf("[LED] OFF\n");
}

void door_led_green_pwm(uint8_t duty)
{
    if (current == LED_HW_GREEN && current_duty == duty)
        return;

    current = LED_HW_GREEN;
    current_duty = duty;

 //   printf("[LED] GREEN duty=%u\n", duty);
}

void door_led_red_pwm(uint8_t duty)
{
    if (current == LED_HW_RED && current_duty == duty)
        return;

    current = LED_HW_RED;
    current_duty = duty;

 //   printf("[LED] RED duty=%u\n", duty);
}

void door_led_tick(void) {
    // NOP
}
