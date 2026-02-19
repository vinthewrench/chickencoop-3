/*
 * foo_device.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Simple ON/OFF relay device
 *
 * Updated: 2026-01-01
 */

#include "device.h"
#include "console/mini_printf.h"

static dev_state_t foo_state = DEV_STATE_UNKNOWN;

static dev_state_t foo_get_state(void)
{
    return foo_state;
}

static void foo_set_state(dev_state_t state)
{
    if (state == foo_state)
        return;

    foo_state = state;

    // if (state == DEV_STATE_ON)
    //     mini_printf("[HOST] foo ON\n");
    // else if (state == DEV_STATE_OFF)
    //     mini_printf("[HOST] foo OFF\n");
}

static const char *foo_state_string(dev_state_t state)
{
    switch (state) {
    case DEV_STATE_ON:  return "ON";
    case DEV_STATE_OFF: return "OFF";
    default:            return "UNKNOWN";
    }
}


static void foo_device_init(void)
{
    static uint8_t init = 0;
    if (init)
        return;

    // foo HW init
    // mini_printf("[HOST] foo INIT\n");
    foo_set_state(DEV_STATE_OFF);
    init = 1;
}



Device foo_device = {
    .name = "foo",
    .deviceID     = DEVICE_ID_FOO,
    .init = foo_device_init,
    .get_state = foo_get_state,
    .set_state = foo_set_state,
    .schedule_state  = NULL,
    .state_string = foo_state_string,
    .tick = NULL,
    .is_busy  = NULL
};
