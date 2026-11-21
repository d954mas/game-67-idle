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

#endif
