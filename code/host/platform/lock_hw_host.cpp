/*
 * lock_hw_host.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Lock actuator hardware simulation (host)
 *
 * Notes:
 *  - Host-only implementation
 *  - Mirrors lock_hw API used on AVR
 *  - Logs direction and enable state
 *  - No timing, no state machine
 *
 * Updated: 2026-01-02
 */

#include "lock_hw.h"

#include <stdio.h>

static const char *g_dir = "NEUTRAL";
static int g_en = 0;

void lock_hw_init(void)
{
    g_dir = "NEUTRAL";
    g_en = 0;
    printf("[LOCK_HW] init (INA/INB/EN)\n");
}

void lock_hw_set_lock_dir(void)
{
    g_dir = "LOCK";
    printf("[LOCK_HW] dir=LOCK (INA=1 INB=0)\n");
}

void lock_hw_set_unlock_dir(void)
{
    g_dir = "UNLOCK";
    printf("[LOCK_HW] dir=UNLOCK (INA=0 INB=1)\n");
}

void lock_hw_enable(void)
{
    g_en = 1;
    printf("[LOCK_HW] EN=1 dir=%s\n", g_dir);
}

void lock_hw_disable(void)
{
    g_en = 0;
    printf("[LOCK_HW] EN=0\n");
}

void lock_hw_stop(void)
{
    g_en = 0;
    g_dir = "NEUTRAL";
    printf("[LOCK_HW] stop (EN=0 INA=0 INB=0)\n");
}
