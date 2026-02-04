/*
 * lock_host.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Source file
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *
 * Updated: 2025-12-29
 */

// host/platform/lock_host.cpp
#include "door_lock.h"
#include "console/mini_printf.h"


void door_lock_init(void){
    // NOP
}

void door_lock_stop(void) {
    mini_printf("[HOST] door_lock_stop()\n");

}

void door_lock_engage(void)
{
    mini_printf("[HOST] door_lock_engage()\n");
}

void door_lock_release(void)
{
    mini_printf("[HOST] door_lock_release()\n");
}
