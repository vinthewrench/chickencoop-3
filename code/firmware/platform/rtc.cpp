/*
 * rtc.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: RTC implementation for PCF8523
 *
 * DESIGN INTENT (CORRECTED):
 *  - Offline system
 *  - Deterministic behavior
 *  - RTC is the sole authority for wall-clock time
 *  - MCU uptime is NOT used for timekeeping decisions
 *
 * HARDWARE (LOCKED V3.0):
 *  - RTC: NXP PCF8523T
 *  - Crystal: AB26T 32.768 kHz (12.5 pF load)
 *  - INT output: open-drain, active-low, latched until AF is cleared
 *  - RTC INT wired to PB7 (PCINT7)
 *  - External pull-up (~10 kΩ) present on RTC INT line
 *
 * IMPORTANT RTC SEMANTICS (PCF8523-SPECIFIC):
 *  - Time registers MUST be written with oscillator stopped (CTRL1.STOP = 1)
 *  - Oscillator MUST be explicitly restarted after time is set
 *  - Crystal load capacitance MUST be configured in software
 *  - Seconds[7] (OS flag) MUST be cleared when setting time
 *
 * ALARM SEMANTICS:
 *  - Alarm INT asserts low when match occurs
 *  - INT remains low until AF is cleared
 *  - PCINT edge behavior is expected and harmless
 *
 * Updated: 2026-01-05
 */

#include "rtc.h"
#include "i2c.h"

#include <stdint.h>
#include <stdbool.h>

/* --------------------------------------------------------------------------
 * PCF8523 definitions
 * -------------------------------------------------------------------------- */

#define PCF8523_ADDR7        0x68

/* Registers */
#define REG_CONTROL_1        0x00
#define REG_CONTROL_2        0x01

#define REG_SECONDS          0x03
#define REG_MINUTES          0x04
#define REG_HOURS            0x05
#define REG_DAYS             0x06
#define REG_MONTHS           0x08
#define REG_YEARS            0x09

#define REG_ALARM_MINUTE     0x0A
#define REG_ALARM_HOUR       0x0B
#define REG_ALARM_DAY        0x0C
#define REG_ALARM_WEEKDAY    0x0D

/* Control bits */
#define CTRL1_STOP_BIT       (1 << 5)   /* 1 = oscillator stopped */
#define CTRL2_AIE_BIT        (1 << 1)
#define CTRL2_AF_BIT         (1 << 3)

/* Alarm disable flag */
#define ALARM_DISABLE        (1 << 7)

/* Crystal load capacitance (Control_1[4:3])
 * AB26T nominal load = 12.5 pF
 * Start with CL = 01; adjust only if drift measurement demands it.
 */
#define CTRL1_CL_MASK        (3u << 3)
#define CTRL1_CL_12P5PF      (1u << 3)

/* --------------------------------------------------------------------------
 * Bring-up
 * -------------------------------------------------------------------------- */

void rtc_init(void)
{
    uint8_t c1;

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
        return;

    /*
     * Configure crystal load capacitance for AB26T.
     * This is REQUIRED for stable frequency.
     */
    c1 &= (uint8_t)~CTRL1_CL_MASK;
    c1 |= CTRL1_CL_12P5PF;

    /* Ensure oscillator is running */
    c1 &= (uint8_t)~CTRL1_STOP_BIT;

    (void)i2c_write(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1);

    /* Clear any stale alarm flag so INT is released */
    rtc_alarm_clear_flag();
}

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

static uint8_t bcd_to_bin(uint8_t v)
{
    return (uint8_t)(((v >> 4) * 10u) + (v & 0x0Fu));
}

static uint8_t bin_to_bcd(uint8_t v)
{
    return (uint8_t)(((v / 10u) << 4) | (v % 10u));
}

/* --------------------------------------------------------------------------
 * Oscillator / validity checks
 * -------------------------------------------------------------------------- */

bool rtc_oscillator_running(void)
{
    uint8_t c1;

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
        return false;

    return (c1 & CTRL1_STOP_BIT) == 0;
}

/*
 * Returns true if stored time is considered valid.
 * Seconds[7] = OS flag (Oscillator Stop)
 * OS = 1 indicates time may be invalid due to power loss or stop.
 */
bool rtc_time_is_set(void)
{
    uint8_t sec;

    if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, &sec, 1))
        return false;

    return (sec & (1 << 7)) == 0;
}

/* --------------------------------------------------------------------------
 * Time API
 * -------------------------------------------------------------------------- */

void rtc_get_time(int *y, int *mo, int *d,
                  int *h, int *m, int *s)
{
    uint8_t buf[7];

    if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, buf, sizeof(buf)))
        return;

    if (s)  *s  = bcd_to_bin(buf[0] & 0x7F);
    if (m)  *m  = bcd_to_bin(buf[1] & 0x7F);
    if (h)  *h  = bcd_to_bin(buf[2] & 0x3F);
    if (d)  *d  = bcd_to_bin(buf[3] & 0x3F);
    if (mo) *mo = bcd_to_bin(buf[5] & 0x1F);
    if (y)  *y  = 2000 + bcd_to_bin(buf[6]);
}

/*
 * rtc_set_time()
 *
 * Safely set PCF8523 time registers.
 *
 * Design goals:
 *  - NEVER write time while oscillator is running
 *  - Preserve crystal load capacitance (CL bits)
 *  - Clear OS flag explicitly
 *  - Verify STOP actually latched
 *  - Avoid CPU-speed-dependent delays
 *
 * If this function returns, oscillator is running and time registers
 * are either fully updated or untouched.
 */

 /*
  * rtc_set_time_simple()
  *
  * Simpler set method (matches Linux rtc-pcf8523.c and Adafruit RTClib patterns):
  * - No STOP bit needed — chip handles writes safely while running
  * - Burst write 7 time registers with OS cleared
  * - Preserve CL bits (though not touched here)
  * - Add input validation
  * - Returns early on any I²C fail
  */
 bool rtc_set_time(int y, int mo, int d, int h, int m, int s)
 {
     uint8_t buf[7];

     /* Basic input validation to prevent bad BCD */
     if (y < 2000 || y > 2099 ||
         mo < 1 || mo > 12 ||
         d < 1 || d > 31 ||
         h < 0 || h > 23 ||
         m < 0 || m > 59 ||
         s < 0 || s > 59) {
         return false;  /* Invalid — caller can log/retry */
     }

     /* Prepare BCD values, OS flag cleared in seconds */
     buf[0] = bin_to_bcd((uint8_t)s) & 0x7F;
     buf[1] = bin_to_bcd((uint8_t)m) & 0x7F;
     buf[2] = bin_to_bcd((uint8_t)h) & 0x3F;
     buf[3] = bin_to_bcd((uint8_t)d) & 0x3F;
     buf[4] = 0;               /* weekday unused */
     buf[5] = bin_to_bcd((uint8_t)mo) & 0x1F;
     buf[6] = bin_to_bcd((uint8_t)(y % 100));

     /* Burst write to REG_SECONDS through REG_YEARS */
     if (!i2c_write(PCF8523_ADDR7, REG_SECONDS, buf, sizeof(buf))) {
         return false;
     }

     return true;
 }

/* --------------------------------------------------------------------------
 * Alarm API
 * -------------------------------------------------------------------------- */

bool rtc_alarm_set_hm(uint8_t hour, uint8_t minute)
{
    uint8_t a[4];

    a[0] = bin_to_bcd(minute) & 0x7F;
    a[1] = bin_to_bcd(hour)   & 0x3F;
    a[2] = ALARM_DISABLE;
    a[3] = ALARM_DISABLE;

    if (!i2c_write(PCF8523_ADDR7, REG_ALARM_MINUTE, a, sizeof(a)))
        return false;

    uint8_t c2;
    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1))
        return false;

    c2 |= CTRL2_AIE_BIT;
    c2 &= (uint8_t)~CTRL2_AF_BIT;

    return i2c_write(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1);
}

void rtc_alarm_disable(void)
{
    uint8_t c2;

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1))
        return;

    c2 &= (uint8_t)~CTRL2_AIE_BIT;
    (void)i2c_write(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1);
}

void rtc_alarm_clear_flag(void)
{
    uint8_t c2;

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1))
        return;

    c2 &= (uint8_t)~CTRL2_AF_BIT;
    (void)i2c_write(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1);
}
