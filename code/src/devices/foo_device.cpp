/*
 * foo_device.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Placeholder device implementation
 *
 * Notes:
 *  - Emits no events
 *  - Used to validate device registry scaling
 *
 * Updated: 2025-12-30
 */

#include "foo_device.h"

static void foo_reconcile(Action expected)
{
    (void)expected;
}

const Device foo_device = {
    .name       = "foo",
     .reconcile  = foo_reconcile
};
