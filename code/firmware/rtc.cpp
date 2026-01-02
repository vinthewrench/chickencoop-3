/*
 * rtc.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: RTC implementation for PCF8523
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No DST or timezone logic
 *  - Alarm interrupt used for wake
 *
 * Updated: 2025-12-29
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
#define CTRL1_STOP_BIT       (1 << 5)
#define CTRL2_AIE_BIT        (1 << 1)
#define CTRL2_AF_BIT         (1 << 3)

/* Alarm disable flag */
#define ALARM_DISABLE        (1 << 7)

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
 * Time API
 * -------------------------------------------------------------------------- */

void rtc_get_time(int *y, int *mo, int *d,
                  int *h, int *m, int *s)
{
    uint8_t buf[7];

    if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, buf, sizeof(buf))) {
        return;
    }

    if (s)  *s  = bcd_to_bin(buf[0] & 0x7F);
    if (m)  *m  = bcd_to_bin(buf[1] & 0x7F);
    if (h)  *h  = bcd_to_bin(buf[2] & 0x3F);
    if (d)  *d  = bcd_to_bin(buf[3] & 0x3F);
    if (mo) *mo = bcd_to_bin(buf[5] & 0x1F);
    if (y)  *y  = 2000 + bcd_to_bin(buf[6]);
}

void rtc_set_time(int y, int mo, int d,
                  int h, int m, int s)
{
    uint8_t buf[7];

    buf[0] = bin_to_bcd((uint8_t)s)  & 0x7F;
    buf[1] = bin_to_bcd((uint8_t)m)  & 0x7F;
    buf[2] = bin_to_bcd((uint8_t)h)  & 0x3F;
    buf[3] = bin_to_bcd((uint8_t)d)  & 0x3F;
    buf[4] = 0; /* weekday not used */
    buf[5] = bin_to_bcd((uint8_t)mo) & 0x1F;
    buf[6] = bin_to_bcd((uint8_t)(y % 100));

    (void)i2c_write(PCF8523_ADDR7, REG_SECONDS, buf, sizeof(buf));
}

bool rtc_time_is_set(void)
{
    uint8_t sec;

    if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, &sec, 1)) {
        return false;
    }

    /* OS flag: 1 = oscillator stopped or invalid time */
    return (sec & (1 << 7)) == 0;
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

    if (!i2c_write(PCF8523_ADDR7, REG_ALARM_MINUTE, a, sizeof(a))) {
        return false;
    }

    uint8_t c2;
    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1)) {
        return false;
    }

    /* Enable alarm interrupt and clear stale flag */
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

    c2 &= (uint8_t)~CTRL2_AF_BIT;
    (void)i2c_write(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1);
}
