/*
 * uptime_host.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Host uptime implementation
 *
 * Notes:
 *  - Offline system
 *  - Deterministic enough for host simulation
 *  - No AVR dependencies
 *
 * Updated: 2026-01-05
 */

#include "uptime.h"

#include <time.h>
#include <stdint.h>

static struct timespec g_start;

void uptime_init(void)
{
    clock_gettime(CLOCK_MONOTONIC, &g_start);
}

uint32_t uptime_millis(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    uint64_t sec  = (uint64_t)(now.tv_sec  - g_start.tv_sec);
    uint64_t nsec = (uint64_t)(now.tv_nsec - g_start.tv_nsec);

    if ((int64_t)nsec < 0) {
        sec--;
        nsec += 1000000000ULL;
    }

    return (uint32_t)(sec * 1000ULL + nsec / 1000000ULL);
}

uint32_t uptime_seconds(void)
{
    return uptime_millis() / 1000;
}
