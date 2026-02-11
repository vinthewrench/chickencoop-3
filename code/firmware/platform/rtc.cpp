/**
 * @file rtc.cpp
 * @brief PCF8523T Real-Time Clock driver (LOCKED V3.0)
 *
 * ============================================================================
 * SYSTEM DESIGN CONTRACT
 * ============================================================================
 *
 *  - Offline-only system.
 *  - RTC is the sole authority for wall-clock time.
 *  - MCU uptime is NEVER used for scheduling decisions.
 *  - All scheduling derives strictly from RTC registers.
 *
 *  - No cloud.
 *  - No network time.
 *  - No fallback clocks.
 *
 *  If the RTC is invalid, the system is invalid.
 *
 * ============================================================================
 * HARDWARE (LOCKED V3.0)
 * ============================================================================
 *
 *  RTC:        NXP PCF8523T (SO8)
 *  Crystal:    AB26T 32.768 kHz, 12.5 pF load
 *  INT/CLKOUT: Pin 7 (open-drain)
 *  RTC INT →   PD2 (INT0)
 *  Pull-up:    ~10 kΩ external to VDD
 *
 * ============================================================================
 * SILICON BEHAVIOR — CRITICAL
 * ============================================================================
 *
 * 1. CLKOUT DEFAULT
 *    REG_TMR_CLKOUT defaults to COF=000 → 32.768 kHz output.
 *    If not disabled, pin 7 outputs a clock and breaks INT wake.
 *    Correct disable: COF[2:0] = 111
 *
 * 2. INT IS OPEN-DRAIN
 *    - Active-low
 *    - Latched low when AF=1 AND AIE=1
 *    - Remains low until AF cleared
 *
 * 3. OSCILLATOR STOP FLAG (OS)
 *    - Seconds register bit 7
 *    - Set when oscillator stops
 *    - Must only be cleared after verifying oscillator running
 *
 * 4. STOP BIT
 *    - CONTROL_1 bit 5
 *    - When 1 → oscillator halted
 *    - Used during atomic time set
 *
 * 5. 24-HOUR MODE
 *    - Hours register bit 5 selects 12/24 mode
 *    - Brown-outs or partial writes can corrupt this
 *    - Driver FORCES 24-hour mode at init
 *
 * ============================================================================
 * NOTE
 * ============================================================================
 * I2C must already be initialized before using this module.
 */

#include <stdint.h>
#include <stdbool.h>
#include <util/delay.h>

#include "rtc.h"
#include "i2c.h"
#include "console/mini_printf.h"

/* ============================================================================
 * REGISTER MAP
 * ========================================================================== */

#define PCF8523_ADDR7        0x68

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

#define REG_TMR_CLKOUT       0x0F

/* ============================================================================
 * CONTROL BITS
 * ========================================================================== */

#define CTRL1_STOP_BIT       (1u << 5)
#define CTRL1_AIE_BIT        (1u << 1)
#define CTRL2_AF_BIT         (1u << 3)
#define ALARM_DISABLE        (1u << 7)

/* Crystal load: AB26T requires 12.5 pF */
#define CTRL1_CL_MASK        (3u << 3)
#define CTRL1_CL_12P5PF      (1u << 3)

/* CLKOUT control */
#define CLKOUT_COF_MASK      (0x07u << 3)
#define CLKOUT_DISABLE       (0x07u << 3)

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
 * OSCILLATOR VALIDATION
 * ========================================================================== */

/**
 * Clear OS flag only if oscillator proven running.
 *
 * If OS is set:
 *   - Wait up to ~1.2 seconds for seconds rollover.
 *   - If advancing, clear OS.
 *   - If not advancing, leave OS set.
 *
 * This prevents clearing OS during partial power collapse.
 */
static void rtc_clear_os_if_running(void)
{
    uint8_t sec1, sec2;

    if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, &sec1, 1))
        return;

    if ((sec1 & 0x80u) == 0u)
        return;

    for (uint16_t i = 0; i < 1200u; i++) {

        _delay_ms(1);

        if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, &sec2, 1))
            return;

        if ((sec1 & 0x7Fu) != (sec2 & 0x7Fu))
            break;
    }

    if ((sec1 & 0x7Fu) == (sec2 & 0x7Fu))
        return;

    sec2 &= 0x7Fu;
    (void)i2c_write(PCF8523_ADDR7, REG_SECONDS, &sec2, 1);
}

/* ============================================================================
 * INITIALIZATION
 * ========================================================================== */

void rtc_init(void)
{
    uint8_t c1;

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
        return;

    /* Configure crystal load */
    c1 &= (uint8_t)~CTRL1_CL_MASK;
    c1 |= CTRL1_CL_12P5PF;

    /* Ensure oscillator running */
    c1 &= (uint8_t)~CTRL1_STOP_BIT;

    (void)i2c_write(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1);

    /* Disable CLKOUT */
    uint8_t clk;
    if (i2c_read(PCF8523_ADDR7, REG_TMR_CLKOUT, &clk, 1)) {
        clk &= (uint8_t)~CLKOUT_COF_MASK;
        clk |= CLKOUT_DISABLE;
        (void)i2c_write(PCF8523_ADDR7, REG_TMR_CLKOUT, &clk, 1);
    }

    /* Force 24-hour mode */
    {
        uint8_t hour;
        if (i2c_read(PCF8523_ADDR7, REG_HOURS, &hour, 1)) {
            hour &= (uint8_t)~(1u << 5);
            (void)i2c_write(PCF8523_ADDR7, REG_HOURS, &hour, 1);
        }
    }

    rtc_alarm_clear_flag();
    rtc_clear_os_if_running();
}

/* ============================================================================
 * STATUS
 * ========================================================================== */

bool rtc_oscillator_running(void)
{
    uint8_t c1;
    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
        return false;
    return (c1 & CTRL1_STOP_BIT) == 0u;
}

bool rtc_time_is_set(void)
{
    uint8_t sec;
    if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, &sec, 1))
        return false;
    return (sec & 0x80u) == 0u;
}

/* ============================================================================
 * TIME ACCESS
 * ========================================================================== */

void rtc_get_time(int *y, int *mo, int *d,
                  int *h, int *m, int *s)
{
    uint8_t buf[7];

    if (!i2c_read(PCF8523_ADDR7, REG_SECONDS, buf, sizeof(buf)))
        return;

    if (s)  *s  = bcd_to_bin(buf[0] & 0x7F);
    if (m)  *m  = bcd_to_bin(buf[1] & 0x7F);

    if (h) {
        uint8_t hr = buf[2] & 0x3F;   /* mask 12/24 + AM/PM */
        *h = bcd_to_bin(hr);
    }

    if (d)  *d  = bcd_to_bin(buf[3] & 0x3F);
    if (mo) *mo = bcd_to_bin(buf[5] & 0x1F);
    if (y)  *y  = 2000 + bcd_to_bin(buf[6]);
}

bool rtc_set_time(int y, int mo, int d,
                  int h, int m, int s)
{
    uint8_t c1;
    uint8_t buf[7];

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
        return false;

    /* Stop oscillator */
    c1 |= CTRL1_STOP_BIT;
    if (!i2c_write(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
        return false;

    buf[0] = bin_to_bcd((uint8_t)s) & 0x7F;
    buf[1] = bin_to_bcd((uint8_t)m) & 0x7F;
    buf[2] = bin_to_bcd((uint8_t)h) & 0x3F;   /* force 24-hour */
    buf[3] = bin_to_bcd((uint8_t)d) & 0x3F;
    buf[4] = 0;
    buf[5] = bin_to_bcd((uint8_t)mo) & 0x1F;
    buf[6] = bin_to_bcd((uint8_t)(y % 100));

    if (!i2c_write(PCF8523_ADDR7, REG_SECONDS, buf, sizeof(buf)))
        return false;

    /* Restart oscillator */
    c1 &= (uint8_t)~CTRL1_STOP_BIT;
    if (!i2c_write(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
        return false;

    return true;
}

/* ============================================================================
 * ALARM
 * ========================================================================== */

bool rtc_alarm_set_hm(uint8_t hour, uint8_t minute)
{
    if (hour > 23u || minute > 59u)
        return false;

    uint8_t c1, c2;

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
        return false;

    c1 &= (uint8_t)~CTRL1_AIE_BIT;
    if (!i2c_write(PCF8523_ADDR7, REG_CONTROL_1, &c1, 1))
        return false;

    if (!i2c_read(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1))
        return false;

    c2 &= (uint8_t)~CTRL2_AF_BIT;
    if (!i2c_write(PCF8523_ADDR7, REG_CONTROL_2, &c2, 1))
        return false;

    uint8_t a[4];
    a[0] = bin_to_bcd(minute) & 0x7F;
    a[1] = bin_to_bcd(hour) & 0x3F;
    a[2] = ALARM_DISABLE;
    a[3] = ALARM_DISABLE;

    if (!i2c_write(PCF8523_ADDR7, REG_ALARM_MINUTE, a, sizeof(a)))
        return false;

    c1 |= CTRL1_AIE_BIT;
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

/* ============================================================================
 * DEBUG
 * ========================================================================== */

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
