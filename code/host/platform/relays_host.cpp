/*
 * relay_host.cpp
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

// firmware/platform/relay_host.cpp
#include "relay_hw.h"
#include "console/mini_printf.h"

void relay_init(void){
    /* nop */
    mini_printf("[HOST] relay INIT()\n");

}

void relay1_set()
{
    mini_printf("[HOST] relay1_set()\n");
}

void relay1_reset(void)
{
    mini_printf("[HOST] relay1_reset()\n");   // TODO: replace with real hardware logic
 }

void relay2_set()
{
    mini_printf("[HOST] relay2_set()\n");
 }

void relay2_reset(void)
{
    mini_printf("[HOST] relay2_reset()\n");
}
