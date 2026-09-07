#ifndef FEATURES_LEADERBOARD_PORTAL_H
#define FEATURES_LEADERBOARD_PORTAL_H

#include "features/leaderboard/leaderboard.h"

/* Portal backend: calls the platform-sdk leaderboard entry points and answers
 * capabilities live, because a host platform may decide them at run time. It
 * contains no JavaScript; portal JS belongs to the platform-sdk pack.
 *
 * A board reaches the portal by `leaderboard_board_def()->portal_id`; a board
 * without one has no capability here. Portals serve one all-time board, so the
 * day scope is never offered. The backend takes no userdata. */

const leaderboard_backend_t *leaderboard_portal_backend(void);

#endif /* FEATURES_LEADERBOARD_PORTAL_H */
