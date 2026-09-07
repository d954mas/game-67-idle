#ifndef FEATURES_LEADERBOARD_HTTP_H
#define FEATURES_LEADERBOARD_HTTP_H

#include "features/leaderboard/leaderboard.h"

/* Anonymous HTTP backend for portals without a board of their own: all-time
 * plus UTC-day, locally re-estimated place. Its endpoint, obfuscation key and
 * cadence arrive in its own config struct from the game. */

const leaderboard_backend_t *leaderboard_http_backend(void);

#endif /* FEATURES_LEADERBOARD_HTTP_H */
