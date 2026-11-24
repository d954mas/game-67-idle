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
/* This host gives a contact claim no grace after the agent's ownership ended: the claim of a
   flight its own release ended, sent behind that release, is refused. */
void body_sync_plant_end_grace_zero(body_sync_t *sync);
/* This host keeps a recalled seat's old number standing for its contact claims: a recall (an
   assign while the seat owned the body) drops none of the claims the seat sent before it heard. */
void body_sync_plant_recall_keeps_number(body_sync_t *sync);
/* This host keeps what it received of a body's old owner when it moves the body to another guest
   (a catch): its hold, holder, flags, data and pose read as the new owner's until her states
   come, and a host claim before them goes on from the old owner's flight. */
void body_sync_plant_move_keeps_received(body_sync_t *sync);
/* This host judges an agent's contacts on its ring of 12 states alone: a claim the record lane
   resent, which lands past them, has no evidence. */
void body_sync_plant_agent_ring_off(body_sync_t *sync);

#endif
