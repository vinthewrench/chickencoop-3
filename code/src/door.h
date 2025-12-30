/*
 * door.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Door device interface
 *
 * Notes:
 *  - Door scheduling rules are declarative
 *  - Scheduler computes expected state; door reconciles hardware
 *  - Lock/unlock sequencing remains internal to door logic
 *
 * Updated: 2025-12-29
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "events.h"

#define DEVICE_DOOR  1

/* Low-level door controls */
void door_open(void);
void door_close(void);
bool door_is_open(void);

/* Emit today's declarative events derived from door rules */
size_t door_get_events(struct Event* out, size_t max);

/* Disable a scheduling rule by refnum */
void door_disable_rule(refnum_t refnum);

/* Reconcile door hardware to expected state */
void door_reconcile(enum Action expected);
