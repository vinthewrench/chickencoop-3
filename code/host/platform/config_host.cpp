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
 * Updated: 2025-12-30
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define HOST_CFG_FILE "coop.cfg"


bool config_load(struct config *cfg)
{
    FILE *f = fopen(HOST_CFG_FILE, "rb");
    if (!f) {
        /* No saved config: start from defaults */
        config_defaults(cfg);
        return false;
    }

    size_t n = fread(cfg, sizeof(*cfg), 1, f);
    fclose(f);

    if (n != 1) {
        /* Corrupt or short read */
        config_defaults(cfg);
        return false;
    }

    /* Verify checksum */
    uint16_t stored = cfg->checksum;
    uint16_t computed = config_fletcher16(
        cfg,
        offsetof(struct config, checksum)
    );

    if (stored != computed) {
        /* Corrupt config */
        config_defaults(cfg);
        return false;
    }

    return true;
}


/* Host does not persist configuration automatically */
void config_save(const struct config *cfg)
{
    FILE *f = fopen(HOST_CFG_FILE, "wb");
    if (!f)
        return;

    struct config tmp = *cfg;

    /* Compute checksum over everything except itself */
    tmp.checksum = 0;
    tmp.checksum = config_fletcher16(
        &tmp,
        offsetof(struct config, checksum)
    );

    fwrite(&tmp, sizeof(tmp), 1, f);
    fclose(f);
}
