/*
 * config.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Persistent configuration storage
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No device-specific logic
 *  - Stores declarative scheduling intent only
 *
 * Updated: 2025-12-30
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "config_events.h"

/* Global configuration */
struct config {
    int32_t latitude_e4;    /* degrees * 10000 */
    int32_t longitude_e4;   /* degrees * 10000 */
    int32_t tz;             /* minutes offset from UTC */
    uint8_t honor_dst;      /* 0 or 1 */
    uint8_t _pad[3];        /* explicit padding to 32-bit boundary */

    struct Event events[MAX_EVENTS];

    uint16_t checksum;      /* Fletcher-16 over all fields above */
};

bool config_load(struct config *cfg);
void config_save(const struct config *cfg);
void config_defaults(struct config *cfg);

/* Checksum (shared: host + AVR) */
uint16_t config_fletcher16(const void *data, size_t len);

extern struct config g_cfg;
