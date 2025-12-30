/*
 * devices.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Device registry
 *
 * Updated: 2025-12-30
 */

#pragma once
#include <stddef.h>
#include "device.h"

extern const Device *devices[];
extern const size_t device_count;

/* Registry helpers */
bool devices_lookup_id(const char *name, uint8_t *out_id);
