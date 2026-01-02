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
#include "lock.h"
#include "console/mini_printf.h"


void lock_engage(void)
{
    mini_printf("[HOST] lock_engage()\n");
}

void lock_release(void)
{
    mini_printf("[HOST] lock_release()\n");
}
