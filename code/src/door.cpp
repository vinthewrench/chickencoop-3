/*
 * door.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door stub (transitional)
 *
 * Notes:
 *  - TEMPORARY glue
 *  - No timing, no scheduling
 *  - State is assumed, not sensed
 *  - Will be replaced by door_control logic
 *
 * Updated: 2026-01-02
 */

#include "door.h"
#include "door_hw.h"

/*
 * This is NOT physical truth.
 * It only reflects the last requested intent.
 */
static bool g_door_assumed_open = false;

void door_open(void)
{
    /* Open door (extend actuator) */
    door_hw_set_open_dir();
    door_hw_enable();

    g_door_assumed_open = true;
}

void door_close(void)
{
    /* Close door (retract actuator) */
    door_hw_set_close_dir();
    door_hw_enable();

    g_door_assumed_open = false;
}

bool door_is_open(void)
{
    /*
     * Conservative assumption based on last request.
     * Real implementation will query control state.
     */
    return g_door_assumed_open;
}
