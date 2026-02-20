/**
 * @file rtc.cpp
 * @brief DS3231 Real-Time Clock driver (LOCKED V4.0)
 *
 * ============================================================================
 * SYSTEM DESIGN CONTRACT
 * ============================================================================
 *
 *  - Offline-only system.
 *  - RTC is sole authority for wall-clock time.
 *  - MCU uptime is NEVER used for scheduling.
 *  - All scheduling derives strictly from RTC registers.
 *
 *  - No cloud.
 *  - No network time.
 *  - No fallback clocks.
 *
 *  If the RTC is invalid, the system is invalid.
 *
 * ============================================================================
 * HARDWARE
 * ============================================================================
 *
 *  RTC:        Maxim/Analog Devices DS3231
 *  Interface:  I2C @ 0x68
 *  INT/SQW:    Open-drain, active-low
 *  RTC INT →   PD2 (INT0)
 *  Pull-up:    External to VCC
 *
 * ============================================================================
 */

#include <stdint.h>
#include <stdbool.h>

#include "rtc.h"
#include "i2c.h"

/* ============================================================================
 * REGISTER MAP
 * ========================================================================== */

#define DS3231_ADDR7        0x68

#define REG_SECONDS         0x00
#define REG_MINUTES         0x01
#define REG_HOURS           0x02
#define REG_DAY             0x03
#define REG_DATE            0x04
#define REG_MONTH           0x05
#define REG_YEAR            0x06

#define REG_ALARM1_SEC      0x07
#define REG_ALARM1_MIN      0x08
#define REG_ALARM1_HOUR     0x09
#define REG_ALARM1_DAY      0x0A

#define REG_CONTROL         0x0E
#define REG_STATUS          0x0F

/* CONTROL bits */
#define CTRL_A1IE           (1u << 0)
#define CTRL_INTCN          (1u << 2)

/* STATUS bits */
#define STAT_A1F            (1u << 0)
#define STAT_OSF            (1u << 7)

/* ============================================================================
 * BCD HELPERS
 * ========================================================================== */

static uint8_t bcd_to_bin(uint8_t v)
{
    return (uint8_t)(((v >> 4) * 10u) + (v & 0x0Fu));
}

static uint8_t bin_to_bcd(uint8_t v)
{
    return (uint8_t)(((v / 10u) << 4) | (v % 10u));
}

/* ============================================================================
 * INITIALIZATION
 * ========================================================================== */

void rtc_init(void)
{
    uint8_t control;

    if (!i2c_read(DS3231_ADDR7, REG_CONTROL, &control, 1))
        return;

    /* Ensure interrupt mode (not square wave) */
    control |= CTRL_INTCN;

    (void)i2c_write(DS3231_ADDR7, REG_CONTROL, &control, 1);

    /* Clear any stale alarm flag */
    rtc_alarm_clear_flag();
}

/* ============================================================================
 * STATUS
 * ========================================================================== */

bool rtc_oscillator_running(void)
{
    uint8_t status;
    if (!i2c_read(DS3231_ADDR7, REG_STATUS, &status, 1))
        return false;

    return (status & STAT_OSF) == 0u;
}

bool rtc_time_is_set(void)
{
    uint8_t status;
    if (!i2c_read(DS3231_ADDR7, REG_STATUS, &status, 1))
        return false;

    return (status & STAT_OSF) == 0u;
}

bool rtc_validate_at_boot(void)
{
    uint8_t status;
    if (!i2c_read(DS3231_ADDR7, REG_STATUS, &status, 1))
        return false;

    if (status & STAT_OSF)
        return false;

    return true;
}

/* ============================================================================
 * TIME ACCESS
 * ========================================================================== */

void rtc_get_time(int *y, int *mo, int *d,
                  int *h, int *m, int *s)
{
    uint8_t buf[7];

    if (!i2c_read(DS3231_ADDR7, REG_SECONDS, buf, sizeof(buf)))
        return;

    if (s)  *s  = bcd_to_bin(buf[0] & 0x7F);
    if (m)  *m  = bcd_to_bin(buf[1] & 0x7F);
    if (h)  *h  = bcd_to_bin(buf[2] & 0x3F);
    if (d)  *d  = bcd_to_bin(buf[4] & 0x3F);
    if (mo) *mo = bcd_to_bin(buf[5] & 0x1F);
    if (y)  *y  = 2000 + bcd_to_bin(buf[6]);
}

bool rtc_set_time(int y, int mo, int d,
                  int h, int m, int s)
{
    uint8_t buf[7];

    buf[0] = bin_to_bcd((uint8_t)s);
    buf[1] = bin_to_bcd((uint8_t)m);
    buf[2] = bin_to_bcd((uint8_t)h);
    buf[3] = 0;
    buf[4] = bin_to_bcd((uint8_t)d);
    buf[5] = bin_to_bcd((uint8_t)mo);
    buf[6] = bin_to_bcd((uint8_t)(y % 100));

    return i2c_write(DS3231_ADDR7, REG_SECONDS, buf, sizeof(buf));
}

/* ============================================================================
 * ALARM (Alarm 1 used)
 * ========================================================================== */

bool rtc_alarm_set_hm(uint8_t hour, uint8_t minute)
{
    if (hour > 23u || minute > 59u)
        return false;

    uint8_t control, status;

    /* Disable alarm */
    if (!i2c_read(DS3231_ADDR7, REG_CONTROL, &control, 1))
        return false;

    control &= ~CTRL_A1IE;
    if (!i2c_write(DS3231_ADDR7, REG_CONTROL, &control, 1))
        return false;

    /* Clear alarm flag */
    if (!i2c_read(DS3231_ADDR7, REG_STATUS, &status, 1))
        return false;

    status &= ~STAT_A1F;
    if (!i2c_write(DS3231_ADDR7, REG_STATUS, &status, 1))
        return false;

    /* Program Alarm1: match seconds=0, minute, hour */
    uint8_t a[4];

    a[0] = bin_to_bcd(0);          /* seconds match 0 */
    a[1] = bin_to_bcd(minute);
    a[2] = bin_to_bcd(hour);
    a[3] = 0x80;                  /* disable day/date match */

    if (!i2c_write(DS3231_ADDR7, REG_ALARM1_SEC, a, sizeof(a)))
        return false;

    /* Enable interrupt mode + alarm */
    if (!i2c_read(DS3231_ADDR7, REG_CONTROL, &control, 1))
        return false;

    control |= CTRL_INTCN;
    control |= CTRL_A1IE;

    return i2c_write(DS3231_ADDR7, REG_CONTROL, &control, 1);
}

void rtc_alarm_disable(void)
{
    uint8_t control;
    if (!i2c_read(DS3231_ADDR7, REG_CONTROL, &control, 1))
        return;

    control &= ~CTRL_A1IE;
    (void)i2c_write(DS3231_ADDR7, REG_CONTROL, &control, 1);
}

void rtc_alarm_clear_flag(void)
{
    uint8_t status;
    if (!i2c_read(DS3231_ADDR7, REG_STATUS, &status, 1))
        return;

    status &= ~STAT_A1F;
    (void)i2c_write(DS3231_ADDR7, REG_STATUS, &status, 1);
}
