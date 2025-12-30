/*
 * devices.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Device registry
 *
 * Rules:
 *  - Defines the ordered list of active devices
 *  - Exposes device_count as part of the registry contract
 *  - No device logic lives here
 *
 * Updated: 2025-12-30
 */

 #include "devices.h"
 #include "door_device.h"
 #include "foo_device.h"

 #include <string.h>

 const Device *devices[] = {
     &door_device,
     &foo_device,

 };

 const size_t device_count =
     sizeof(devices) / sizeof(devices[0]);

bool devices_lookup_id(const char *name, uint8_t *out_id)
{
    if (!name || !out_id)
        return false;

    for (uint8_t i = 0; i < device_count; i++) {
        const Device *d = devices[i];
        if (d && d->name && !strcmp(d->name, name)) {
            *out_id = i;
            return true;
        }
    }
    return false;
}
