/*
 * device_ids.h
 *
 * Stable device identifiers.
 * These IDs are used by config, events, and scheduler.
 * Order must never be repurposed.
 */

#pragma once

#include <stdint.h>

typedef enum {
    DEVICE_ID_NONE   = 0x00,

    DEVICE_ID_DOOR   = 0x01,
    DEVICE_ID_LED    = 0x03,
    DEVICE_ID_RELAY1 = 0x04,
    DEVICE_ID_RELAY2 = 0x05,
    DEVICE_ID_FOO    = 0x06,

    DEVICE_ID_MAX_PLUS_ONE   /* not a device, marks table size */
} device_id_t;

/* Size of device lookup table */
#define DEVICE_ID_TABLE_SIZE  ((uint8_t)DEVICE_ID_MAX_PLUS_ONE)
