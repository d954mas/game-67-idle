#ifndef BODY_SYNC_STAND_TESTING_H
#define BODY_SYNC_STAND_TESTING_H

/* Reach into a running stand for tests that plant a defect and require a check to catch it:
   a check that passes on a planted defect proves nothing. Not for games. */

#include "features/body_sync/body_sync_stand.h"
#include "stand_world.h"

stand_world_t *stand_testing_world(body_sync_stand_t *stand, uint32_t endpoint);
body_sync_t *stand_testing_sync(body_sync_stand_t *stand, uint32_t endpoint);
/* Applies a host effect to `target` as if a claim from 1 m away had been accepted. */
bool stand_testing_effect(body_sync_stand_t *stand, stand_effect_t effect, body_sync_id_t target);
/* Records a guest as left without its session ending. */
void stand_testing_mark_left(body_sync_stand_t *stand, uint8_t seat);

/* Planted defects for the lag-aligned gap: state packets every endpoint takes are stamped
   `ticks` later than they were sent (a copy then shows its owner a tick early for the tick it
   names), and what a guest draws of the bodies it simulates is moved `metres` along x from what it
   simulates and sends (a render offset that never decays on a local body). */
void stand_testing_plant_shift(body_sync_stand_t *st, uint32_t ticks);
void stand_testing_plant_drift(body_sync_stand_t *st, float metres);
/* What a guest draws of a predicted copy in its pause is moved `metres` along x: a lead on the
   playback tick, as this guest's own answer to its touch draws ahead of it. */
void stand_testing_plant_lead(body_sync_stand_t *st, float metres);
/* At the end of tick `tick`, the first light body guest 1 simulates (`local`) or holds as a copy is
   moved `metres` along x outside the seam: a seam move of a local body, or a copy knocked off its
   rest. */
void stand_testing_plant_jolt(body_sync_stand_t *st, uint32_t tick, float metres, bool local);
/* From the next tick, `endpoint`'s copy of the scene body `body` is held kinematic and still, a
   wall a walker cannot push (the reference run of `expect pace` has no plant). Up to 8 bodies,
   one call each, on one endpoint. */
void stand_testing_plant_wall(body_sync_stand_t *st, uint32_t endpoint, const char *body);
/* `expect pace` holds the walk to the others acting on time only, as if the network delayed
   nothing the walker meets. */
void stand_testing_plant_pace_on_time(body_sync_stand_t *st);
/* The copy's jolt `count` times from `tick`, every `every` ticks, back and forth: a held copy
   churned in steps too small for one tick's snap (the driven body's rest window, stand_logic.h). */
void stand_testing_plant_jitter(body_sync_stand_t *st, uint32_t tick, float metres, uint32_t every, uint32_t count);

#endif
