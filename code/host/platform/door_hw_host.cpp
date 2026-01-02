/*
 * door_hw_host.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door actuator hardware simulation (host)
 *
 * Notes:
 *  - Host-only implementation
 *  - Mirrors door_hw API used on AVR
 *  - Logs direction and enable state
 *  - No timing, no state machine
 *
 * Updated: 2026-01-02
 */

#include "door_hw.h"

#include <stdio.h>

static const char *g_dir = "NEUTRAL";
static int g_en = 0;

void door_hw_init(void)
{
    g_dir = "NEUTRAL";
    g_en = 0;
    printf("[DOOR_HW] init (INA/INB/EN)\n");
}

void door_hw_set_open_dir(void)
{
    g_dir = "OPEN";
    printf("[DOOR_HW] dir=OPEN (INA=1 INB=0)\n");
}

void door_hw_set_close_dir(void)
{
    g_dir = "CLOSE";
    printf("[DOOR_HW] dir=CLOSE (INA=0 INB=1)\n");
}

void door_hw_enable(void)
{
    g_en = 1;
    printf("[DOOR_HW] EN=1 dir=%s\n", g_dir);
}

void door_hw_disable(void)
{
    g_en = 0;
    printf("[DOOR_HW] EN=0\n");
}

void door_hw_stop(void)
{
    g_en = 0;
    g_dir = "NEUTRAL";
    printf("[DOOR_HW] stop (EN=0 INA=0 INB=0)\n");
}
