/*
 * door_device.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door device declarative events
 *
 * Updated: 2025-12-30
 */

#include "door_device.h"
#include "devices/door_device.h"
#include "rtc.h"
#include "console/mini_printf.h"   // for mini_printf / console_puts



static const char *action_name(Action a)
{
    switch (a) {
    case ACTION_ON:  return "OPEN";
    case ACTION_OFF: return "CLOSE";
    default:         return "UNKNOWN";
    }
}

static void door_reconcile(Action expected)
{
    uint16_t now = rtc_minutes_since_midnight();

    mini_printf(
        "[door] reconcile %s @ %02u:%02u\n",
        action_name(expected),
        now / 60,
        now % 60
    );

    /* no execution yet */
}


const Device door_device = {
    .name       = "door",
    .reconcile  = door_reconcile
};
