/*
 * door_device.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door device abstraction
 *
 * Updated: 2026-01-01
 */

#include "device.h"
#include "door.h"

static dev_state_t door_state = DEV_STATE_UNKNOWN;

static dev_state_t door_get_state(void)
{
    return door_state;
}

static void door_set_state(dev_state_t state)
{
    if (state == door_state)
        return;

    door_state = state;

    if (state == DEV_STATE_ON)
        door_open();
    else if (state == DEV_STATE_OFF)
        door_close();
}

static const char *door_state_string(dev_state_t state)
{
    switch (state) {
    case DEV_STATE_ON:  return "OPEN";
    case DEV_STATE_OFF: return "CLOSED";
    default:            return "UNKNOWN";
    }
}

Device door_device = {
    .name = "door",
    .get_state = door_get_state,
    .set_state = door_set_state,
    .state_string = door_state_string
};
