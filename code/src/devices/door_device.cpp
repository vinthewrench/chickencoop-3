/*
 * door_device.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door device adapter (Device API → door state machine)
 *
 * Notes:
 *  - Implements Device interface
 *  - Delegates motion and timing to door_state_machine
 *  - No direct hardware control here
 *
 * Updated: 2026-01-06
 */

#include "device.h"
#include "door_state_machine.h"

/*
 * Device-visible state only.
 * This reflects settled truth, not motion.
 */
static dev_state_t door_get_state(void)
{
    return door_sm_get_state();
}

static void door_set_state(dev_state_t state)
{
    /*
     * Only ON/OFF are meaningful requests.
     * UNKNOWN is ignored.
     */
    if (state == DEV_STATE_ON || state == DEV_STATE_OFF)
        door_sm_request(state);
}

static const char *door_state_string(dev_state_t state)
{
    switch (state) {
    case DEV_STATE_ON:  return "OPEN";
    case DEV_STATE_OFF: return "CLOSED";
    default:            return "UNKNOWN";
    }
}

static void door_init(void)
{
    door_sm_init();
}

static void door_tick(uint32_t now_ms)
{
    door_sm_tick(now_ms);
}

/* --------------------------------------------------------------------------
 * Device registration
 * -------------------------------------------------------------------------- */

Device door_device = {
    .name         = "door",
    .init         = door_init,
    .get_state    = door_get_state,
    .set_state    = door_set_state,
    .state_string = door_state_string,
    .tick         = door_tick
};
