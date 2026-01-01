/*
 * door_avr.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: AVR door platform stub
 *
 * Updated: 2026-01-01
 */

#include "door.h"

static bool hw_open = false;

void door_open(void)
{
    hw_open = true;
}

void door_close(void)
{
    hw_open = false;
}

bool door_is_open(void)
{
    return hw_open;
}
