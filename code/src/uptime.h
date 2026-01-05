/*
 * uptime.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Source file
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *
 * Updated: 2025-12-29
 */

#pragma once

#include <stdint.h>

// Initialize uptime timebase (firmware). Host may stub.
void uptime_init(void);

// Monotonic seconds since boot.
uint32_t uptime_seconds(void);

// Monotonic milliseconds since boot.
uint32_t uptime_millis(void);
