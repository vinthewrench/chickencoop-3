#pragma once

#include <stdint.h>

/*
 * door_led_hw.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Low-level door status LED hardware driver
 *
 * Notes:
 *  - Hardware-only layer
 *  - No timing, no state, no policy
 *  - All animation and behavior handled by led_state_machine
 *  - Safe to call repeatedly
 *  - Host build may implement as no-op or log-only
 */

/*
 * Initialize LED hardware.
 *
 * - Configures GPIO and PWM peripherals as needed
 * - Leaves LED in OFF state
 * - Safe to call more than once
 */
void door_led_init(void);

/*
 * Turn LED fully off.
 *
 * - Disables PWM
 * - Forces both LED channels inactive
 */
void door_led_off(void);

/*
 * Drive GREEN LED channel using PWM.
 *
 * Parameters:
 *  - duty: PWM duty cycle (0–255)
 *
 * Behavior:
 *  - Enables PWM on GREEN channel only
 *  - Forces RED channel inactive
 *  - Does not block or delay
 */
void door_led_green_pwm(uint8_t duty);

/*
 * Drive RED LED channel using PWM.
 *
 * Parameters:
 *  - duty: PWM duty cycle (0–255)
 *
 * Behavior:
 *  - Enables PWM on RED channel only
 *  - Forces GREEN channel inactive
 *  - Does not block or delay
 */
void door_led_red_pwm(uint8_t duty);
