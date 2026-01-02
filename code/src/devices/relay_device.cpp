/*
 * foo_device.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Simple ON/OFF relay device
 *
 * Updated: 2026-01-01
 */

#include "device.h"
#include "relay.h"

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

Device relay1_device = {
    .name = "relay1",
    .get_state = relay1_get_state,
    .set_state = relay1_set_state,
    .state_string = relay_state_string
};

Device relay2_device = {
    .name = "relay2",
    .get_state = relay2_get_state,
    .set_state = relay2_set_state,
    .state_string = relay_state_string
};
