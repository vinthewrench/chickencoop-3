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
 * Updated: 2026-02-08
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

#include "time_dst.h"

#include "scheduler.h"
#include "state_reducer.h"
#include "schedule_apply.h"

#include "devices/devices.h"
#include "devices/led_state_machine.h"

#include "platform/gpio_avr.h"
#include "platform/i2c.h"


/* ============================================================================
 * INT0 WAKE (PD2)
 * ========================================================================== */
 /*
  * INT0 ISR
  *
  * One-shot mask so we don't re-enter while
  * RTC INT line is still held low (AF not cleared yet).
  */
 ISR(INT0_vect)
 {
     EIMSK &= (uint8_t)~(1u << INT0);
 }


 ISR(INT1_vect)
 {
     EIMSK &= (uint8_t)~(1u << INT1);
 }


// static void rtc_wake_init_pd2(void)
// {
//     /* PD2 input */
//     DDRD  &= (uint8_t)~(1u << PD2);

//     /* External pull-up exists. Leave internal OFF. */
//     PORTD &= (uint8_t)~(1u << PD2);

//     /* LOW-level trigger required for PWR_DOWN wake */
//     EICRA &= (uint8_t)~((1u << ISC01) | (1u << ISC00));

//     /* Clear any stale flag */
//     EIFR  |= (uint8_t)(1u << INTF0);

//     /* Enable INT0 */
//     EIMSK |= (uint8_t)(1u << INT0);
// }

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



static uint8_t g_reset_flags;

static void reset_cause_capture_early(void)
{
    g_reset_flags = MCUSR;
    MCUSR = 0;          /* clear flags immediately */
    wdt_disable();      /* if WDT ever enabled, keep boot deterministic */

    /* Disable JTAG immediately (double write required) */
    MCUCR |= _BV(JTD);
    MCUCR |= _BV(JTD);

}

static void reset_cause_debug_print(void)
{
    /* Optional, but useful while you are validating BOD behavior */
     if (g_reset_flags & _BV(PORF))  mini_printf("RESET: Power On\n");
//    if (g_reset_flags & _BV(EXTRF)) mini_printf(" EXT");
    if (g_reset_flags & _BV(BORF))  mini_printf("RESET: Brown-Out\n");
    if (g_reset_flags & _BV(WDRF))  mini_printf("RESET: Watchdog\n");
 }


/* ============================================================================
 * MAIN
 * ========================================================================== */

int main(void)
{
    reset_cause_capture_early();

     /* If brown-out reset, allow rails + peripherals to settle */
     if (g_reset_flags & _BV(BORF)) {
         _delay_ms(50);
     }


    uart_init();

    if (!i2c_init(100000)) {
        mini_printf("I2C init failed\n");
        while (1);
    }

    uptime_init();
    rtc_init();

    coop_gpio_init();

    /* INT0 wake path (PD2) */
    system_sleep_init();

    sei();

    device_init();
    scheduler_init();

    (void)config_load(&g_cfg);

    /* ----------------------------------------------------------
     * CONFIG MODE
     * ---------------------------------------------------------- */
    static bool config_consumed = false;

    if (config_sw_state() && !config_consumed) {

        config_consumed = true;

        console_init();

        reset_cause_debug_print();   /* <-- add this here */

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

    /* Healthy boot pulse */
    led_state_machine_set(LED_ON, LED_GREEN);
    {
        uint32_t t0 = uptime_millis();
        while ((uint32_t)(uptime_millis() - t0) < 1000u)
            device_tick(uptime_millis());
    }
    led_state_machine_set(LED_OFF, LED_GREEN);

    /* ----------------------------------------------------------
     * Scheduler state
     * ---------------------------------------------------------- */
    uint16_t last_minute = 0xFFFF;
    uint32_t last_etag   = 0;

    int last_y  = -1;
    int last_mo = -1;
    int last_d  = -1;

    struct solar_times sol;
    bool have_sol = false;

    /* ----------------------------------------------------------
     * MAIN LOOP
     * ---------------------------------------------------------- */
    for (;;) {

        /* Always clear AF first.
           This releases INT line high again. */
        rtc_alarm_clear_flag();

        /* Re-arm INT0 after AF cleared */
        EIFR  |= (uint8_t)(1u << INTF0);
        EIMSK |= (uint8_t)(1u << INT0);

        /* Re-arm INT1 only when door switch is released */
        EIFR |= (uint8_t)(1u << INTF1);
        if (!gpio_door_sw_is_asserted()) {
            EIMSK |= (uint8_t)(1u << INT1);
        }

        /* Read time */
        int y, mo, d, h, m, s;
        rtc_get_time(&y, &mo, &d, &h, &m, &s);

        uint16_t now_minute = minute_of_day(h, m);
        uint32_t cur_etag   = schedule_etag();

        bool minute_changed = (now_minute != last_minute);
        bool schedule_dirty = (cur_etag != last_etag);

        /* ------------------------------------------------------
         * Sleep if nothing changed
         * ------------------------------------------------------ */
        if (!minute_changed && !schedule_dirty) {

            uint16_t next_min;
            uint16_t wake_min;

            if (scheduler_next_event_minute(&next_min))
                wake_min = strictly_future_minute(now_minute, next_min);
            else
                wake_min = next_minute(now_minute);

            (void)rtc_alarm_set_minute_of_day(wake_min);

            system_sleep_until(wake_min);

            continue;
        }

        last_minute = now_minute;
        last_etag   = cur_etag;

        /* ------------------------------------------------------
         * Solar recompute per day
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
         * Apply schedule
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
        {
            uint16_t next_min;
            uint16_t wake_min;

            if (scheduler_next_event_minute(&next_min))
                wake_min = strictly_future_minute(now_minute, next_min);
            else
                wake_min = next_minute(now_minute);

            (void)rtc_alarm_set_minute_of_day(wake_min);

            system_sleep_until(wake_min);
        }
    }
}
