/*
 * door_host.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Host door platform stub
 *
 * Updated: 2026-01-01
 */

#include "door.h"
#include "console/mini_printf.h"

static bool hw_open = false;

void door_open(void)
{
    mini_printf("[HOST] door_open()\n");
    hw_open = true;
}

void door_close(void)
{
    mini_printf("[HOST] door_close()\n");
    hw_open = false;
}

bool door_is_open(void)
{
    return hw_open;
}
