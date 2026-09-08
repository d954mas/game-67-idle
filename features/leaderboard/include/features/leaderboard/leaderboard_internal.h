#ifndef FEATURES_LEADERBOARD_INTERNAL_H
#define FEATURES_LEADERBOARD_INTERNAL_H

#include "features/leaderboard/leaderboard.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Pure pieces shared by the backends, split out so core tests cover the
 * silent-failure logic (day math, wire encoding, response model, place
 * estimation) without a network or the live game state. */

#define LB_TOP_MAX (LEADERBOARD_TOP_MAX + 1) /* +1 slot for inserting yourself */
#define LB_INTERP_MAX 16
#define LB_USER_ID_MAX LEADERBOARD_PLAYER_ID_MAX
#define LB_DAY_STR_MAX 9

typedef struct {
    char user_id[LB_USER_ID_MAX];
    /* Every row field the server sends besides user_id and value, packed with
     * the pack's k=v codec: the only identity a row carries, there are no names. */
    char extra[LEADERBOARD_EXTRA_MAX];
    uint32_t value;
} lb_top_entry_t;

typedef struct {
    uint32_t value;
    /* Number of better scores under the board's sort; thresholds ascend by value. */
    uint32_t count;
} lb_interp_entry_t;

typedef struct {
    lb_top_entry_t top[LB_TOP_MAX];
    int top_count;
    lb_interp_entry_t interp[LB_INTERP_MAX];
    int interp_count;
    int user_place;
} lb_board_t;

typedef struct {
    lb_board_t all;
    lb_board_t day;
    char day_str[LB_DAY_STR_MAX];
    bool valid;
} lb_response_t;

/* UTC calendar day as "YYYYMMDD" (proleptic Gregorian from Unix time). */
void lb_utc_day(time_t now, char out[LB_DAY_STR_MAX]);

/* "YYYYMMDD" -> days since the Unix epoch; -1 on a malformed string. Used to
 * turn the server/local day difference into a clock offset. */
int64_t lb_day_number(const char *day);

/* JSON -> xor -> base64url (no padding). Returns length or 0 when out_cap is
 * too small. */
uint32_t lb_encode_payload(const char *json, const char *key, char *out, uint32_t out_cap);

/* Parses the server JSON into the fixed-size model. `root_key` names the
 * board's object in the response; false on any shape it does not recognize
 * (the caller treats that as a failed request). */
bool lb_parse_response(const char *json, uint32_t len, const char *root_key, lb_response_t *out);

/* Re-ranks the player inside a board copy: replaces/inserts the player's
 * entry (carrying the given extra payload), sorts, and falls back to bucket
 * interpolation past the visible top. Returns the 1-based place. */
int lb_recalc_place(lb_board_t *board, const char *user_id, const char *extra, uint32_t value,
                    leaderboard_sort_t sort);

/* Seconds left in the UTC day that contains `now`; 86400 at a day's first second. */
int64_t lb_seconds_until_day_end(time_t now);

#endif /* FEATURES_LEADERBOARD_INTERNAL_H */
