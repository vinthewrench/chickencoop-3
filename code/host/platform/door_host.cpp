/*
 * door_lock_host.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Host door platform stub
 *
 * Updated: 2026-01-01
 */

#include "door_hw.h"
#include "console/mini_printf.h"

void door_hw_init(void)
{
    mini_printf("[HOST] door_hw_init()\n");
}

void door_hw_set_open_dir(void)
{
    mini_printf("[HOST] door_hw_set_open_dir()\n");
}

void door_hw_set_close_dir(void)
{
    mini_printf("[HOST] door_hw_set_close_dir()\n");
}

void door_hw_enable(void)
{
    mini_printf("[HOST] door_hw_enable()\n");
}

void door_hw_stop(void)
{
    mini_printf("[HOST] door_hw_stop()\n");
}
