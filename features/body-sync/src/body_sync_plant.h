#ifndef BODY_SYNC_PLANT_H
#define BODY_SYNC_PLANT_H

/* Planted defects of the real seam, for tests that require a check to catch them: a check that
   passes on a planted defect proves nothing. Not for games. */

#include "features/body_sync/body_sync.h"

/* Every take's evidence on this host reads an unbounded reach: a take of any body is granted. */
void body_sync_plant_take_reach_unbounded(body_sync_t *sync);
/* This host's evidence of contacts and takes grows over the gaps in what it took: a chord of
   any spacing on the sender's span, the last state's ray and bend over its age, a resting state
   standing for the whole ownership. */
void body_sync_plant_evidence_grows_over_gaps(body_sync_t *sync);

#endif
