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
// extern const Device *devices[];
// extern const size_t device_count;

size_t device_count();
bool device_id(size_t deviceNum, uint8_t *out_id);
bool device_name(uint8_t id, const char**device_name_out);

bool device_lookup_id(const char *name, uint8_t *out_id);

bool device_set_state_by_id(uint8_t id, dev_state_t state);
bool device_get_state_by_id(uint8_t id, dev_state_t *out_state);

bool device_parse_state_by_id(uint8_t id,
                              const char *arg,
                              dev_state_t *out);

bool device_get_state_string(uint8_t id, dev_state_t state, const char**state_string_out);

void device_init();

void device_tick(uint32_t now_ms);
