/*
 * door_device.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door device abstraction (temporary hardware drive)
 *
 * Notes:
 *  - TEMPORARY: directly drives door_hw
 *  - No timing, no blocking
 *  - Device state reflects scheduler intent only
 *  - Will be refactored to use door_control
 *
 * Updated: 2026-01-02
 */

#include "device.h"
#include "door.h"
#include "door_hw.h"

static dev_state_t door_state = DEV_STATE_UNKNOWN;

/*
 * Scheduler-declared state only.
 * This is NOT physical position.
 */
static dev_state_t door_get_state(void)
{
    return door_state;
}

static void door_set_state(dev_state_t state)
{
    if (state == door_state)
        return;

    door_state = state;

    if (state == DEV_STATE_ON) {
        /* OPEN */
        door_hw_set_open_dir();
        door_hw_enable();
    }
    else if (state == DEV_STATE_OFF) {
        /* CLOSE */
        door_hw_set_close_dir();
        door_hw_enable();
    }
    else {
        /* UNKNOWN → safest action is stop */
        door_hw_stop();
    }
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
