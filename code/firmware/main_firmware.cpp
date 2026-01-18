/*
 * main_firmware.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Firmware entry point
 *
 * Design intent:
 *  - Firmware executes the SAME scheduler logic as host
 *  - Deterministic, offline, single-threaded
 *  - No alternate execution paths
 *
 * Execution model:
 *  - Scheduler runs on:
 *      * minute boundary
 *      * OR schedule ETag change
 *  - Schedule application is idempotent
 *  - Firmware sleeps between events using RTC alarm
 *
 * Hardware: Chicken Coop Controller V3.0
 *
 * Updated: 2026-01-08
 */

#include <stdbool.h>
#include <stdint.h>
#include <util/delay.h>

#include "config.h"
#include "config_sw.h"

#include "console/console.h"
#include "uptime.h"
#include "rtc.h"
#include "solar.h"
#include "platform/uart.h"
#include "system_sleep.h"

#include "time_dst.h"

#include "scheduler.h"
#include "state_reducer.h"
#include "schedule_apply.h"

#include "devices/devices.h"
#include "devices/led_state_machine.h"

#include "platform/gpio_avr.h"


int main(void)
{
    /* ----------------------------------------------------------
     * Bring-up
     * ---------------------------------------------------------- */
    uptime_init();
    rtc_init();

    coop_gpio_init();

    device_init();
    scheduler_init();

    (void)config_load(&g_cfg);

    /* ----------------------------------------------------------
     * CONFIG MODE (latched once per boot)
     * ---------------------------------------------------------- */
    static bool config_consumed = false;

    if (config_sw_state() && !config_consumed) {

        config_consumed = true;
        console_init();

        if (!rtc_time_is_set())
            led_state_machine_set(LED_BLINK, LED_RED);

        while (!console_should_exit()) {
            console_poll();
            device_tick(uptime_millis());
        }
    }

    /* ----------------------------------------------------------
     * RUN MODE requires valid RTC
     * ---------------------------------------------------------- */
    if (!rtc_time_is_set()) {
        led_state_machine_set(LED_BLINK, LED_RED);
        for (;;) {
            device_tick(uptime_millis());
        }
    }

    /* Brief green = healthy boot */
    led_state_machine_set(LED_ON, LED_GREEN);
    {
        uint32_t t0 = uptime_millis();
        while ((uint32_t)(uptime_millis() - t0) < 1000u) {
            device_tick(uptime_millis());
        }
    }
    led_state_machine_set(LED_OFF, LED_GREEN);

    /* ----------------------------------------------------------
     * Scheduler loop state
     * ---------------------------------------------------------- */
    uint16_t last_minute = 0xFFFF;
    uint32_t last_etag   = 0;

    int last_y  = -1;
    int last_mo = -1;
    int last_d  = -1;

    struct solar_times sol;
    bool have_sol = false;

    /* ----------------------------------------------------------
     * MAIN RUN LOOP
     * ---------------------------------------------------------- */
    for (;;) {

        /* Always clear latched RTC alarm (safe, idempotent) */
        rtc_alarm_clear_flag();

        /* ------------------------------------------------------
         * Read current time
         * ------------------------------------------------------ */
        int y, mo, d, h, m, s;
        rtc_get_time(&y, &mo, &d, &h, &m, &s);

        uint16_t now_minute = (uint16_t)(h * 60 + m);
        uint32_t cur_etag   = schedule_etag();

        bool minute_changed = (now_minute != last_minute);
        bool schedule_dirty = (cur_etag != last_etag);

        /* ------------------------------------------------------
         * If nothing changed, sleep until next event or interrupt
         * ------------------------------------------------------ */
        if (!minute_changed && !schedule_dirty) {
            uint16_t next_min;
            if (scheduler_next_event_minute(&next_min)) {
                system_sleep_until(next_min);
            } else {
                system_sleep_until(now_minute); /* idle sleep */
            }
            continue;
        }

        last_minute = now_minute;
        last_etag   = cur_etag;

        /* ------------------------------------------------------
         * Recompute solar ONCE per calendar day
         * ------------------------------------------------------ */
        if (y != last_y || mo != last_mo || d != last_d) {

            have_sol = false;

            if (g_cfg.latitude_e4 || g_cfg.longitude_e4) {

                double lat = (double)g_cfg.latitude_e4  / 10000.0;
                double lon = (double)g_cfg.longitude_e4 / 10000.0;

                int tz = g_cfg.tz;
                if (g_cfg.honor_dst && is_us_dst(y, mo, d, h))
                    tz += 1;

                have_sol = solar_compute(
                    y, mo, d,
                    lat,
                    lon,
                    (int8_t)tz,
                    &sol
                );
            }

            scheduler_update_day(
                y, mo, d,
                have_sol ? &sol : NULL,
                have_sol
            );

            last_y  = y;
            last_mo = mo;
            last_d  = d;
        }

        /* ------------------------------------------------------
         * APPLY SCHEDULE (same reducer + executor as host)
         * ------------------------------------------------------ */
        {
            struct reduced_state rs;

            size_t used = 0;
            const Event *events = config_events_get(&used);

            if (events && used > 0) {
                state_reducer_run(
                    events,
                    MAX_EVENTS,
                    have_sol ? &sol : NULL,
                    now_minute,
                    &rs
                );

                schedule_apply(&rs);
            }
        }

        /* ------------------------------------------------------
         * Program next wake
         * ------------------------------------------------------ */
        uint16_t next_min;
        if (scheduler_next_event_minute(&next_min)) {
            rtc_alarm_set_minute_of_day(next_min);
            system_sleep_until(next_min);
        } else {
            system_sleep_until(now_minute);
        }
    }
}
