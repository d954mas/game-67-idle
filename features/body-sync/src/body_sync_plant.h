#ifndef BODY_SYNC_PLANT_H
#define BODY_SYNC_PLANT_H

/* Planted defects of the real seam, for tests that require a check to catch them: a check that
   passes on a planted defect proves nothing. Not for games. */

#include "features/body_sync/body_sync.h"

/* Every take's evidence on this host reads an unbounded reach: a take of any body is granted. */
void body_sync_plant_take_reach_unbounded(body_sync_t *sync);
/* This host's evidence of contacts and takes grows over the gaps in what it took: a state's ray
   and bend over any ticks to the contact, a chord of any spacing on the sender's span, a silent
   seat's states standing for the whole ownership. */
void body_sync_plant_evidence_grows_over_gaps(body_sync_t *sync);
/* This host reads a contact's approach as the fastest step of the agent's whole ring of states,
   not the steps at the contact. */
void body_sync_plant_approach_from_ring(body_sync_t *sync);
/* This host's evidence stands however long ago it took the states: a claim may name any tick
   the ring still holds. */
void body_sync_plant_evidence_age_unbounded(body_sync_t *sync);
/* This side's bodies rest whenever Box3D keeps them asleep, a body moved by transform included:
   a carried body goes out no more once its rest pose is acknowledged. */
void body_sync_plant_transform_sleep(body_sync_t *sync);

#endif
