/*
 * door_state_machine.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door motion state machine (internal)
 */

#include "door_state_machine.h"
#include "lock_state_machine.h"
#include "led_state_machine.h"

#include "door_hw.h"
#include "config.h"

/* --------------------------------------------------------------------------
 * Internal state
 * -------------------------------------------------------------------------- */

static door_motion_t g_motion = DOOR_IDLE_UNKNOWN;
static dev_state_t   g_settled_state = DEV_STATE_UNKNOWN;
static uint32_t      g_motion_t0_ms = 0;

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

static void update_led_for_motion(door_motion_t m)
{
    switch (m) {

    case DOOR_IDLE_OPEN:
    case DOOR_IDLE_CLOSED:
        led_state_machine_set(LED_OFF, LED_GREEN); /* color ignored */
        break;

    case DOOR_PREOPEN_UNLOCK:
    case DOOR_MOVING_OPEN:
        led_state_machine_set(LED_PULSE, LED_GREEN);
        break;

    case DOOR_PRECLOSE_UNLOCK:
    case DOOR_MOVING_CLOSE:
        led_state_machine_set(LED_PULSE, LED_RED);
        break;

    case DOOR_POSTCLOSE_LOCK:
        led_state_machine_set(LED_ON, LED_RED);
        break;

    case DOOR_IDLE_UNKNOWN:
    default:
        led_state_machine_set(LED_BLINK, LED_RED);
        break;
    }
}

static void set_motion(door_motion_t m)
{
    if (g_motion == m)
        return;

    g_motion = m;
    update_led_for_motion(m);
}

static void door_stop(void)
{
    door_hw_stop();
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void door_sm_init(void)
{
    door_stop();

    g_settled_state = DEV_STATE_UNKNOWN;
    g_motion_t0_ms = 0;

    set_motion(DOOR_IDLE_UNKNOWN);
}

void door_sm_request(dev_state_t state)
{
    if (state != DEV_STATE_ON && state != DEV_STATE_OFF)
        return;

    /* Abort any active motion immediately */
    switch (g_motion) {
    case DOOR_MOVING_OPEN:
    case DOOR_MOVING_CLOSE:
        door_stop();
        set_motion(DOOR_IDLE_UNKNOWN);
        break;
    default:
        break;
    }

    g_motion_t0_ms = 0;
    g_settled_state = DEV_STATE_UNKNOWN;

    if (state == DEV_STATE_ON) {
        /* OPEN request */
        lock_sm_release();
        set_motion(DOOR_PREOPEN_UNLOCK);
    } else {
        /* CLOSE request */
        lock_sm_release();
        set_motion(DOOR_PRECLOSE_UNLOCK);
    }
}

void door_sm_tick(uint32_t now_ms)
{
    switch (g_motion) {

    /* --------------------------------------------------
     * Waiting for unlock before opening
     * -------------------------------------------------- */
    case DOOR_PREOPEN_UNLOCK:
        if (!lock_sm_busy()) {
            door_hw_set_open_dir();
            door_hw_enable();

            g_motion_t0_ms = 0;
            set_motion(DOOR_MOVING_OPEN);
        }
        break;

    /* --------------------------------------------------
     * Waiting for unlock before closing
     * -------------------------------------------------- */
    case DOOR_PRECLOSE_UNLOCK:
        if (!lock_sm_busy()) {
            door_hw_set_close_dir();
            door_hw_enable();

            g_motion_t0_ms = 0;
            set_motion(DOOR_MOVING_CLOSE);
        }
        break;

    /* --------------------------------------------------
     * Door moving open
     * -------------------------------------------------- */
    case DOOR_MOVING_OPEN:
        if (g_motion_t0_ms == 0) {
            g_motion_t0_ms = now_ms;
            break;
        }

        if ((uint32_t)(now_ms - g_motion_t0_ms) >= g_cfg.door_travel_ms) {
            door_stop();
            g_motion_t0_ms = 0;
            g_settled_state = DEV_STATE_ON;
            set_motion(DOOR_IDLE_OPEN);
        }
        break;

    /* --------------------------------------------------
     * Door moving closed
     * -------------------------------------------------- */
    case DOOR_MOVING_CLOSE:
        if (g_motion_t0_ms == 0) {
            g_motion_t0_ms = now_ms;
            break;
        }

        if ((uint32_t)(now_ms - g_motion_t0_ms) >= g_cfg.door_travel_ms) {
            door_stop();
            lock_sm_engage();

            g_motion_t0_ms = 0;
            set_motion(DOOR_POSTCLOSE_LOCK);
        }
        break;

    /* --------------------------------------------------
     * Locking after close
     * -------------------------------------------------- */
    case DOOR_POSTCLOSE_LOCK:
        if (!lock_sm_busy()) {
            g_settled_state = DEV_STATE_OFF;
            set_motion(DOOR_IDLE_CLOSED);
        }
        break;

    /* --------------------------------------------------
     * Idle / unknown states
     * -------------------------------------------------- */
    case DOOR_IDLE_OPEN:
    case DOOR_IDLE_CLOSED:
    case DOOR_IDLE_UNKNOWN:
    default:
        break;
    }
}

dev_state_t door_sm_get_state(void)
{
    return g_settled_state;
}

door_motion_t door_sm_get_motion(void)
{
    return g_motion;
}
