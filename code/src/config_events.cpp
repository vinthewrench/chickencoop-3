/*
 * config_events.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Event configuration storage
 *
 * Updated: 2025-12-30
 */

 #include "config_events.h"
 #include "config.h"

 /*
  * Return pointer to config-backed event table.
  * Count is number of non-zero refnum entries.
  */
 const Event *config_events_get(size_t *count)
 {
     size_t n = 0;

     for (size_t i = 0; i < MAX_EVENTS; i++) {
         if (g_cfg.events[i].refnum != 0)
             n++;
     }

     if (count)
         *count = n;

     return g_cfg.events;
 }

 /*
  * Add event to first free slot (refnum == 0).
  * Assign a refnum.
  */
 bool config_events_add(const Event *ev)
 {
     if (!ev)
         return false;

     for (size_t i = 0; i < MAX_EVENTS; i++) {
         if (g_cfg.events[i].refnum == 0) {
             g_cfg.events[i] = *ev;
             g_cfg.events[i].refnum = (uint8_t)(i + 1);  /* non-zero */
             return true;
         }
     }

     return false;
 }

 /*
  * Update event at logical index (nth used slot).
  */
 bool config_events_update(uint8_t index, const Event *ev)
 {
     if (!ev)
         return false;

     size_t n = 0;

     for (size_t i = 0; i < MAX_EVENTS; i++) {
         if (g_cfg.events[i].refnum == 0)
             continue;

         if (n == index) {
             uint8_t ref = g_cfg.events[i].refnum;
             g_cfg.events[i] = *ev;
             g_cfg.events[i].refnum = ref;  /* preserve identity */
             return true;
         }

         n++;
     }

     return false;
 }

 /*
  * Delete event at logical index.
  */
 bool config_events_delete(uint8_t index)
 {
     size_t n = 0;

     for (size_t i = 0; i < MAX_EVENTS; i++) {
         if (g_cfg.events[i].refnum == 0)
             continue;

         if (n == index) {
             g_cfg.events[i].refnum = 0;  /* mark unused */
             return true;
         }

         n++;
     }

     return false;
 }

 /*
  * Clear all events.
  */
 void config_events_clear(void)
 {
     for (size_t i = 0; i < MAX_EVENTS; i++)
         g_cfg.events[i].refnum = 0;
 }
