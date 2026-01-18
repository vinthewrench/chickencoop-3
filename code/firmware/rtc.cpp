/*
 * rtc.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: RTC implementation for PCF8523
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No DST or timezone logic (handled elsewhere)
 *  - Alarm interrupt used solely for wake from sleep
 *
 * Hardware assumptions (LOCKED V3.0):
 *  - RTC: NXP PCF8523
 *  - INT output is open-drain, active-low, latched until AF is cleared
 *  - RTC INT is wired to PB7 (PCINT7)
 *  - External pull-up (~10 kΩ) present on RTC INT line
 *
 * Interrupt semantics:
 *  - Alarm INT remains asserted (low) until AF is cleared
 *  - Wake logic MUST confirm alarm source by reading RTC flags
 *  - PCINT edge behavior (assert + release) is expected and harmless
 *
 * Updated: 2026-01-05
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
#define CTRL1_STOP_BIT       (1 << 5)   /* 1 = oscillator stopped */
#define CTRL2_AIE_BIT        (1 << 1)
#define CTRL2_AF_BIT         (1 << 3)

/* Alarm disable flag */
#define ALARM_DISABLE        (1 << 7)



/**
 * @brief Initialize RTC hardware control state (PCF8523).
 *
 * This function establishes firmware ownership of the RTC by normalizing
 * control registers into a known, safe baseline state.
 *
 * Responsibilities:
 * - Ensures the 32.768 kHz crystal oscillator is running by clearing
 *   CTRL1.STOP if set.
 * - Disables the alarm interrupt (AIE) to prevent unintended wake events.
 * - Clears any stale alarm flag (AF) to release the INT line if latched.
 *
 * Non-responsibilities (intentional):
 * - Does NOT set or validate the current time.
 * - Does NOT clear the VL (Voltage Low) flag.
 * - Does NOT arm alarms or configure scheduling behavior.
 * - Does NOT make any policy decisions.
 *
 * Rationale:
 * The PCF8523 does not guarantee a deterministic control-register state
 * after power transitions or backup-domain operation. In particular,
 * the oscillator may be stopped (CTRL1.STOP = 1), causing timekeeping
 * to halt even when VBAT is present.
 *
 * This function performs mandatory hardware normalization only. Higher-
 * level firmware is responsible for interpreting RTC validity, setting
 * time, and enabling alarms during RUN mode.
 *
 * This function is safe to call multiple times.
 */

 void rtc_init(void)
 {
     uint8_t c1;
     uint8_t c2;

     /*
      * Take ownership of RTC control state.
      *
      * Policy-free:
      *  - Do NOT set time
      *  - Do NOT clear VL
      *  - Do NOT arm alarms
      *
      * Hardware normalization only.
      */

     /* ---- CONTROL_1: ensure oscillator is running ---- */
     if (i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1)) {
         if (c1 & CTRL1_STOP_BIT) {
             c1 &= (uint8_t)~CTRL1_STOP_BIT;
             (void)i2c_write(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1);
         }
     }

     /* ---- CONTROL_2: disable alarm interrupt, clear stale AF ---- */
     if (i2c_read(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1)) {
         c2 &= (uint8_t)~CTRL2_AIE_BIT;  /* alarms off by default */
         c2 &= (uint8_t)~CTRL2_AF_BIT;   /* release INT if latched */
         (void)i2c_write(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1);
     }
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

/*
 * Returns true if the RTC crystal oscillator is running.
 * This directly reflects the CTRL1.STOP bit.
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
 * Returns true if the stored time is considered valid.
 * This checks the VL (Voltage Low) flag in the seconds register.
 *
 * Note:
 *  - VL = 1 means time may be invalid due to power loss
 *  - This does NOT indicate whether the oscillator is currently running
 */
bool rtc_time_is_set(void)
{
    uint8_t sec;

    if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, &sec, 1)) {
        return false;
    }

    /* VL flag is bit 7 of seconds register */
    return (sec & (1 << 7)) == 0;
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

/* --------------------------------------------------------------------------
 * Alarm API
 * -------------------------------------------------------------------------- */

bool rtc_alarm_set_hm(uint8_t hour, uint8_t minute)
{
    uint8_t a[4];

    /* Minute/hour match only; day and weekday disabled */
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

    /*
     * Enable alarm interrupt and clear any stale alarm flag.
     * INT will assert low when the alarm matches and remain low
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

    /* Clearing AF releases the INT line */
    c2 &= (uint8_t)~CTRL2_AF_BIT;
    (void)i2c_write(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1);
}
