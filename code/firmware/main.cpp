/*
 * main.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Source file
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *
 * Updated: 2025-12-29
 */

 #include "config.h"
 #include "config_sw.h"
 #include "console/console.h"
 #include "uart.h"
 #include "uptime.h"

int main(void)
{
    uart_init();
    uptime_init();

    /* Load persistent configuration (EEPROM, defaults on failure) */
    bool cfg_ok = config_load(&g_cfg);
    (void)cfg_ok;   /* unused for now */

    static bool config_consumed = false;

    /*
     * CONFIG is a boot-time service session selected by a slide switch.
     * The switch is sampled once per boot via config_sw_state().
     */
    // Latch CONFIG to one entry per boot, even if the switch remains asserted.

    if (config_sw_state() && !config_consumed) {
        config_consumed = true;

        console_init();
        while (!console_should_exit())
            console_poll();
    }

    /*
     * No RUN mode yet.
     * After CONFIG exit or timeout, park forever.
     * A reboot re-evaluates the CONFIG switch.
     */
    for (;;);
}
