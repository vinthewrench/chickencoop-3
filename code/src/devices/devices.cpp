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

/* Registry table */
const Device *devices[] = {
    &door_device,  /* ID 0 */
    &foo_device,   /* ID 1 */
    &relay1_device,  /* ID 2*/
    &relay2_device  /* ID 2*/
};

const size_t device_count =
    sizeof(devices) / sizeof(devices[0]);

const Device *device_by_id(uint8_t id)
{
    if (id >= device_count)
        return NULL;
    return devices[id];
}

int device_lookup_id(const char *name, uint8_t *out_id)
{
    if (!name || !out_id)
        return 0;

    for (size_t i = 0; i < device_count; i++) {
        if (strcmp(devices[i]->name, name) == 0) {
            *out_id = (uint8_t)i;
            return 1;
        }
    }
    return 0;
}
