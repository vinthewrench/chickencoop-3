/*
 * door_state_machine.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door motion state machine (simplified, safe)
 */

#include "door_state_machine.h"
#include "led_state_machine.h"

#include "door_hw.h"
#include "door_lock.h"
#include "config.h"

/* --------------------------------------------------------------------------
 * Internal state
 * -------------------------------------------------------------------------- */

static door_motion_t g_motion        = DOOR_IDLE_UNKNOWN;
static dev_state_t   g_settled_state = DEV_STATE_UNKNOWN;
static uint32_t      g_motion_t0_ms  = 0;

/* Optional delay before locking (settle time) */
#define POSTCLOSE_DELAY_MS  250u

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

static void update_led(door_motion_t m)
{
    switch (m) {

    case DOOR_IDLE_OPEN:
    case DOOR_IDLE_CLOSED:
        led_state_machine_set(LED_OFF, LED_GREEN);
        break;

    case DOOR_MOVING_OPEN:
        led_state_machine_set(LED_PULSE, LED_GREEN);
        break;

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
    update_led(m);
}

static void door_stop(void)
{
    door_hw_stop();
}

static inline uint16_t door_settle_ms(void)
{
    /* Defensive clamp: settling is mechanical, not infinite */
    uint16_t ms = g_cfg.door_settle_ms;

    if (ms < 250)
        ms = 250;      /* minimum sanity */
    else if (ms > 5000)
        ms = 5000;     /* gravity + chickens, not geology */

    return ms;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void door_sm_init(void)
{
    door_lock_init();
    door_stop();

    g_settled_state = DEV_STATE_UNKNOWN;
    g_motion_t0_ms  = 0;

    set_motion(DOOR_IDLE_UNKNOWN);
}

void door_sm_request(dev_state_t state)
{
    if (state != DEV_STATE_ON && state != DEV_STATE_OFF)
        return;

    /* Abort any active motion immediately */
    door_stop();

    g_motion_t0_ms  = 0;
    g_settled_state = DEV_STATE_UNKNOWN;

    /* ALWAYS unlock first (blocking, safe) */
    door_lock_release();

    if (state == DEV_STATE_ON) {
        /* OPEN */
        door_hw_set_open_dir();
        door_hw_enable();
        set_motion(DOOR_MOVING_OPEN);
    } else {
        /* CLOSE */
        door_hw_set_close_dir();
        door_hw_enable();
        set_motion(DOOR_MOVING_CLOSE);
    }
}

void door_sm_tick(uint32_t now_ms)
{
    switch (g_motion) {

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
            g_motion_t0_ms  = 0;
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
            g_motion_t0_ms = now_ms;
            set_motion(DOOR_POSTCLOSE_LOCK);
        }
        break;

    /* --------------------------------------------------
     * Post-close delay + lock (blocking)
     * -------------------------------------------------- */
     case DOOR_POSTCLOSE_LOCK:
         if ((uint32_t)(now_ms - g_motion_t0_ms) < door_settle_ms())
             break;

         /*
          * Blocking lock pulse:
          * - bounded by lock driver
          * - returns with power OFF
          * - nothing else should happen during this window
          */
         door_lock_engage();

         g_motion_t0_ms  = 0;
         g_settled_state = DEV_STATE_OFF;
         set_motion(DOOR_IDLE_CLOSED);
         break;

    /* --------------------------------------------------
     * Idle / unknown
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
