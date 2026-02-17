/*
 * main_firmware.cpp
 *
 * Chicken Coop Controller
 *
 * WAKE TRUTH:
 *   RTC INT → PD2 (INT0)
 *   LOW-level trigger
 *   SLEEP_MODE_PWR_DOWN
 *
 * Anything else is wrong.
 *
 * Time Model (UPDATED):
 *   - RTC returns UTC.
 *   - Scheduler runs in UTC.
 *   - Solar compute is requested in UTC (tz = 0).
 *   - TZ/DST must NOT affect scheduling here.
 *   - TZ/DST are console/UI concerns only.
 *
 * Updated: 2026-02-16
 */

#include <stdbool.h>
#include <stdint.h>
#include <util/delay.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>

#include "config.h"
#include "config_sw.h"

#include "console/console.h"
#include "console/mini_printf.h"

#include "uptime.h"
#include "rtc.h"
#include "solar.h"
#include "platform/uart.h"
#include "system_sleep.h"

/* DST/TZ must not be used for scheduling here. */
// #include "time_dst.h"

#include "scheduler.h"
#include "state_reducer.h"
#include "schedule_apply.h"

#include "devices/devices.h"
#include "devices/led_state_machine.h"
#include "devices/door_state_machine.h"

#include "platform/gpio_avr.h"
#include "platform/i2c.h"


/* ============================================================================
 * INT0 WAKE (PD2)
 * ========================================================================== */

ISR(INT0_vect)
{
    EIMSK &= (uint8_t)~(1u << INT0);
}

static volatile uint8_t g_door_event = 0;

ISR(INT1_vect)
{
    EIMSK &= (uint8_t)~(1u << INT1);
    g_door_event = 1u;
}


/* ============================================================================
 * TIME HELPERS
 * ========================================================================== */

static inline uint16_t minute_of_day(int h, int m)
{
    return (uint16_t)((uint16_t)h * 60u + (uint16_t)m);
}

static inline uint16_t next_minute(uint16_t now_min)
{
    return (uint16_t)((now_min + 1u) % 1440u);
}

static inline uint16_t strictly_future_minute(uint16_t now_min,
                                              uint16_t target_min)
{
    if (target_min <= now_min)
        return next_minute(now_min);
    return target_min;
}


/* ============================================================================
 * RESET CAUSE
 * ========================================================================== */

static uint8_t g_reset_flags;

static void reset_cause_capture_early(void)
{
    g_reset_flags = MCUSR;
    MCUSR = 0;
    wdt_disable();

    MCUCR |= _BV(JTD);
    MCUCR |= _BV(JTD);
}

static void reset_cause_debug_print(void)
{
    if (g_reset_flags & _BV(PORF))  mini_printf("RESET: Power On\n");
    if (g_reset_flags & _BV(BORF))  mini_printf("RESET: Brown-Out\n");
    if (g_reset_flags & _BV(WDRF))  mini_printf("RESET: Watchdog\n");
}


/* ============================================================================
 * MAIN
 * ========================================================================== */

int main(void)
{
    bool rtc_valid = false;

    reset_cause_capture_early();

    if (g_reset_flags & _BV(BORF)) {
        _delay_ms(50);
    }

    uart_init();
    uptime_init();
    coop_gpio_init();

    if (!i2c_init(100000)) {
        led_state_machine_init();
        led_state_machine_set(LED_BLINK, LED_RED);
        for (;;) {
            led_state_machine_tick(uptime_millis());
        }
    }

    rtc_init();

    rtc_valid = rtc_validate_at_boot();

    system_sleep_init();
    sei();

    device_init();
    scheduler_init();
    (void)config_load(&g_cfg);

    led_state_machine_set(LED_BLINK, LED_GREEN, 4);

    int last_y  = -1;
    int last_mo = -1;
    int last_d  = -1;

    uint16_t last_minute = 0xFFFF;
    uint32_t last_etag   = 0;

    struct solar_times sol;
    bool have_sol = false;

    bool in_config_mode = false;

    uint8_t  door_debounce_active   = 0;
    uint32_t door_debounce_start_ms = 0;

    int cached_y = 0, cached_mo = 0, cached_d = 0;
    int cached_h = 0, cached_m = 0, cached_s = 0;

    for (;;) {

        uint32_t now_ms = uptime_millis();
        device_tick(now_ms);

        /* ------------------------------------------------------
         * CONFIG switch
         * ------------------------------------------------------ */

        bool raw = config_sw_state();

        if (raw != in_config_mode) {

            _delay_ms(75);

            if (config_sw_state() == raw) {

                in_config_mode = raw;

                if (in_config_mode) {
                    console_init();
                    reset_cause_debug_print();
                } else {
                    mini_printf("Exiting console\n\n");
                    console_flush();
                    console_terminal_shutdown();
                }
            }
        }

        if (in_config_mode) {
            console_poll();
        }

        /* ------------------------------------------------------
         * Door ISR latch
         * ------------------------------------------------------ */

        if (g_door_event && !door_debounce_active) {
            g_door_event = 0u;
            door_debounce_active = 1u;
            door_debounce_start_ms = now_ms;
        }

        if (door_debounce_active) {
            if ((uint32_t)(now_ms - door_debounce_start_ms) >= 20u) {
                door_debounce_active = 0u;
                if (gpio_door_sw_is_asserted()) {
                    door_sm_toggle();
                }
            }
        }

        if (!gpio_door_sw_is_asserted() && !door_debounce_active) {
            EIFR  |= (uint8_t)(1u << INTF1);
            EIMSK |= (uint8_t)(1u << INT1);
        }

        /* ------------------------------------------------------
         * RTC required
         * ------------------------------------------------------ */

        if (rtc_time_is_set()) {
            if (!rtc_valid) rtc_valid = true;
        }
        else
        {
            led_state_machine_set(LED_BLINK, LED_RED);
            continue;
        }

        /* ------------------------------------------------------
         * Always refresh RTC (UTC authoritative)
         * ------------------------------------------------------ */

        rtc_get_time(&cached_y,
                     &cached_mo,
                     &cached_d,
                     &cached_h,
                     &cached_m,
                     &cached_s);

        uint16_t now_minute = minute_of_day(cached_h, cached_m);
        uint32_t cur_etag   = schedule_etag();

        bool minute_changed = (now_minute != last_minute);
        bool schedule_dirty = (cur_etag != last_etag);

        /* ------------------------------------------------------
         * On minute change or schedule change, run scheduler
         * ------------------------------------------------------ */

        if (minute_changed || schedule_dirty) {

            last_minute = now_minute;
            last_etag   = cur_etag;

            /* ---- Solar recompute if date changed ---- */

            if (cached_y != last_y ||
                cached_mo != last_mo ||
                cached_d != last_d) {

                have_sol = false;

                if (g_cfg.latitude_e4 != 0 ||
                    g_cfg.longitude_e4 != 0) {

                    double lat = (double)g_cfg.latitude_e4  / 10000.0;
                    double lon = (double)g_cfg.longitude_e4 / 10000.0;

                    /*
                     * Scheduling must be DST-invariant.
                     *
                     * Request solar times in UTC by forcing tz = 0.
                     * (Any TZ/DST adjustments belong in console/UI only.)
                     */
                    have_sol = solar_compute(
                        cached_y, cached_mo, cached_d,
                        lat, lon,
                        0,
                        &sol
                    );
                }

                scheduler_update_day(
                    cached_y, cached_mo, cached_d,
                    have_sol ? &sol : NULL,
                    have_sol
                );

                last_y  = cached_y;
                last_mo = cached_mo;
                last_d  = cached_d;
            }

            /* ---- Apply schedule ---- */

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
         * Sleep only in RUN mode
         * ------------------------------------------------------ */

        if (in_config_mode)
            continue;

        if (devices_busy() ||
            door_debounce_active ||
            g_door_event)
            continue;

        uint16_t next_min;
        uint16_t wake_min;

        if (scheduler_next_event_minute(&next_min))
            wake_min = strictly_future_minute(now_minute, next_min);
        else
            wake_min = next_minute(now_minute);

        (void)rtc_alarm_set_minute_of_day(wake_min);
        system_sleep_until(wake_min);

        if (gpio_rtc_int_is_asserted())
            rtc_alarm_clear_flag();

        EIFR |= (uint8_t)((1u << INTF0) | (1u << INTF1));

        if (!gpio_rtc_int_is_asserted())
            EIMSK |= (uint8_t)(1u << INT0);

        if (!gpio_door_sw_is_asserted())
            EIMSK |= (uint8_t)(1u << INT1);
    }
}
