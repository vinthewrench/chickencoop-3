/*
 * foo_device.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Simple ON/OFF relay device
 *
 * Updated: 2026-01-01
 */

#include "device.h"
#include "relay_hw.h"

static dev_state_t relay1_state = DEV_STATE_UNKNOWN;
static dev_state_t relay2_state = DEV_STATE_UNKNOWN;


static dev_state_t relay1_get_state(void)
{
    return relay1_state;
}

static void relay1_set_state(dev_state_t state)
{
    if (state == relay1_state)
        return;

    relay1_state = state;

    if (state == DEV_STATE_ON)
        relay1_set();
    else if (state == DEV_STATE_OFF)
        relay1_reset();
}

static dev_state_t relay2_get_state(void)
{
    return relay2_state;
}

static void relay2_set_state(dev_state_t state)
{
    if (state == relay2_state)
        return;

    relay2_state = state;

    if (state == DEV_STATE_ON)
        relay2_set();
    else if (state == DEV_STATE_OFF)
        relay2_reset();
}

static const char *relay_state_string(dev_state_t state)
{
    switch (state) {
    case DEV_STATE_ON:  return "ON";
    case DEV_STATE_OFF: return "OFF";
    default:            return "UNKNOWN";
    }
}


static void relay_device_init(void)
{
    static uint8_t init = 0;
    if (init)
        return;

    relay_init();
    relay1_set_state(DEV_STATE_OFF);
    relay2_set_state(DEV_STATE_OFF);
    init = 1;
}

Device relay1_device = {
    .name = "relay1",
    .init = relay_device_init,
    .get_state = relay1_get_state,
    .set_state = relay1_set_state,
    .state_string = relay_state_string,
    .tick = NULL

};

Device relay2_device = {
    .name = "relay2",
    .init = relay_device_init,
    .get_state = relay2_get_state,
    .set_state = relay2_set_state,
    .state_string = relay_state_string,
    .tick = NULL

};
