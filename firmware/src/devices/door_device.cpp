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

static void door_schedule_state(dev_state_t state, uint32_t when)
{
    if (state == DEV_STATE_ON || state == DEV_STATE_OFF)
        door_sm_schedule(state, when);
}


static const char *door_state_string(dev_state_t state)
{
    if (state == DEV_STATE_ON)
        return "OPEN";

    if (state == DEV_STATE_OFF)
        return "CLOSED";

    /* If unsettled, reflect motion truth */
    door_motion_t m = door_sm_get_motion();

    switch (m) {
    case DOOR_MOVING_OPEN:     return "OPENING";
    case DOOR_MOVING_CLOSE:    return "CLOSING";
    case DOOR_POSTCLOSE_LOCK:  return "LOCKING";
    case DOOR_IDLE_UNKNOWN:    return "UNKNOWN";
    default:                   return "TRANSITION";
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

static bool door_busy()
{

    bool is_busy = true;

    door_motion_t m = door_sm_get_motion();


    switch (m) {
        case DOOR_IDLE_UNKNOWN:
        case DOOR_IDLE_CLOSED:
        case DOOR_IDLE_OPEN:
        is_busy = false;
        break;

        default:  break;

    }
    return is_busy;
}


/* --------------------------------------------------------------------------
 * Device registration
 * -------------------------------------------------------------------------- */

Device door_device = {
    .name         = "door",
    .deviceID     = DEVICE_ID_DOOR,
    .init         = door_init,
    .get_state    = door_get_state,
    .set_state    = door_set_state,
    .schedule_state = door_schedule_state,
    .state_string = door_state_string,
    .tick         = door_tick,
    .is_busy      = door_busy
};
