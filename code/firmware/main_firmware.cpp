/*
 * main.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Firmware entry point
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *
 * BOOT MODES (CURRENT IMPLEMENTATION):
 * -----------------------------------
 *  - CONFIG mode only (service / bring-up)
 *  - RUN mode intentionally NOT implemented yet
 *
 * CONFIG MODE:
 *  - Selected by a physical slide switch
 *  - Latched once per boot
 *  - Provides a console service session
 *  - Exits only when the console requests exit
 *
 * POST-CONFIG BEHAVIOR:
 *  - Firmware intentionally parks forever
 *  - No scheduler, no door logic, no automation
 *  - Reboot is required to re-evaluate boot mode
 *
 * This structure is intentional and enforced during early hardware bring-up.
 *
 * Hardware: Chicken Coop Controller V3.0
 *
 * Updated: 2026-01-05
 */

#include <stdbool.h>

#include "config.h"
#include "config_sw.h"
#include "console/console.h"
#include "uart.h"
#include "relay.h"
#include "rtc.h"
#include "uptime.h"
#include "door_led.h"

int main(void)
{
    /* ------------------------------------------------------------------
     * Basic bring-up
     * ------------------------------------------------------------------ */
    uart_init();
    relay_init();
    uptime_init();
    rtc_init();        /* RTC present; policy handled elsewhere */
    door_led_init();

    /* ------------------------------------------------------------------
     * Configuration validity check
     * ------------------------------------------------------------------ */
    if (!rtc_time_is_set()) {
        /* First power-up or dead battery: demand user attention */
        door_led_set(DOOR_LED_BLINK_RED);
    }

    /* Load persistent configuration (EEPROM, defaults on failure) */
    bool cfg_ok = config_load(&g_cfg);
    (void)cfg_ok;   /* intentionally unused during bring-up */

    /* ------------------------------------------------------------------
     * CONFIG mode handling
     * ------------------------------------------------------------------ */
    static bool config_consumed = false;

    if (config_sw_state() && !config_consumed) {
        config_consumed = true;

        console_init();
        while (!console_should_exit()) {
            console_poll();
            door_led_tick(uptime_millis());
        }
    }

    /* ------------------------------------------------------------------
     * RUN MODE (NOT YET IMPLEMENTED)
     * ------------------------------------------------------------------
     * When implemented, RUN mode will:
     *  - configure RTC alarms
     *  - enter sleep/wake cycles
     *  - run the scheduler
     *  - control door and lock hardware
     *
     * Until then, firmware must remain inert.
     */
    for (;;) {
        door_led_tick(uptime_millis());
    }
}
