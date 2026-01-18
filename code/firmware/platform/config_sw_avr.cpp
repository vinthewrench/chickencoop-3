/*
 * config_sw_avr.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: CONFIG slide switch (boot-time only)
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *  - CONFIG is sampled once per boot and cached
 *
 * Hardware assumptions (LOCKED):
 *  - CONFIG slide switch is a static strap set BEFORE reset/power-up
 *
 * Electrical behavior (per schematic + verified):
 *  - Switch OPEN    → PC6 pulled HIGH → CONFIG MODE
 *  - Switch CLOSED  → PC6 tied to GND  → NORMAL MODE
 *
 * Firmware rules:
 *  - GPIO direction and pull-up configured in coop_gpio_init()
 *  - CONFIG is sampled once at boot and then ignored
 *  - CONFIG is NOT a wake source
 *
 * Updated: 2026-01-16
 */

#include "config_sw.h"
#include <avr/io.h>
#include "gpio_avr.h"

/*
 * Read CONFIG strap once at boot.
 *
 * Returns:
 *   true  = CONFIG MODE active
 *   false = normal operation
 *
 * ACTIVE-HIGH:
 *   PC6 HIGH -> CONFIG MODE
 *   PC6 LOW  -> normal mode
 */
static bool read_hw_state_once(void)
{
    /* Allow pin + RC + pull-up to settle after reset */
    for (volatile uint8_t i = 0; i < 50; i++) {
        __asm__ __volatile__("nop");
    }

    /* Active-HIGH: HIGH means CONFIG enabled */
    return (PINC & _BV(CONFIG_SW_BIT)) != 0;
}

/*
 * Public API
 *
 * CONFIG switch state is sampled once per boot and cached.
 */
bool config_sw_state(void)
{
    static int8_t cached = -1;

    if (cached < 0) {
        cached = read_hw_state_once() ? 1 : 0;
    }

    return cached != 0;
}
