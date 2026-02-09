/**
 * @file rtc.cpp
 * @brief PCF8523T Real-Time Clock driver.
 *
 * @details
 * Deterministic, offline RTC implementation for the Chicken Coop Controller.
 *
 * DESIGN PRINCIPLES
 * -----------------
 *  - Offline-only system.
 *  - RTC is the sole authority for wall-clock time.
 *  - MCU uptime is NEVER used for scheduling decisions.
 *  - All scheduling derives from RTC registers.
 *
 * HARDWARE (LOCKED V3.0)
 * ----------------------
 *  - RTC: NXP PCF8523T (8-pin)
 *  - Crystal: AB26T 32.768 kHz, 12.5 pF load
 *  - INT/CLKOUT pin: Pin 7 (shared function)
 *  - RTC INT wired to PB7 (PCINT7)
 *  - External pull-up (~10 kΩ) on INT line
 *
 * CRITICAL SILICON BEHAVIOR
 * -------------------------
 * 1. CLKOUT DEFAULT
 *    Register 0x0F defaults to COF=000 which outputs 32.768 kHz.
 *    If not disabled, pin 7 will output a continuous clock.
 *    This WILL break low-level interrupt wake logic.
 *
 *    Proper disable:
 *        COF[2:0] = 111 (bits 5..3)
 *
 * 2. INT IS OPEN-DRAIN
 *    - Active-low
 *    - Latched low when AF=1 and AIE=1
 *    - Remains low until AF cleared
 *
 * 3. OSCILLATOR STOP FLAG (OS)
 *    - Seconds[7] indicates oscillator stop.
 *    - Must be cleared when time is written.
 *
 * 4. CRYSTAL LOAD CAPACITANCE
 *    - Control_1[4:3]
 *    - MUST be configured for AB26T crystal.
 *
 * @note This module assumes I2C has already been initialized.
 *
 * @author Vinnie
 * @date 2026-02-07
 */

#include "rtc.h"
#include "i2c.h"

#include <stdint.h>
#include <stdbool.h>

#include "console/mini_printf.h"

/* ============================================================================
 * PCF8523 REGISTER MAP
 * ========================================================================== */

/** 7-bit I2C address of PCF8523 */
#define PCF8523_ADDR7        0x68

/* Control registers */
#define REG_CONTROL_1        0x00
#define REG_CONTROL_2        0x01

/* Time registers */
#define REG_SECONDS          0x03
#define REG_MINUTES          0x04
#define REG_HOURS            0x05
#define REG_DAYS             0x06
#define REG_MONTHS           0x08
#define REG_YEARS            0x09

/* Alarm registers */
#define REG_ALARM_MINUTE     0x0A
#define REG_ALARM_HOUR       0x0B
#define REG_ALARM_DAY        0x0C
#define REG_ALARM_WEEKDAY    0x0D

/* Timer / CLKOUT */
#define REG_TMR_CLKOUT       0x0F

/* ============================================================================
 * CONTROL BITS
 * ========================================================================== */

/** STOP bit: 1 = oscillator stopped */
#define CTRL1_STOP_BIT       (1 << 5)

#define CTRL1_AIE_BIT        (1 << 1)



/** Alarm Interrupt Enable */
//#define CTRL2_AIE_BIT        (1 << 1)

/** Alarm Flag (latched when alarm match occurs) */
#define CTRL2_AF_BIT         (1 << 3)

/** Alarm register disable bit (AENx) */
#define ALARM_DISABLE        (1 << 7)

/* ============================================================================
 * CRYSTAL LOAD CAPACITANCE
 * ========================================================================== */

/**
 * Control_1[4:3] selects crystal load capacitance.
 *
 * AB26T nominal load = 12.5 pF
 */
#define CTRL1_CL_MASK        (3u << 3)
#define CTRL1_CL_12P5PF      (1u << 3)

/* ============================================================================
 * CLKOUT CONTROL
 * ========================================================================== */

/**
 * COF[2:0] bits are located in REG_TMR_CLKOUT[5:3].
 *
 * COF values:
 *   000 -> 32.768 kHz (default)
 *   011 -> 4.096 kHz
 *   110 -> 1 Hz
 *   111 -> Disabled (high-Z)
 */
#define CLKOUT_COF_MASK      (0x07u << 3)
#define CLKOUT_DISABLE       (0x07u << 3)

/* ============================================================================
 * RTC INITIALIZATION
 * ========================================================================== */

/**
 * @brief Initialize RTC hardware configuration.
 *
 * Performs:
 *  - Configure crystal load capacitance
 *  - Ensure oscillator running
 *  - Disable CLKOUT (mandatory)
 *  - Clear stale alarm flags
 */
void rtc_init(void)
{
    uint8_t c1;

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
        return;

    /* Configure crystal load capacitance */
    c1 &= (uint8_t)~CTRL1_CL_MASK;
    c1 |= CTRL1_CL_12P5PF;

    /* Ensure oscillator running */
    c1 &= (uint8_t)~CTRL1_STOP_BIT;

    (void)i2c_write(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1);

    /* Disable CLKOUT so pin 7 acts only as INT */
    uint8_t clk;
    if (i2c_read(PCF8523_ADDR7, REG_TMR_CLKOUT, &clk, 1)) {
        clk &= (uint8_t)~CLKOUT_COF_MASK;
        clk |= (uint8_t)CLKOUT_DISABLE;
        (void)i2c_write(PCF8523_ADDR7, REG_TMR_CLKOUT, &clk, 1);
    }

    rtc_alarm_clear_flag();
}

/* ============================================================================
 * INTERNAL HELPERS
 * ========================================================================== */

/**
 * @brief Convert BCD to binary.
 */
static uint8_t bcd_to_bin(uint8_t v)
{
    return (uint8_t)(((v >> 4) * 10u) + (v & 0x0Fu));
}

/**
 * @brief Convert binary to BCD.
 */
static uint8_t bin_to_bcd(uint8_t v)
{
    return (uint8_t)(((v / 10u) << 4) | (v % 10u));
}

/* ============================================================================
 * STATUS FUNCTIONS
 * ========================================================================== */

/**
 * @brief Check if RTC oscillator is running.
 * @return true if oscillator active.
 */
bool rtc_oscillator_running(void)
{
    uint8_t c1;

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
        return false;

    return (c1 & CTRL1_STOP_BIT) == 0;
}

/**
 * @brief Check if stored time is valid.
 *
 * Seconds[7] (OS flag) indicates oscillator stop event.
 *
 * @return true if time considered valid.
 */
bool rtc_time_is_set(void)
{
    uint8_t sec;

    if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, &sec, 1))
        return false;

    return (sec & (1 << 7)) == 0;
}

/* ============================================================================
 * TIME API
 * ========================================================================== */

/**
 * @brief Read current time from RTC.
 */
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

/**
 * @brief Set RTC time using burst write.
 *
 * @return true on success.
 */

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

/* ============================================================================
 * ALARM API
 * ========================================================================== */

/**
 * @brief Set alarm match for hour/minute.
 */


 bool rtc_alarm_set_hm(uint8_t hour, uint8_t minute)
 {
     if (hour > 23u || minute > 59u)
         return false;

     /* ------------------------------------------------------------------
      * Guard against re-arming inside the match window.
      * PCF8523 compares hour+minute only (seconds ignored).
      * If we arm during the matching minute (sec > 0),
      * AF will immediately assert and INT will stay low.
      * ------------------------------------------------------------------ */
     {
         uint8_t buf[3];

         if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, buf, 3))
             return false;

         uint8_t now_s = bcd_to_bin(buf[0] & 0x7F);
         uint8_t now_m = bcd_to_bin(buf[1] & 0x7F);
         uint8_t now_h = bcd_to_bin(buf[2] & 0x3F);

         if ((now_h == hour) && (now_m == minute) && (now_s != 0u)) {
             /* Inside matching minute window — refuse */
             return false;
         }
     }

     uint8_t c1, c2;

     /* 1) Disable AIE */
     if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
         return false;
     c1 &= (uint8_t)~CTRL1_AIE_BIT;
     if (!i2c_write(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
         return false;

     /* 2) Clear AF */
     if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1))
         return false;
     c2 &= (uint8_t)~CTRL2_AF_BIT;
     if (!i2c_write(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1))
         return false;

     /* 3) Program alarm registers */
     uint8_t a[4];
     a[0] = (uint8_t)(bin_to_bcd(minute) & 0x7Fu);
     a[1] = (uint8_t)(bin_to_bcd(hour)   & 0x3Fu);
     a[2] = (uint8_t)ALARM_DISABLE;
     a[3] = (uint8_t)ALARM_DISABLE;

     if (!i2c_write(PCF8523_ADDR7, REG_ALARM_MINUTE, a, sizeof(a)))
         return false;

     /* 4) Clear AF again (defensive) */
     if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1))
         return false;
     c2 &= (uint8_t)~CTRL2_AF_BIT;
     if (!i2c_write(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1))
         return false;

     /* 5) Enable AIE */
     if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
         return false;
     c1 |= (uint8_t)CTRL1_AIE_BIT;
     if (!i2c_write(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
         return false;

     return true;
 }


 void rtc_alarm_disable(void)
 {
     uint8_t c1;

     if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
         return;

     c1 &= (uint8_t)~CTRL1_AIE_BIT;
     (void)i2c_write(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1);
 }

 void rtc_alarm_clear_flag(void)
 {
     uint8_t c2;

     if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1))
         return;

     c2 &= (uint8_t)~CTRL2_AF_BIT;
     (void)i2c_write(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1);
 }


void rtc_debug_dump(void)
{
    uint8_t c1, c2;

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1) ||
        !i2c_read(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1)) {
        mini_printf("RTC: read failed\n");
        return;
    }

    mini_printf("RTC CTRL1=0x%02x CTRL2=0x%02x AIE=%u AF=%u\n",
                c1,
                c2,
                (c1 & CTRL1_AIE_BIT) ? 1u : 0u,
                (c2 & CTRL2_AF_BIT) ? 1u : 0u);
}
