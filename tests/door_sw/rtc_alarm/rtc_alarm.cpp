#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <util/delay.h>

#include "i2c.h"
#include "uart.h"
#include "mini_printf.h"

/* ===================== PCF8523 ===================== */

#define RTC_ADDR          0x68

#define REG_CONTROL_1     0x00
#define REG_CONTROL_2     0x01
#define REG_SECONDS       0x03

#define REG_ALM_MIN       0x0A
#define REG_ALM_HOUR      0x0B
#define REG_ALM_DAY       0x0C
#define REG_ALM_WKDAY     0x0D

#define CTRL1_STOP        (1u << 5)
#define CTRL1_CL_MASK     (3u << 3)
#define CTRL1_CL_12P5PF   (1u << 3)
#define CTRL1_AIE         (1u << 1)

#define CTRL2_AF          (1u << 3)

#define ALARM_DISABLE     (1u << 7)

/* ===================== Globals ===================== */

volatile uint8_t g_woke = 0;

/* RTC_INT is on PD2 (INT0), not PB7. */
ISR(INT0_vect)
{
    /* One-shot: mask INT0 until main clears AF and re-arms. */
    EIMSK &= (uint8_t)~(1u << INT0);
    g_woke = 1;
}

static void uart_flush(void)
{
    /* Wait for TX shift register empty (safe, deterministic) */
    while (!(UCSR0A & (1 << UDRE0))) {
        /* wait */
    }

    /* Then wait for "transmit complete" (last byte fully sent) */
    UCSR0A |= (1 << TXC0);              /* clear first */
    while (!(UCSR0A & (1 << TXC0))) {   /* wait until set */
        /* wait */
    }
    UCSR0A |= (1 << TXC0);              /* clear again */
}

/* ===================== INT0 setup (PD2) ===================== */

/* ===================== INT0 setup (PD2) ===================== */
/* FIX: For wake from SLEEP_MODE_PWR_DOWN, INT0 must be LOW-LEVEL, not falling edge. */
static void int0_init_pd2(void)
{
    /* PD2 input. External pull-up required. */
    DDRD  &= (uint8_t)~(1u << PD2);
    PORTD &= (uint8_t)~(1u << PD2);

    /* LOW level trigger: ISC01=0, ISC00=0 */
    EICRA &= (uint8_t)~((1u << ISC01) | (1u << ISC00));

    /* Clear pending and enable */
    EIFR  |= (uint8_t)(1u << INTF0);
    EIMSK |= (uint8_t)(1u << INT0);
}

/* ===================== Sleep ===================== */
/* FIX: race-free entry for LOW-level INT wake (don’t sleep if line already low). */
static void sleep_now(void)
{

    cli();

    /* Clear any latched interrupt flag */
    EIFR |= (uint8_t)(1u << INTF0);

    /* If RTC INT is already low, do NOT sleep. */
    if ((PIND & (1u << PD2)) == 0) {
        sei();
        return;
    }

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();

    /* Interrupts on, then sleep immediately. */
    sei();

    mini_printf("SMCR=%02X  SE=%u  SM=%u\n",
                SMCR,
                (SMCR & _BV(SE)) ? 1u : 0u,
                (SMCR >> SM0) & 0x07u);
    uart_flush();

    sleep_cpu();

    /* Execution resumes here after wake. */
    cli();
    sleep_disable();
    sei();
}

/* ===================== RTC ===================== */

/* ===================== Helpers ===================== */

static uint8_t bcd(uint8_t v)
{
    return (uint8_t)(((v / 10u) << 4) | (v % 10u));
}

static uint8_t bin(uint8_t v)
{
    return (uint8_t)(((v >> 4) * 10u) + (v & 0x0Fu));
}

/* ===================== RTC Core ===================== */

static void rtc_init(void)
{
    uint8_t c1;
    uint8_t clk;
    uint8_t c2;

    /* CONTROL_1 */
    if (!i2c_read(RTC_ADDR, REG_CONTROL_1, &c1, 1)) return;

    /* 12.5pF crystal load */
    c1 &= (uint8_t)~CTRL1_CL_MASK;
    c1 |= CTRL1_CL_12P5PF;

    /* Ensure oscillator running */
    c1 &= (uint8_t)~CTRL1_STOP;

    /* Ensure MI=0 (interrupts not masked) */
    c1 &= (uint8_t)~(1u << 7);

    i2c_write(RTC_ADDR, REG_CONTROL_1, &c1, 1);

    /* Disable CLKOUT (COF[2:0] = 111) */
    if (i2c_read(RTC_ADDR, 0x0F, &clk, 1)) {
        clk &= (uint8_t)~(0x07u << 3);
        clk |=  (uint8_t)(0x07u << 3);
        i2c_write(RTC_ADDR, 0x0F, &clk, 1);
    }

    /* Clear stale AF */
    if (i2c_read(RTC_ADDR, REG_CONTROL_2, &c2, 1)) {
        c2 &= (uint8_t)~CTRL2_AF;
        i2c_write(RTC_ADDR, REG_CONTROL_2, &c2, 1);
    }
}

static void rtc_read_hms(uint8_t *h, uint8_t *m, uint8_t *s)
{
    uint8_t buf[3];

    if (!i2c_read(RTC_ADDR, REG_SECONDS, buf, 3))
        return;

    *s = bin(buf[0] & 0x7Fu);
    *m = bin(buf[1] & 0x7Fu);
    *h = bin(buf[2] & 0x3Fu);
}

static void rtc_alarm_clear(void)
{
    uint8_t c2;

    if (!i2c_read(RTC_ADDR, REG_CONTROL_2, &c2, 1))
        return;

    c2 &= (uint8_t)~CTRL2_AF;
    i2c_write(RTC_ADDR, REG_CONTROL_2, &c2, 1);
}

static void rtc_print_time(const char *tag)
{
    uint8_t h=0, m=0, s=0;
    rtc_read_hms(&h, &m, &s);
    mini_printf("%s %02u:%02u:%02u\n", tag, h, m, s);
}

static void rtc_alarm_set_next_minute(void)
{
    uint8_t h = 0, m = 0, s = 0;
    rtc_read_hms(&h, &m, &s);

    mini_printf("NOW  %02u:%02u:%02u\n", h, m, s);

    /*
     * PCF8523 alarm compares minute+hour for the whole minute (seconds ignored).
     * If you clear AF while the match condition is true, AF re-asserts immediately
     * and INT can stay LOW for the entire minute. Since INT0 is LOW-level wake in
     * PWR_DOWN, you will "wake instantly" and get UART garbage.
     *
     * Fix: arm for (next minute boundary) + 2 minutes, not + 1.
     * That guarantees we're outside any match window during arming/clearing.
     */

    /* normalize to the NEXT minute boundary */
    if (s != 0u) {
        m++;
        if (m >= 60u) {
            m = 0u;
            h = (uint8_t)((h + 1u) % 24u);
        }
    }

    /* +2 minutes from that boundary */
    m = (uint8_t)(m + 2u);
    if (m >= 60u) {
        m = (uint8_t)(m - 60u);
        h = (uint8_t)((h + 1u) % 24u);
    }

    mini_printf("ARM  %02u:%02u\n", h, m);

    uint8_t c1 = 0, c2 = 0;

    /* Disable AIE while programming */
    if (!i2c_read(RTC_ADDR, REG_CONTROL_1, &c1, 1)) return;
    c1 &= (uint8_t)~CTRL1_AIE;
    i2c_write(RTC_ADDR, REG_CONTROL_1, &c1, 1);

    /* Clear AF to release INT */
    if (!i2c_read(RTC_ADDR, REG_CONTROL_2, &c2, 1)) return;
    c2 &= (uint8_t)~CTRL2_AF;
    i2c_write(RTC_ADDR, REG_CONTROL_2, &c2, 1);

    /* Program alarm minute/hour; disable day/weekday */
    uint8_t a[4];
    a[0] = (uint8_t)(bcd(m) & 0x7Fu);
    a[1] = (uint8_t)(bcd(h) & 0x3Fu);
    a[2] = (uint8_t)ALARM_DISABLE;
    a[3] = (uint8_t)ALARM_DISABLE;
    i2c_write(RTC_ADDR, REG_ALM_MIN, a, 4);

    /* Clear AF again (defensive) */
    if (!i2c_read(RTC_ADDR, REG_CONTROL_2, &c2, 1)) return;
    c2 &= (uint8_t)~CTRL2_AF;
    i2c_write(RTC_ADDR, REG_CONTROL_2, &c2, 1);

    /* Enable AIE */
    if (!i2c_read(RTC_ADDR, REG_CONTROL_1, &c1, 1)) return;
    c1 |= (uint8_t)CTRL1_AIE;
    i2c_write(RTC_ADDR, REG_CONTROL_1, &c1, 1);

    /* Status print */
    i2c_read(RTC_ADDR, REG_CONTROL_1, &c1, 1);
    i2c_read(RTC_ADDR, REG_CONTROL_2, &c2, 1);

    mini_printf("C1=%02X C2=%02X AIE=%u AF=%u PD2=%u\n",
                c1, c2,
                (c1 & CTRL1_AIE) ? 1u : 0u,
                (c2 & CTRL2_AF) ? 1u : 0u,
                (PIND & (1u << PD2)) ? 1u : 0u);
}

/* ===================== main ===================== */

int main(void)
{
    uart_init();
    i2c_init(100000);

    rtc_init();          /* includes CLKOUT disable */
    int0_init_pd2();     /* PD2 is the wake line */

    mini_printf("\033[2J\033[3J\033[H");
    mini_printf("\n\nALARM SLEEP TEST START\n");

    for (;;) {

        /* Disable alarm interrupt first */
        uint8_t c1;
        i2c_read(RTC_ADDR, REG_CONTROL_1, &c1, 1);
        c1 &= (uint8_t)~CTRL1_AIE;
        i2c_write(RTC_ADDR, REG_CONTROL_1, &c1, 1);

        /* Clear AF */
        rtc_alarm_clear();

        /* Wait until minute changes so match condition is gone */
        uint8_t h, m, s;
        uint8_t last_minute;

        rtc_read_hms(&h, &m, &s);
        last_minute = m;

        mini_printf("Waiting for minute to roll...\n");

        while (1) {
            rtc_read_hms(&h, &m, &s);
            if (m != last_minute)
                break;
        }

        rtc_alarm_set_next_minute();

        g_woke = 0;

        mini_printf("SLEEP\n");

        uart_flush();

        sleep_now();

        mini_printf("WAKE g_woke=%u\n", g_woke);
        rtc_print_time("WAKE ");

        {
            uint8_t c2 = 0;
            i2c_read(RTC_ADDR, REG_CONTROL_2, &c2, 1);

            mini_printf("POST C2=%02X AF=%u PD2=%u\n",
                        c2,
                        (c2 & CTRL2_AF) ? 1u : 0u,
                        (PIND & (1u << PD2)) ? 1u : 0u);

            /* Clear AF to release RTC INT (open drain). */
            rtc_alarm_clear();

            /* Wait until PD2 returns high, otherwise INT0 low-level will keep waking. */
            for (uint16_t i = 0; i < 50000u; i++) {
                if (PIND & (1u << PD2)) break;
                _delay_us(10);
            }

            mini_printf("POSTCLR PD2=%u\n", (PIND & (1u << PD2)) ? 1u : 0u);

            /* Re-arm INT0 for next cycle */
            EIFR  |= (uint8_t)(1u << INTF0);
            EIMSK |= (uint8_t)(1u << INT0);
        }
    }

}
