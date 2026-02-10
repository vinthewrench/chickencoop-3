/*
 * i2c_avr.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: AVR TWI (I2C) master implementation (ATmega32U4)
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - Blocking transactions with timeouts
 *  - Supports repeated-start for register reads
 *
 * Updated: 2025-12-29
 */

#include "i2c.h"

#include <avr/io.h>
#include <util/delay.h>

#ifndef F_CPU
#error "F_CPU must be defined for i2c_avr.cpp"
#endif

/* --------------------------------------------------------------------------
 * TWI status helpers
 * -------------------------------------------------------------------------- */

static inline uint8_t twi_status(void)
{
    return (uint8_t)(TWSR & 0xF8);
}

/* Timeouts are counted in simple spin loops, deterministic. */
#define I2C_SPIN_LIMIT  5000u

static bool twi_wait_twint(void)
{
    for (uint16_t i = 0; i < I2C_SPIN_LIMIT; i++) {
        if (TWCR & (1 << TWINT)) {
            return true;
        }
    }
    return false;
}

static bool twi_start(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    if (!twi_wait_twint()) return false;

    uint8_t st = twi_status();
    return (st == 0x08) || (st == 0x10); /* START or REPEATED START */
}

static void twi_stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);

    /* Optional: short settle */
    _delay_us(4);
}

static bool twi_send(uint8_t data)
{
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!twi_wait_twint()) return false;
    return true;
}

static bool twi_write_ack_ok(uint8_t expected_status)
{
    return twi_status() == expected_status;
}

static bool twi_read_byte(uint8_t *out, bool ack)
{
    if (ack) {
        TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    } else {
        TWCR = (1 << TWINT) | (1 << TWEN);
    }

    if (!twi_wait_twint()) return false;

    *out = TWDR;
    return true;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

bool i2c_init(uint32_t scl_hz)
{
    if (scl_hz == 0) return false;

    /* Prescaler = 1 */
    TWSR &= ~((1 << TWPS0) | (1 << TWPS1));

    /* TWBR calculation: SCL = F_CPU / (16 + 2*TWBR*prescaler) */
    uint32_t twbr = ((uint32_t)F_CPU / scl_hz - 16u) / 2u;
    if (twbr > 255u) twbr = 255u;

    TWBR = (uint8_t)twbr;

    /* Enable TWI */
    TWCR = (1 << TWEN);

    return true;
}

bool i2c_ping(uint8_t addr7)
{
    if (!twi_start()) return false;

    uint8_t sla_w = (uint8_t)((addr7 << 1) | 0);
    if (!twi_send(sla_w)) { twi_stop(); return false; }

    bool ok = twi_write_ack_ok(0x18) || twi_write_ack_ok(0x20) == false;
    /* 0x18 SLA+W ACK, 0x20 SLA+W NACK */

    twi_stop();
    return ok;
}

bool i2c_write(uint8_t addr7, uint8_t reg, const uint8_t *buf, uint8_t len)
{
    if (!twi_start()) return false;

    uint8_t sla_w = (uint8_t)((addr7 << 1) | 0);
    if (!twi_send(sla_w)) { twi_stop(); return false; }
    if (!twi_write_ack_ok(0x18)) { twi_stop(); return false; }

    if (!twi_send(reg)) { twi_stop(); return false; }
    if (!twi_write_ack_ok(0x28)) { twi_stop(); return false; }

    for (uint8_t i = 0; i < len; i++) {
        if (!twi_send(buf[i])) { twi_stop(); return false; }
        if (!twi_write_ack_ok(0x28)) { twi_stop(); return false; }
    }

    twi_stop();
    return true;
}

bool i2c_read(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t len)
{
    if (len == 0) return true;

    /* Write register pointer */
    if (!twi_start()) return false;

    uint8_t sla_w = (uint8_t)((addr7 << 1) | 0);
    if (!twi_send(sla_w)) { twi_stop(); return false; }
    if (!twi_write_ack_ok(0x18)) { twi_stop(); return false; }

    if (!twi_send(reg)) { twi_stop(); return false; }
    if (!twi_write_ack_ok(0x28)) { twi_stop(); return false; }

    /* Repeated start, then read */
    if (!twi_start()) { twi_stop(); return false; }

    uint8_t sla_r = (uint8_t)((addr7 << 1) | 1);
    if (!twi_send(sla_r)) { twi_stop(); return false; }
    if (!twi_write_ack_ok(0x40)) { twi_stop(); return false; }

    for (uint8_t i = 0; i < len; i++) {
        bool ack = (i + 1u) < len;
        if (!twi_read_byte(&buf[i], ack)) { twi_stop(); return false; }

        uint8_t st = twi_status();
        if (ack) {
            if (st != 0x50) { twi_stop(); return false; } /* data received, ACK returned */
        } else {
            if (st != 0x58) { twi_stop(); return false; } /* data received, NACK returned */
        }
    }

    twi_stop();
    return true;
}
