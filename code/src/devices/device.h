/*
 * device.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Device event interface for scheduler
 *
 * Rules:
 *  - Devices emit declarative events only
 *  - No execution, no timing logic
 *  - Stable refnums per rule
 *
 * Updated: 2025-12-30
 */

#pragma once
#include <stddef.h>
#include "events.h"

typedef struct {
    const char *name;

     /* Reconcile expected state (not used yet) */
    void (*reconcile)(Action expected);

} Device;
