/*
 * config_events.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Event configuration storage
 *
 * Notes:
 *  - Authoritative event list
 *  - Mutable via console only
 *  - Read-only to scheduler
 *
 * Updated: 2025-12-30
 */

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "events.h"

#ifdef HOST_BUILD
#define MAX_EVENTS 16
#else
#define MAX_EVENTS 8
#endif

typedef struct {
    Event   events[MAX_EVENTS];
    uint8_t count;
} EventTable;

/* Accessors */
const Event *config_events_get(size_t *count);

/* Mutators */
bool config_events_add(const Event *ev);
bool config_events_update(uint8_t index, const Event *ev);
bool config_events_delete(uint8_t index);

/* Utilities */
void config_events_clear(void);
