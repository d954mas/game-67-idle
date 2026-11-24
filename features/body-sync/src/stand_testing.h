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
/* A stand record (family byte first) reaching the host from guest `seat` now, as the network
   would hand it, stamped with that guest's tick: parsed, then judged at the host's next tick.
   False when it was not taken. */
bool stand_testing_record(body_sync_stand_t *stand, uint8_t seat, const uint8_t *data, uint32_t size);
/* The tick `endpoint` stamps its states and records with now. */
uint32_t stand_testing_tick(body_sync_stand_t *stand, uint32_t endpoint);
/* For the next `ticks` of `endpoint`, the velocity its seam reads of `body` at collect, and so
   declares in its states, is `add` more than the body's own; the body moves as it would. A peer
   lying about its speed. */
void stand_testing_plant_declared_velocity(body_sync_stand_t *stand, uint32_t endpoint, body_sync_id_t body, const float add[3], uint32_t ticks);
/* The host's limits on an impact claim, and on a catch (a grab of another guest's flight), read
   `reach_scale` times the seam's reach and `approach_scale` times its approach (1 and 1
   unplanted): an approach of 0 refuses every honest claim of a throw, an infinite reach takes a
   claim or a catch of a body far from its agent. */
void stand_testing_plant_claim_limits(body_sync_stand_t *stand, float reach_scale, float approach_scale);
/* The host reads a catch's gap `metres` farther than the seam measures it: the catcher's evidence
   off by that much. */
void stand_testing_plant_catch_offset(body_sync_stand_t *stand, float metres);
/* Guest `player`'s grabs claim `hold` and name no holder, though her character carries the body: a
   forged catcher that would keep a caught body a flight. */
void stand_testing_plant_catch_hold(body_sync_stand_t *stand, uint32_t player, body_sync_hold_t hold);

/* Planted defects for the lag-aligned gap: state packets every endpoint takes are stamped
   `ticks` later than they were sent (a copy then shows its owner a tick early for the tick it
   names), and what a guest draws of the bodies it simulates is moved `metres` along x from what it
   simulates and sends (a render offset that never decays on a local body). */
void stand_testing_plant_shift(body_sync_stand_t *st, uint32_t ticks);
/* Guest `endpoint` ignores the host's removals: a body the host broke stays on its world. */
void stand_testing_plant_keep_broken(body_sync_stand_t *st, uint8_t endpoint);
void stand_testing_plant_drift(body_sync_stand_t *st, float metres);
/* What a guest draws of a predicted copy in its pause is moved `metres` along x: a lead on the
   playback tick, as this guest's own answer to its touch draws ahead of it. */
void stand_testing_plant_lead(body_sync_stand_t *st, float metres);
/* At the end of tick `tick`, the first light body guest 1 simulates (`local`) or holds as a copy is
   moved `metres` along x outside the seam: a seam move of a local body, or a copy knocked off its
   rest. */
void stand_testing_plant_jolt(body_sync_stand_t *st, uint32_t tick, float metres, bool local);
/* From the end of tick `tick`, the render offset the local correction reads of the first light
   body guest 1 simulates then carries `metres` more along x and a turn of `radians` about y: the
   seam setting an offset on a body the guest simulates, with no physics move. */
void stand_testing_plant_offset(body_sync_stand_t *st, uint32_t tick, float metres, float radians);
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
/* For `ticks` ticks from `tick`, guest 1 records the first light copy of a host body it holds as
   a prediction with its pull paused, whatever the seam does: a pause held past the layer's chain
   cap. */
void stand_testing_plant_pause(body_sync_stand_t *st, uint32_t tick, uint32_t ticks);
/* For `ticks` ticks from `tick`, the host's awake body of the first light copy guest 1 holds is
   moved `metres` along x after every tick, outside physics: a creep its velocity, the run's rest
   speed, never shows. With `copy`, that copy creeps instead, held kinematic and still between the
   moves as stand_testing_plant_wall holds one: a copy crept off its host body at rest. */
void stand_testing_plant_creep(body_sync_stand_t *st, uint32_t tick, float metres, uint32_t ticks, bool copy);
/* As stand_testing_plant_creep on the host body, but it turns `radians` about its own centre and
   the vertical after every tick instead of moving: a turn its position never shows. */
void stand_testing_plant_turn(body_sync_stand_t *st, uint32_t tick, float radians, uint32_t ticks);

#endif
