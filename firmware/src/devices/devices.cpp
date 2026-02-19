/*
 * devices.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Device registry implementation
 *
 * Updated: 2026-01-08
 */

#include "devices.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Device implementations (provided elsewhere)
 * -------------------------------------------------------------------------- */

extern const Device door_device;
extern const Device led_device;
extern const Device relay1_device;
extern const Device relay2_device;
//extern const Device foo_device;

/* --------------------------------------------------------------------------
 * Registry table (sparse, explicit IDs)
 * -------------------------------------------------------------------------- */

static const Device *devices[DEVICE_ID_TABLE_SIZE];

/* --------------------------------------------------------------------------
 * Initialization
 * -------------------------------------------------------------------------- */

void device_init(void)
{
    /* Clear registry */
    for (size_t i = 0; i < DEVICE_ID_TABLE_SIZE; i++) {
        devices[i] = NULL;
    }

    /* Register devices by explicit ID */
    devices[DEVICE_ID_DOOR]   = &door_device;
    devices[DEVICE_ID_LED]    = &led_device;
    devices[DEVICE_ID_RELAY1] = &relay1_device;
    devices[DEVICE_ID_RELAY2] = &relay2_device;
  //  devices[DEVICE_ID_FOO]    = &foo_device;

    /* Initialize registered devices only */
    for (size_t i = 0; i < DEVICE_ID_TABLE_SIZE; i++) {
        const Device *dev = devices[i];
        if (!dev)
            continue;

        if (dev->init)
            dev->init();
    }
}

/* --------------------------------------------------------------------------
 * Internal helper
 * -------------------------------------------------------------------------- */

static const Device *device_by_id(uint8_t id)
{
    if (id >= DEVICE_ID_TABLE_SIZE)
        return NULL;

    return devices[id];
}


/* --------------------------------------------------------------------------
 * Enumeration
 * -------------------------------------------------------------------------- */

bool device_enum_first(uint8_t *out_id)
{
    if (!out_id)
        return false;

    for (uint8_t id = 0; id < DEVICE_ID_TABLE_SIZE; id++) {
        if (devices[id]) {
            *out_id = id;
            return true;
        }
    }

    return false;
}

bool device_enum_next(uint8_t cur_id, uint8_t *out_id)
{
    if (!out_id)
        return false;

    if (cur_id >= DEVICE_ID_TABLE_SIZE - 1)
        return false;

    for (uint8_t id = cur_id + 1; id < DEVICE_ID_TABLE_SIZE; id++) {
        if (devices[id]) {
            *out_id = id;
            return true;
        }
    }

    return false;
}

/* --------------------------------------------------------------------------
 * Lookup
 * -------------------------------------------------------------------------- */

bool device_lookup_id(const char *name, uint8_t *out_id)
{
    if (!name || !out_id)
        return false;

    for (uint8_t id = 0; id < DEVICE_ID_TABLE_SIZE; id++) {
        const Device *dev = devices[id];
        if (!dev || !dev->name)
            continue;

        if (strcmp(dev->name, name) == 0) {
            *out_id = id;
            return true;
        }
    }

    return false;
}

/* --------------------------------------------------------------------------
 * Periodic service
 * -------------------------------------------------------------------------- */

void device_tick(uint32_t now_ms)
{
    for (size_t i = 0; i < DEVICE_ID_TABLE_SIZE; i++) {
        const Device *dev = devices[i];
        if (!dev)
            continue;

        if (dev->tick)
            dev->tick(now_ms);
    }
}

/* --------------------------------------------------------------------------
 * State access
 * -------------------------------------------------------------------------- */

bool device_set_state_by_id(uint8_t id, dev_state_t state)
{
    const Device *dev = device_by_id(id);
    if (!dev || !dev->set_state)
        return false;

    dev->set_state(state);
    return true;
}


bool device_schedule_state_by_id(uint8_t id,
                                 dev_state_t state,
                                 uint32_t when)
{
    const Device *dev = device_by_id(id);
    if (!dev)  return false;

    if(dev->schedule_state){
        dev->schedule_state(state, when);
        return true;
    }
    else if(dev->set_state){
        dev->set_state(state);
        return true;
    }

    return false;

}

bool device_get_state_by_id(uint8_t id, dev_state_t *out_state)
{
    if (!out_state)
        return false;

    const Device *dev = device_by_id(id);
    if (!dev)
        return false;

    if (!dev->get_state) {
        *out_state = DEV_STATE_UNKNOWN;
        return true;
    }

    *out_state = dev->get_state();
    return true;
}

bool device_get_state_string(uint8_t id,
                             dev_state_t state,
                             const char **out_string)
{
    if (!out_string)
        return false;

    const Device *dev = device_by_id(id);
    if (!dev || !dev->state_string)
        return false;

    *out_string = dev->state_string(state);
    return (*out_string != NULL);
}

bool device_name(uint8_t id, const char **out_name)
{
    if (!out_name)
        return false;

    const Device *dev = device_by_id(id);
    if (!dev || !dev->name)
        return false;

    *out_name = dev->name;
    return true;
}

/* --------------------------------------------------------------------------
 * State parsing
 * -------------------------------------------------------------------------- */

bool device_parse_state_by_id(uint8_t id,
                              const char *arg,
                              dev_state_t *out)
{
    if (!arg || !out)
        return false;

    const Device *dev = device_by_id(id);
    if (!dev)
        return false;

    /* Device-specific names first */
    if (dev->state_string) {
        for (dev_state_t s = DEV_STATE_UNKNOWN;
             s <= DEV_STATE_ON;
             s = (dev_state_t)(s + 1)) {

            const char *name = dev->state_string(s);
            if (!name)
                continue;

            if (!strcasecmp(arg, name)) {
                *out = s;
                return true;
            }
        }
    }

    /* Generic fallback */
    if (!strcasecmp(arg, "on")) {
        *out = DEV_STATE_ON;
        return true;
    }

    if (!strcasecmp(arg, "off")) {
        *out = DEV_STATE_OFF;
        return true;
    }

    return false;
}


/*
 * devices_busy()
 *
 * Returns true if any device state machine is currently active
 * and requires CPU time.
 *

 * Returns:
 *   true  → System must remain awake
 *   false → Safe to enter sleep
 */
bool devices_busy(void){

    for (size_t i = 0; i < DEVICE_ID_TABLE_SIZE; i++) {
        const Device *dev = devices[i];
        if (!dev)
            continue;

        if (dev->is_busy)
            if(dev->is_busy()) return true;
    }
    return false;
}



bool device_is_busy(uint8_t id)
{
    const Device *dev = device_by_id(id);
    if (!dev || !dev->is_busy)
        return false;

    return dev->is_busy;
}
