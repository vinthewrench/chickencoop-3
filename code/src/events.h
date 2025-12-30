/*
 * events.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Shared scheduling/event model (host + firmware)
 *
 * Design principles:
 *  - Offline, deterministic system
 *  - No heap, no STL, no exceptions
 *  - Declarative intent only (events are not executed or replayed)
 *  - Expected state is derived from events, not history
 *
 * Notes:
 *  - All times are minute-of-day (0..1439)
 *  - Invalid or out-of-range times are discarded, never wrapped
 *
 * Updated: 2025-12-29
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t refnum_t;

/* Time reference used for resolving events */
enum TimeRef : uint8_t {
    REF_NONE = 0,      /* Disabled rule */
    REF_MIDNIGHT,      /* 00:00 local */
    REF_SOLAR_STD,     /* Standard sunrise/sunset */
    REF_SOLAR_CIV      /* Civil twilight sunrise/sunset */
};

/* Declarative time expression */
struct When {
    enum TimeRef ref;
    int16_t offset_minutes; /* signed offset from reference */
};

/* Generic device action */
enum Action : uint8_t {
    ACTION_OFF = 0,
    ACTION_ON  = 1
};

/* Declarative scheduling event */
struct Event {
    uint8_t device_id;   /* DEVICE_* identifier */
    enum Action action;  /* Expected action */
    struct When when;    /* Time expression */
    refnum_t refnum;     /* Rule identifier */
};

/* Fully-resolved event for today */
struct ResolvedEvent {
    uint8_t device_id;
    enum Action action;
    refnum_t refnum;
    uint16_t minute;     /* 0..1439 */
};
