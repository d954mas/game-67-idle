#ifndef BODY_SYNC_SYNC_POLICY_H
#define BODY_SYNC_SYNC_POLICY_H

/* The authority policy: when a body this side simulates goes back to the host. Pure C, asked by
   sync_authority once a step for every local body; nothing else decides a handover the caller
   did not ask for, so another policy (authority following the interaction, for one) replaces
   this pair of files and leaves the shell and the rest of authority as they are.

   This one: the thrower keeps what it threw or dropped (a hold until settled) through its flight,
   so the host replays the flight from the release (sync_replay.h); the flight ends at its first
   touch of a body that is not static (walls and floors are the same in every world, so a bounce
   off them is still the same flight) other than the character that let it go, or when the body
   falls asleep, and the body goes back to the host, which simulated the same flight and now
   judges what it hit. Any other hold stays until the caller releases it. */

#include <stdbool.h>
#include <stdint.h>

typedef struct sync_policy_in {
    uint8_t hold;       /* body_sync_hold_t of the local body */
    bool asleep;        /* after the step */
    bool touched_body;  /* began touching a body that is neither static nor its last holder */
} sync_policy_in_t;

/* True when the body goes back to the host now (on the host: its hold ends). */
bool sync_policy_returns(const sync_policy_in_t *in);

#endif
