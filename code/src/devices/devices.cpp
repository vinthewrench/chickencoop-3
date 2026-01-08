/*
 * devices.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Device registry implementation
 *
 * Updated: 2026-01-01
 */

#include "devices.h"
#include <string.h>

/* Provided by device implementation units */
extern const Device door_device;
extern const Device foo_device;
extern const Device relay1_device;
extern const Device relay2_device;
extern const Device led_device;
extern const Device lock_device;


/* Registry table */
const Device *devices[] = {
    &door_device,       /* ID 0 */
    &lock_device,        /* ID 1 */
    &relay1_device,     /* ID 2*/
    &relay2_device,     /* ID 3*/
    &led_device,        /* ID 4*/
    &foo_device,        /* ID ? */
 };

const size_t max_devices =
    sizeof(devices) / sizeof(devices[0]);

size_t device_count() {
    return max_devices;
}


bool device_id(size_t deviceNum, uint8_t *out_id){

    if (deviceNum >= max_devices)
        return false;

    if(out_id) *out_id = (uint8_t) deviceNum;

    return true;
}



const Device *device_by_id(uint8_t id)
{
    if (id >= max_devices)
        return NULL;
    return devices[id];
}

/*
 * Look up a device by name.
 *
 * Returns:
 *  - 1 if the device is found (success), and *out_id is set
 *  - 0 if not found or on invalid arguments
 */

bool device_lookup_id(const char *name, uint8_t *out_id)
{
    if (!name || !out_id)
        return 0;

    for (size_t i = 0; i < max_devices; i++) {
        if (strcmp(devices[i]->name, name) == 0) {
            if(out_id)  *out_id = (uint8_t)i;
            return true;
        }
    }
    return false;
}

void device_tick(uint32_t now_ms)
{
    for (size_t i = 0; i < max_devices; i++) {
        const Device *dev = devices[i];

        if (dev->tick)
            dev->tick(now_ms);
    }
}


void device_init()
{
    for (size_t i = 0; i < max_devices; i++) {
        const Device *dev = devices[i];

        if (dev->init)
            dev->init();
    }
}


bool device_set_state_by_id(uint8_t id, dev_state_t state)
{
    const Device *dev = device_by_id(id);
    if (!dev)
        return false;

    if (!dev->set_state)
        return false;

    dev->set_state(state);
    return true;
}

bool device_get_state_by_id(uint8_t id, dev_state_t *out_state)
{
    if (!out_state)
        return false;

    const Device *dev = device_by_id(id);
    if (!dev || !dev->get_state)
        return false;

    *out_state = dev->get_state();
    return true;
}


bool device_get_state_string(uint8_t id, dev_state_t state, const char**state_string){

    const Device *dev = device_by_id(id);
    if (!dev || !dev->get_state)
        return false;

    if(state_string)
        *state_string = dev->state_string(state);

    return true;
}


bool device_name(uint8_t id, const char**device_name){

    const Device *dev = device_by_id(id);
    if (!dev)
        return false;

    if(device_name)
        *device_name = dev->name;

    return true;
}



/*
 * Parse a device state from user input.
 *
 * Rules:
 *  - Prefer device-defined state_string()
 *  - Case-insensitive match
 *  - Fallback to "on"/"off"
 */

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
