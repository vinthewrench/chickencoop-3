#pragma once
#include <stdint.h>

/*
 * system_sleep_until()
 *
 * Purpose:
 *  - Enter low-power wait state until the given minute-of-day
 *
 * Contract:
 *  - minute is [0..1439]
 *  - Wake MAY occur earlier due to external events
 *  - Caller is responsible for re-evaluating schedule on wake
 *
 * Platform behavior:
 *  - HOST: prints intent only (no real sleep)
 *  - FIRMWARE: programs RTC alarm and enters sleep
 *
 * Notes:
 *  - No scheduler logic here
 *  - No device logic here
 *  - No busy-waiting
 */
void system_sleep_until(uint16_t minute);


 void system_sleep_init(void);
