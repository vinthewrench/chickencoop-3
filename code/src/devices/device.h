/*
 * device.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Generic device interface
 *
 * Notes:
 *  - Devices are dumb
 *  - No scheduling or event knowledge
 *  - Scheduler decides WHAT, devices decide HOW
 *
 * Updated: 2026-01-01
 */

#pragma once
#include <stddef.h>
#include <stdint.h>

/* Device-visible state */
typedef enum {
    DEV_STATE_UNKNOWN = 0,
    DEV_STATE_OFF,
    DEV_STATE_ON
} dev_state_t;

/* Generic device vtable */
typedef struct {
    const char *name;
    void        (*init)(void);
    dev_state_t (*get_state)(void);
    void        (*set_state)(dev_state_t state);
    const char *(*state_string)(dev_state_t state);
    void        (*tick)(uint32_t now_ms);

} Device;
