/*
 * config_host.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Host-side configuration source
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *  - Uses shared defaults, then applies host overrides
 *
 * Updated: 2025-12-29
 */

#include "config.h"



void config_load(struct config *cfg)
{
    /* Start from shared defaults */
    config_defaults(cfg);

 /* fake for testing */
 cfg->door.open_when.ref = REF_SOLAR_STD;
 cfg->door.open_when.offset_minutes = 0;

 cfg->door.close_when.ref = REF_SOLAR_CIV;
 cfg->door.close_when.offset_minutes = 0;
}

/* Host does not persist configuration */
void config_save(const struct config *)
{
    /* no-op */
}
