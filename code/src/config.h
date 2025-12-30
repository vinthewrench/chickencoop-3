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
 * Updated: 2025-12-29
 */

#pragma once
#include <stdbool.h>
#include "events.h"

/* Door scheduling configuration */
struct door_config {
    struct When open_when;
    struct When close_when;
};

/* Global configuration */
struct config {
    double latitude;
    double longitude;
    int    tz;          /* standard time offset */
    bool   honor_dst;   /* apply US DST rules */

    struct door_config door;
};

void config_load(struct config *cfg);
void config_save(const struct config *cfg);
void config_defaults(struct config *cfg);
