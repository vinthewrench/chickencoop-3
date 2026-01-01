/*
 * devices.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Device registry
 *
 * Notes:
 *  - Device ID == index in registry
 *  - Shared by host and firmware
 *
 * Updated: 2026-01-01
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "device.h"

/* Registry */
extern const Device *devices[];
extern const size_t device_count;

/* Optional helpers (useful, not magic) */
const Device *device_by_id(uint8_t id);
int device_lookup_id(const char *name, uint8_t *out_id);
