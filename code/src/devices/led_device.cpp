/*
 *led_device.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Simple ON/OFF relay device
 *
 * Updated: 2026-01-01
 */

#include "device.h"
#include "console/mini_printf.h"
#include "led_state_machine.h"



static dev_state_t led_get_state(void)
{
    return led_state_machine_is_on()? DEV_STATE_ON: DEV_STATE_OFF;
}


static const char *led_state_string(dev_state_t state)
{
    switch (state) {
    case DEV_STATE_ON:  return "ON";
    case DEV_STATE_OFF: return "OFF";
    default:            return "UNKNOWN";
    }
}

static void led_init(void)
{
    led_state_machine_init();
}


static void led_tick(uint32_t now_ms)
{
    led_state_machine_tick(now_ms);
}


Device led_device = {
    .name       = "led",
    .init      = led_init,
    .get_state = led_get_state,
    .set_state  = NULL,
    .state_string = led_state_string,
    .tick         =  led_tick

};
