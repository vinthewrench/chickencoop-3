#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Scheduler reducer output.
 * Pure facts. No side effects.
 */
typedef struct {
    bool     has_next;
    uint16_t next_minute;   /* minutes since midnight, 0–1439 */
} scheduler_result_t;

/*
 * Run one scheduler pass using current RTC time and rules.
 * Does NOT execute events.
 * Does NOT program alarms.
 */
void scheduler_run(scheduler_result_t *out);
