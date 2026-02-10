/*
 * gpio_avr.h
 *
 * Project: Chicken Coop Controller
 * Purpose: AVR GPIO pin definitions and low-level hardware mapping
 *
 * This header defines the canonical mapping between AVR GPIO pins
 * and physical hardware functions on the Chicken Coop Controller PCB.
 *
 * IMPORTANT DESIGN RULES:
 *  - All mappings in this file are LOCKED to the board schematic.
 *  - Do NOT rename, repurpose, or reassign any pin here unless
 *    the PCB schematic and layout are changed accordingly.
 *  - Higher-level code MUST treat these as hardware truth.
 *
 * SAFETY NOTE:
 *  - Several pins in this file control motors, relays, and solenoids.
 *  - Incorrect initialization or prolonged activation CAN damage hardware.
 *  - All outputs must be initialized to a known-safe OFF state at boot.
 *
 * Updated:
 *   (fill in date when pin mapping changes — not for code-only edits)
 */

#pragma once

/* --------------------------------------------------------------------------
 * Status LED Outputs
 * --------------------------------------------------------------------------
 *
 * These pins drive the front-panel red/green status LED through
 * external driver hardware.
 *
 * Naming convention:
 *  - LED_IN1 / LED_IN2 match the schematic net names.
 *  - Color association is documented for human clarity only.
 *
 * NOTE:
 *  - Do NOT change these names unless the hardware wiring changes.
 * -------------------------------------------------------------------------- */
 #define LED_IN1_BIT   PA1   /* RED   */
 #define LED_IN2_BIT   PA0   /* GREEN */

/* --------------------------------------------------------------------------
 * Door Motor H-Bridge Control (VNH7100BASTR)
 * --------------------------------------------------------------------------
 *
 * This H-bridge drives the main chicken coop door actuator.
 *
 * Electrical notes:
 *  - The door actuator includes internal end-of-travel limit switches.
 *  - Despite this, firmware MUST still enforce a maximum travel time
 *    as a fail-safe against wiring or switch failure.
 *
 * Pin behavior:
 *  - DOOR_EN   : Enables/disables the H-bridge (active HIGH)
 *  - DOOR_INA  : Direction control A
 *  - DOOR_INB  : Direction control B
 *
 * Reset behavior:
 *  - DOOR_EN MUST be driven LOW at boot.
 *  - INA/INB MUST be driven LOW at boot.
 *
 * -------------------------------------------------------------------------- */
/* DOOR_INA -> PF5 (physical pin 38) */
/* DOOR_INB -> PF6 (physical pin 37) */
/* DOOR_EN  -> PF7 (physical pin 36) */
#define DOOR_INA_BIT   PA5
#define DOOR_INB_BIT   PA6
#define DOOR_EN_BIT    PA7

/* --------------------------------------------------------------------------
 * Door Lock Actuator H-Bridge (VNH7100BASTR)
 * --------------------------------------------------------------------------
 *
 * This H-bridge drives the door LOCK actuator.
 *
 * CRITICAL SAFETY WARNING:
 *  - The lock actuator does NOT have end-of-travel limit switches.
 *  - It MUST be driven using a SHORT, FIXED-DURATION PULSE ONLY.
 *  - Continuous drive WILL overheat and destroy the actuator.
 *
 * Firmware requirements:
 *  - LOCK_EN MUST never be left asserted continuously.
 *  - A hard maximum pulse time (lock_pulse_ms) is mandatory.
 *  - Lock control must be edge-triggered, not state-held.
 *
 * JTAG NOTE:
 *  - LOCK_EN uses PF4, which is a JTAG pin by default.
 *  - Runtime JTAG disable is REQUIRED before driving this pin.
 * -------------------------------------------------------------------------- */
/* LOCK_INA -> PF0 */
/* LOCK_INB -> PF1 */
/* LOCK_EN  -> PF4 */
#define LOCK_INA_BIT   PA2
#define LOCK_INB_BIT   PA3
#define LOCK_EN_BIT    PA4

/* --------------------------------------------------------------------------
 * Latching Relay Outputs
 * --------------------------------------------------------------------------
 *
 * These pins drive a dual latching relay via external driver circuitry.
 *
 * Relay behavior:
 *  - SET and RESET are separate coils.
 *  - Coils must be pulsed briefly; they must NOT be held energized.
 *
 * Firmware requirements:
 *  - All relay outputs must default to OFF at boot.
 *  - Pulse duration must be limited in software.
 * -------------------------------------------------------------------------- */
#define RELAY1_SET_BIT    PD5
#define RELAY1_RESET_BIT  PD4
#define RELAY2_SET_BIT    PD6
#define RELAY2_RESET_BIT  PD7


/* --------------------------------------------------------------------------
 * Configuration Mode Slide Switch
 * --------------------------------------------------------------------------
 *
 * Electrical behavior (LOCKED):
 *  - Switch OPEN    → PC6 pulled HIGH → CONFIG MODE
 *  - Switch CLOSED  → PC6 tied to GND  → NORMAL MODE
 *
 * Firmware requirements:
 *  - Pin configured as INPUT
 *  - Internal pull-up enabled
 *  - Logic is ACTIVE-HIGH for CONFIG
 *
 * Read example:
 *   bool config_mode = (PINC & (1u << CONFIG_SW_BIT)) != 0;
 * -------------------------------------------------------------------------- */
#define CONFIG_SW_BIT   PC6


#define DOOR_SW_BIT     PD3


#define RTC_INT_BIT     PD2



/* --------------------------------------------------------------------------
 * GPIO Initialization
 * --------------------------------------------------------------------------
 *
 * coop_gpio_init():
 *  - Configures all actuator-related pins as outputs
 *  - Forces a known-safe OFF state on all drivers
 *
 * This function MUST be called:
 *  - At the very start of main()
 *  - Before any scheduler, state machine, or interrupt enables
 *
 * Failure to do so can result in:
 *  - Motors energizing unexpectedly
 *  - Solenoids overheating
 *  - Hardware damage
 * -------------------------------------------------------------------------- */
void coop_gpio_init(void);


/* --------------------------------------------------------------------------
 * External Wake Inputs (INT0 / INT1)
 * -------------------------------------------------------------------------- */

static inline void gpio_rtc_int_input_init(void)
{
    /* RTC_INT uses external pull-up */
    DDRD  &= (uint8_t)~(1u << RTC_INT_BIT);
    PORTD &= (uint8_t)~(1u << RTC_INT_BIT);
}

static inline void gpio_door_sw_input_init(void)
{
    /* Door switch uses internal pull-up */
    DDRD  &= (uint8_t)~(1u << DOOR_SW_BIT);
    PORTD |=  (uint8_t)(1u << DOOR_SW_BIT);
}

static inline uint8_t gpio_rtc_int_is_asserted(void)
{
    return (PIND & (1u << RTC_INT_BIT)) == 0u;
}

static inline uint8_t gpio_door_sw_is_asserted(void)
{
    return (PIND & (1u << DOOR_SW_BIT)) == 0u;
}
