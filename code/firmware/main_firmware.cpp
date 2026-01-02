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
 *  BOOT MODES (CURRENT IMPLEMENTATION):
 *  -----------------------------------
 *  - CONFIG mode only (service / bring-up)
 *  - No RUN mode implemented yet
 *
 *  CONFIG MODE:
 *  - Selected by a physical slide switch
 *  - Latched once per boot
 *  - Provides a console service session
 *  - Exits only when the console requests exit
 *
 *  POST-CONFIG BEHAVIOR:
 *  - Firmware intentionally parks forever
 *  - No scheduler, no door logic, no automation yet
 *  - Reboot is required to re-evaluate boot mode
 *
 *  This structure is intentional during early bring-up.
 *
 * Updated: 2026-01-02
 */

#include "config.h"
#include "config_sw.h"
#include "console/console.h"
#include "uart.h"
#include "relay.h"
#include "uptime.h"

int main(void)
{
    uart_init();
    relay_init();
    uptime_init();

    /* Load persistent configuration (EEPROM, defaults on failure) */
    bool cfg_ok = config_load(&g_cfg);
    (void)cfg_ok;   /* unused during bring-up */

    /*
     * CONFIG mode is a boot-time service session selected
     * by a physical slide switch.
     *
     * The switch is sampled once per boot and latched.
     * Even if the switch remains asserted, CONFIG is
     * entered only once.
     */
    static bool config_consumed = false;

    if (config_sw_state() && !config_consumed) {
        config_consumed = true;

        console_init();
        while (!console_should_exit())
            console_poll();
    }

    /*
     * No RUN mode yet.
     *
     * After CONFIG exit (or if CONFIG was not entered),
     * the firmware intentionally parks forever.
     *
     * A reboot is required to:
     *  - re-evaluate the CONFIG switch
     *  - enter CONFIG again
     *  - eventually enter RUN mode when implemented
     */
    for (;;);
}
