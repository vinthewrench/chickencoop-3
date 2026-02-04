#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#include "i2c.h"
#include "uart.h"

/* --------------------------------------------------------------------------
 * PCF8523 definitions
 * -------------------------------------------------------------------------- */

#define RTC_ADDR          0x68

#define REG_CONTROL_1     0x00
#define REG_SECONDS       0x03

#define CTRL1_STOP        (1 << 5)
#define CTRL1_CL_MASK     (3 << 3)
#define CTRL1_CL_12P5PF   (1 << 3)    /* try first */
//#define CTRL1_CL_7PF    (0 << 3)    /* try if drift persists */

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

static uint8_t bcd(uint8_t v)
{
    return ((v / 10) << 4) | (v % 10);
}

static uint8_t bin(uint8_t v)
{
    return ((v >> 4) * 10) + (v & 0x0F);
}

static void uart_put_u8(uint8_t v)
{
    uart_putc('0' + (v / 10));
    uart_putc('0' + (v % 10));
}

/* --------------------------------------------------------------------------
 * RTC demo setup
 * -------------------------------------------------------------------------- */

static void rtc_init_demo(void)
{
    uint8_t c1;

    /* Read Control_1 */
    if (!i2c_read(RTC_ADDR, REG_CONTROL_1, &c1, 1))
        return;

    /* Stop oscillator */
    c1 |= CTRL1_STOP;
    i2c_write(RTC_ADDR, REG_CONTROL_1, &c1, 1);

    /* Configure crystal load */
    c1 &= (uint8_t)~CTRL1_CL_MASK;
    c1 |= CTRL1_CL_12P5PF;

    /* Restart oscillator */
    c1 &= (uint8_t)~CTRL1_STOP;
    i2c_write(RTC_ADDR, REG_CONTROL_1, &c1, 1);
}

static void rtc_set_time_demo(void)
{
    uint8_t t[7];

    /* 2026-02-04 12:00:00 */
    t[0] = bcd(0)  & 0x7F;   /* seconds, OS cleared */
    t[1] = bcd(0);
    t[2] = bcd(12);
    t[3] = bcd(4);
    t[4] = 0;
    t[5] = bcd(2);
    t[6] = bcd(26);

    i2c_write(RTC_ADDR, REG_SECONDS, t, 7);
}

static void rtc_read_hms(uint8_t *h, uint8_t *m, uint8_t *s)
{
    uint8_t buf[3];

    if (!i2c_read(RTC_ADDR, REG_SECONDS, buf, 3))
        return;

    *s = bin(buf[0] & 0x7F);
    *m = bin(buf[1] & 0x7F);
    *h = bin(buf[2] & 0x3F);
}

/* --------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------- */

int main(void)
{
    uart_init();

    /* 100 kHz I2C — explicit, deterministic */
    i2c_init(100000);

    uart_putc('\n');
    uart_putc('R'); uart_putc('T'); uart_putc('C');
    uart_putc(' ');
    uart_putc('D'); uart_putc('E'); uart_putc('M'); uart_putc('O');
    uart_putc('\n');

    rtc_init_demo();
    rtc_set_time_demo();

    uint8_t last_s = 0xFF;

    for (;;) {
        uint8_t h = 0, m = 0, s = 0;

        rtc_read_hms(&h, &m, &s);

        if (s != last_s) {
            last_s = s;

            uart_put_u8(h); uart_putc(':');
            uart_put_u8(m); uart_putc(':');
            uart_put_u8(s);
            uart_putc('\n');
        }

        _delay_ms(10);
    }
}
