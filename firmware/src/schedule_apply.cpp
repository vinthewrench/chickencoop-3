#include "schedule_apply.h"

#include "devices/devices.h"

#include "schedule_apply.h"
#include "devices/devices.h"
#include "console/mini_printf.h"
#include "console/console.h"

/*
 * Apply reduced scheduler state to devices.
 *
 * This is the ONLY place where scheduled intent
 * actually turns into device actions.
 */
 void schedule_apply(const struct reduced_state *rs)
 {
     if (!rs)
         return;

     uint8_t id;

     for (bool ok = device_enum_first(&id);
          ok;
          ok = device_enum_next(id, &id)) {

         if (!rs->has_action[id])
             continue;

         dev_state_t want =
             (rs->action[id] == ACTION_ON) ? DEV_STATE_ON
                                           : DEV_STATE_OFF;

         dev_state_t have;
         if (!device_get_state_by_id(id, &have))
             continue;

         /* No-op if already correct */
         if (have == want)
             continue;


         uint32_t when = rs->when[id];

#if 0    /* ---- DEBUG: print scheduled action ---- */

         const char *name = "?";
         device_name(id, &name);

         mini_printf("\tDEBUG SCHED: %s -> %s (when=%lu)\n",
                     name,
                     (want == DEV_STATE_ON) ? "ON" : "OFF",
                     when);

#endif

         /* ---- Apply action ---- */

         device_schedule_state_by_id(id, want,when);
     }
 }
