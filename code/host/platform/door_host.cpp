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

void door_open(void)
{
    mini_printf("[HOST] door_open()\n");
 }

void door_close(void)
{
    mini_printf("[HOST] door_close()\n");
}
