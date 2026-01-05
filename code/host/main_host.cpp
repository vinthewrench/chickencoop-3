/*
 * main_host.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Host console entry point
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *  - Host provides visibility, not hardware emulation
 *
 * Updated: 2026-01-05
 */

#include "console/console.h"
#include "uptime.h"
#include "door_led.h"

int main(void)
{
    uptime_init();
    door_led_init();

    /* Host has no RTC persistence; always indicate setup-needed */
    door_led_set(DOOR_LED_BLINK_RED);

    console_init();

    while (!console_should_exit()) {
        console_poll();
    }

    return 0;
}
