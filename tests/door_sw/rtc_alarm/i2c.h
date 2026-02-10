/*
 * i2c.h
 *
 * Project: Chicken Coop Controller
 * Purpose: AVR TWI (I2C) master interface
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - Blocking API with timeouts
 *  - 7-bit addressing
 *
 * Updated: 2025-12-29
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool i2c_init(uint32_t scl_hz);

/* Register-style helpers (most devices, including PCF8523) */
bool i2c_write(uint8_t addr7, uint8_t reg, const uint8_t *buf, uint8_t len);
bool i2c_read(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t len);

/* Optional raw ops if you need them later */
bool i2c_ping(uint8_t addr7);

#ifdef __cplusplus
}
#endif
