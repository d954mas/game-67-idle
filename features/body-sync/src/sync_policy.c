#include "sync_policy.h"

#include <math.h>

#include "features/body_sync/body_sync_types.h"
#include "sync_playback.h"
#include "sync_soft.h"

/* An agent that takes is one that marks: one speed for both rules. */
_Static_assert(SYNC_POLICY_AGENT_MM_S == SYNC_SOFT_TOUCH_MM_S, "the take speed is the second layer's touch speed");
/* The host's hold outlasts the marking walk's reach of an agent leaving at the touch speed: its
   travel in SYNC_SOFT_REACH_TICKS steps plus SYNC_SOFT_REACH_MM, covered at that speed, in 60 Hz
   ticks (sync_policy.h). */
_Static_assert(SYNC_POLICY_HOST_CLEAR_TICKS * SYNC_SOFT_TOUCH_MM_S >= SYNC_SOFT_REACH_TICKS * SYNC_SOFT_TOUCH_MM_S + SYNC_SOFT_REACH_MM * 60,
               "the host's hold is shorter than the marking walk's reach");

static bool agent_moves(float speed, bool pushed) { return speed > SYNC_POLICY_AGENT_SPEED || pushed; }

sync_flight_t sync_policy_flight(const sync_policy_flight_in_t *in) {
    if (in->asleep || (in->until_rest && in->age >= SYNC_POLICY_AGE_TICKS)) {
        return SYNC_FLIGHT_RETURNS;
    }
    if (!in->touched_body) {
        return SYNC_FLIGHT_ON;
    }
    /* Under interaction a flight that meets a body its thrower may take goes on as that body's
       owner: the thrower's result is what everybody sees. A body it may not take is judged by the
       host, as under the host policy. */
    if (in->mode == SYNC_POLICY_INTERACTION && !in->touched_untakable) {
        return SYNC_FLIGHT_INTERACTS;
    }
    return in->until_rest ? SYNC_FLIGHT_ON : SYNC_FLIGHT_RETURNS;
}

bool sync_policy_takes(const sync_policy_take_in_t *in) {
    if (in->mode != SYNC_POLICY_INTERACTION || !in->light || in->held || in->owner_kinematic || !agent_moves(in->agent_speed, in->pushed)) {
        return false;
    }
    /* Only a free body of the host's: one the host's own agents set going (a hold until rest
       there), threw or carry is the host's, and another guest's is that guest's. */
    if (in->owner_seat != BODY_SYNC_SEAT_HOST || in->owner_hold != BODY_SYNC_HOLD_UNTIL_RELEASE) {
        return false;
    }
    /* Given back quiet under this side's own press, it stays a copy until the press ends: the press
       on its copy would take it again, it would quiet and go back, over and over. Kept instead, it
       would go back only by the age bound, long after its heap rested, and wake it. */
    if (in->returned_pressed) {
        return false;
    }
    if (in->host) {
        return in->local;
    }
    return !in->local && in->owned < SYNC_POLICY_OWNED_MAX && in->taken < SYNC_POLICY_TAKES_PER_TICK;
}

bool sync_policy_grants(const sync_policy_grant_in_t *in) {
    if (in->mode != SYNC_POLICY_INTERACTION || !in->light || in->held || in->kinematic || in->claimant == BODY_SYNC_SEAT_HOST) {
        return false;
    }
    /* Already the claimant's by interaction: a duplicate, answered without a move. A body the
       caller granted under another hold stays under it: taken, it would leave the caller's hold
       and the bound of 64. */
    if (in->seat == in->claimant) {
        return in->hold == BODY_SYNC_HOLD_UNTIL_REST;
    }
    /* A take claims a body an agent touched: the host's own world must show one of the
       claimant's agents there, or a guest could take any body it names. */
    return in->seat == BODY_SYNC_SEAT_HOST && in->hold == BODY_SYNC_HOLD_UNTIL_RELEASE && in->owned < SYNC_POLICY_OWNED_MAX && in->budget >= 1.0f &&
           in->agent_gap <= in->agent_reach;
}

float sync_policy_take_reach(float agent_speed, float body_speed, float age_s) {
    return SYNC_POLICY_TAKE_SLOP_M + (agent_speed + body_speed) * age_s;
}

float sync_policy_evidence_ticks(float round_trip_ticks, float delay_ticks, uint32_t tick_hz) {
    float ticks = round_trip_ticks + delay_ticks + 1.0f, cap = SYNC_POLICY_EVIDENCE_AGE_MAX_S * (float)tick_hz;
    return ticks < cap ? ticks : cap;
}

uint32_t sync_policy_rebind_ticks(uint32_t tick_hz) {
    return (uint32_t)ceilf(SYNC_POLICY_EVIDENCE_AGE_MAX_S * (float)tick_hz) + SYNC_DELAY_MAX;
}

uint32_t sync_policy_agent_stride(uint32_t tick_hz, uint32_t poses) {
    float span = ceilf(SYNC_POLICY_EVIDENCE_AGE_MAX_S * (float)tick_hz) + (float)SYNC_POLICY_CHORD_TICKS_MAX;
    uint32_t stride = poses > 0 ? (uint32_t)ceilf(span / (float)poses) : 1u;
    return stride > 0 ? stride : 1u;
}

float sync_policy_box_gap(const float lo[3], const float hi[3], const float other_lo[3], const float other_hi[3]) {
    float d2 = 0.0f;
    for (int i = 0; i < 3; i++) {
        if (!isfinite(lo[i]) || !isfinite(hi[i]) || !isfinite(other_lo[i]) || !isfinite(other_hi[i])) {
            return INFINITY;
        }
        float d = other_lo[i] - hi[i] > lo[i] - other_hi[i] ? other_lo[i] - hi[i] : lo[i] - other_hi[i];
        d2 += d > 0.0f ? d * d : 0.0f;
    }
    return sqrtf(d2);
}

bool sync_policy_host_reclaims(const sync_policy_reclaim_in_t *in) {
    return in->mode == SYNC_POLICY_INTERACTION && in->priority == SYNC_POLICY_HOST_FIRST && in->owner_seat != BODY_SYNC_SEAT_HOST &&
           in->owner_hold == BODY_SYNC_HOLD_UNTIL_REST && agent_moves(in->agent_speed, in->pushed);
}

bool sync_policy_host_takes_contested(const sync_policy_contest_in_t *in) {
    /* Where two guests' bodies touch, each would lean on a copy of the other's that is a round
       trip through the host late, and the heap would creep for ever: the host takes both, as its
       own agents' touch does, until they rest. */
    return in->mode == SYNC_POLICY_INTERACTION && in->seat != BODY_SYNC_SEAT_HOST && in->other_seat != BODY_SYNC_SEAT_HOST &&
           in->seat != in->other_seat && in->hold == BODY_SYNC_HOLD_UNTIL_REST && in->other_hold == BODY_SYNC_HOLD_UNTIL_REST;
}

bool sync_policy_returns_at_rest(const sync_policy_rest_in_t *in) {
    if (!in->host && in->released >= SYNC_POLICY_RELEASES_PER_TICK) {
        return false;
    }
    if (in->age >= SYNC_POLICY_AGE_TICKS || in->out_of_box) {
        return true;
    }
    if (in->host && in->age < sync_policy_host_hold_ticks(in->round_trip_ticks)) {
        return false;
    }
    /* A guest waits for the host to hold the very rest pose it continues from: the release then
       moves nothing on either side. */
    return in->quiet && in->neighbours_quiet && (in->host || in->rest_acked);
}

uint32_t sync_policy_host_hold_ticks(float round_trip_ticks) {
    /* A hold past the age bound changes nothing (the age returns the body first): a longer round
       trip holds to that bound, never shorter. A NaN one is unknown, as 0. */
    float rtt = round_trip_ticks > 0.0f ? (round_trip_ticks < (float)SYNC_POLICY_AGE_TICKS ? round_trip_ticks : (float)SYNC_POLICY_AGE_TICKS) : 0.0f;
    return SYNC_POLICY_HOST_CLEAR_TICKS + (uint32_t)ceilf(rtt);
}

/* NaN in, NaN out: a rotation that is no number is never within the quiet turn. */
static float turn_angle(const float a[4], const float b[4]) {
    float d = fabsf(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]);
    return 2.0f * acosf(d >= 1.0f ? 1.0f : d);
}

bool sync_policy_quiet(sync_policy_still_t *still, const float position[3], const float rotation[4], bool asleep) {
    float dx = position[0] - still->anchor[0], dy = position[1] - still->anchor[1], dz = position[2] - still->anchor[2];
    bool near = still->ticks > 0 && sqrtf(dx * dx + dy * dy + dz * dz) <= SYNC_POLICY_QUIET_M &&
                turn_angle(rotation, still->anchor_rotation) <= SYNC_POLICY_QUIET_RAD;
    if (!near) {
        for (int i = 0; i < 3; i++) {
            still->anchor[i] = position[i];
        }
        for (int i = 0; i < 4; i++) {
            still->anchor_rotation[i] = rotation[i];
        }
        still->ticks = 1;
    } else if (still->ticks < 0xFFFFu) {
        still->ticks++;
    }
    return asleep || still->ticks > SYNC_POLICY_QUIET_TICKS;
}
