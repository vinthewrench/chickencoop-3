/*
 * config.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Persistent configuration storage
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - Self-describing configuration
 *  - Identical layout on host and AVR
 *
 * Time Model (UPDATED):
 *  - RTC runs in UTC.
 *  - Scheduler runs in UTC.
 *  - Solar scheduling uses UTC (tz = 0 when calling solar_compute).
 *
 *  - tz and honor_dst are for console/UI presentation only:
 *      display LOCAL time
 *      accept LOCAL time input (convert to UTC before writing RTC)
 *
 *  - rtc_set_epoch is stored in UTC epoch seconds (2000 base) and is used
 *    only for drift tracking (time since last manual set).
 *
 * Updated: 2026-02-16
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "config_events.h"

/* Config identity */
#define CONFIG_MAGIC   0x434F4F50UL  /* 'COOP' */
#define CONFIG_VERSION 2

struct config {
    /* Identity */
    uint32_t magic;
    uint8_t  version;
    uint8_t  _pad0[3];          /* align to 32-bit */

    /* Location / time */
    int32_t latitude_e4;        /* degrees * 10000 */
    int32_t longitude_e4;       /* degrees * 10000 */

    /*
     * Timezone offset from UTC in HOURS.
     *
     * Example:
     *   Arkansas (CST) = -6
     *
     * This value is used for console/UI conversion only.
     * It must NOT affect scheduler or RTC storage.
     */
    int32_t tz;

    /*
     * Apply US DST rule for console/UI presentation only.
     *
     * Must NOT affect scheduler or RTC storage.
     */
    uint8_t honor_dst;          /* 0 or 1 */

    /*
     * last time we set the clock
     *
     * Stored as UTC epoch seconds with 2000-01-01 base (per rtc_get_epoch()).
     * Used only for drift tracking.
     */
    uint32_t rtc_set_epoch;

    /* Mechanical timing (physical constants) */
    uint16_t door_travel_ms;    /* full open or close time */
    uint16_t lock_pulse_ms;     /* solenoid energize duration */
    uint16_t door_settle_ms;    /* delay after close before locking */
    uint16_t lock_settle_ms;    /* time after unlock before motion */

    uint8_t _pad1[2];           /* align events */

    /* Scheduler intent */
    struct Event events[MAX_EVENTS];

    /* Integrity */
    uint16_t checksum;          /* Fletcher-16 over all fields above */
};

/* API */
bool config_load(struct config *cfg);
void config_save(const struct config *cfg);
void config_defaults(struct config *cfg);

/* Checksum helper */
uint16_t config_fletcher16(const void *data, size_t len);

extern struct config g_cfg;
