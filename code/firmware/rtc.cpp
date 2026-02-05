/*
 * rtc.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: RTC implementation for PCF8523T
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No DST or timezone logic (handled elsewhere)
 *  - Alarm interrupt used solely for wake from sleep
 *
 * Hardware assumptions (LOCKED V3.0/V3.2):
 *  - RTC: NXP PCF8523T
 *  - Crystal: AB26T 32.768 kHz (12.5 pF load)
 *  - INT output is open-drain, active-low, latched until AF is cleared
 *  - RTC INT is wired to PB7 (PCINT7)
 *  - External pull-up (~10 kΩ) present on RTC INT line
 *
 * PCF8523-specific rules:
 *  - Time registers MUST be written with oscillator stopped (CTRL1.STOP=1)
 *  - Oscillator MUST be restarted after setting time
 *  - Seconds[7] is OS (Oscillator Stop) flag and must be cleared when setting time
 *  - Crystal load capacitance (CTRL1[4:3]) must be configured in software
 *
 * Updated: 2026-02-04
 */

#include "rtc.h"
#include "platform/i2c.h"

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
#define CTRL1_STOP_BIT       (1u << 5)  /* 1 = oscillator stopped */
#define CTRL2_AIE_BIT        (1u << 1)
#define CTRL2_AF_BIT         (1u << 3)

/* Alarm disable flag */
#define ALARM_DISABLE        (1u << 7)

/* Crystal load capacitance: Control_1[4:3]
 * AB26T nominal load = 12.5 pF.
 */
#define CTRL1_CL_MASK        (3u << 3)
#define CTRL1_CL_12P5PF      (1u << 3)

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
 * Bring-up
 * -------------------------------------------------------------------------- */

void rtc_init(void)
{
    uint8_t c1;

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1)) {
        return;
    }

    /* Configure crystal load capacitance for AB26T (12.5 pF). */
    c1 &= (uint8_t)~CTRL1_CL_MASK;
    c1 |= CTRL1_CL_12P5PF;

    /* Ensure oscillator is running. */
    c1 &= (uint8_t)~CTRL1_STOP_BIT;

    (void)i2c_write(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1);

    /* Release INT line if a stale alarm flag is set. */
    rtc_alarm_clear_flag();
}

/* --------------------------------------------------------------------------
 * Oscillator / validity checks
 * -------------------------------------------------------------------------- */

/*
 * Returns true if the RTC crystal oscillator is running.
 * This directly reflects CTRL1.STOP.
 */
bool rtc_oscillator_running(void)
{
    uint8_t c1;

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1)) {
        return false;
    }

    return (c1 & CTRL1_STOP_BIT) == 0;
}

/*
 * Returns true if stored time is considered valid.
 * PCF8523 Seconds[7] is OS (Oscillator Stop) flag:
 *  - OS=1 indicates time may be invalid due to oscillator stop/power loss.
 *  - OS=0 indicates time is valid.
 */
bool rtc_time_is_set(void)
{
    uint8_t sec;

    if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, &sec, 1)) {
        return false;
    }

    return (sec & (1u << 7)) == 0;
}

/* --------------------------------------------------------------------------
 * Time API
 * -------------------------------------------------------------------------- */

void rtc_get_time(int *y, int *mo, int *d,
                  int *h, int *m, int *s)
{
    uint8_t buf[7];

    if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, buf, (uint8_t)sizeof(buf))) {
        return;
    }

    if (s)  *s  = bcd_to_bin(buf[0] & 0x7F);
    if (m)  *m  = bcd_to_bin(buf[1] & 0x7F);
    if (h)  *h  = bcd_to_bin(buf[2] & 0x3F);
    if (d)  *d  = bcd_to_bin(buf[3] & 0x3F);
    if (mo) *mo = bcd_to_bin(buf[5] & 0x1F);
    if (y)  *y  = 2000 + bcd_to_bin(buf[6]);
}


bool rtc_set_time(int y, int mo, int d,
                  int h, int m, int s)
{
    uint8_t c1;
    uint8_t buf[7];
    uint8_t sec1, sec2;

    /* 1. Read CTRL1 and assert STOP (preserve CL bits) */
    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
        return false;

    c1 |= CTRL1_STOP_BIT;
    if (!i2c_write(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
        return false;

    /* 2. Confirm STOP latched */
    uint8_t attempts = 20;
    do {
        if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
            return false;
        if (c1 & CTRL1_STOP_BIT)
            break;
    } while (--attempts);

    if (attempts == 0)
        return false;

    /* 3. Verify seconds frozen */
    if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, &sec1, 1))
        return false;
    if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, &sec2, 1))
        return false;
    if (sec1 != sec2)
        return false;

    /* 4. Write time (OS flag cleared) */
    buf[0] = bin_to_bcd((uint8_t)s) & 0x7F;
    buf[1] = bin_to_bcd((uint8_t)m) & 0x7F;
    buf[2] = bin_to_bcd((uint8_t)h) & 0x3F;
    buf[3] = bin_to_bcd((uint8_t)d) & 0x3F;
    buf[4] = 0;
    buf[5] = bin_to_bcd((uint8_t)mo) & 0x1F;
    buf[6] = bin_to_bcd((uint8_t)(y % 100));

    if (!i2c_write(PCF8523_ADDR7, REG_SECONDS, buf, sizeof(buf)))
        return false;

    /* 5. Restart oscillator */
    c1 &= (uint8_t)~CTRL1_STOP_BIT;
    if (!i2c_write(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
        return false;

    /* 6. Confirm restart */
    attempts = 20;
    do {
        if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
            return false;
        if ((c1 & CTRL1_STOP_BIT) == 0)
            break;
    } while (--attempts);

    return true;
}
/* --------------------------------------------------------------------------
 * Alarm API
 * -------------------------------------------------------------------------- */

bool rtc_alarm_set_hm(uint8_t hour, uint8_t minute)
{
    uint8_t a[4];

    /* Minute/hour match only; day and weekday disabled. */
    a[0] = (uint8_t)(bin_to_bcd(minute) & 0x7F);
    a[1] = (uint8_t)(bin_to_bcd(hour)   & 0x3F);
    a[2] = ALARM_DISABLE;
    a[3] = ALARM_DISABLE;

    if (!i2c_write(PCF8523_ADDR7, REG_ALARM_MINUTE, a, (uint8_t)sizeof(a))) {
        return false;
    }

    uint8_t c2;
    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1)) {
        return false;
    }

    /*
     * Enable alarm interrupt and clear any stale alarm flag.
     * INT asserts low when the alarm matches and remains low
     * until AF is cleared via rtc_alarm_clear_flag().
     */
    c2 |= CTRL2_AIE_BIT;
    c2 &= (uint8_t)~CTRL2_AF_BIT;

    return i2c_write(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1);
}

void rtc_alarm_disable(void)
{
    uint8_t c2;

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1)) {
        return;
    }

    c2 &= (uint8_t)~CTRL2_AIE_BIT;
    (void)i2c_write(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1);
}

void rtc_alarm_clear_flag(void)
{
    uint8_t c2;

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1)) {
        return;
    }

    /* Clearing AF releases the INT line (open-drain). */
    c2 &= (uint8_t)~CTRL2_AF_BIT;
    (void)i2c_write(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1);
}
