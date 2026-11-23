#include <math.h>
#include <string.h>

#include "sync_soft.h"

static float length3(const float v[3]) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }

static void clamp3(float v[3], float limit) {
    float n = length3(v);
    if (n > limit && n > 0.0f) {
        for (int i = 0; i < 3; i++) {
            v[i] *= limit / n;
        }
    }
}

bool sync_soft_owner_rests(sync_soft_still_t *still, const sync_play_t *play) {
    if (!play->known) {
        return false; /* no pose to count from: the count waits */
    }
    float moved[3], turned[3];
    for (int i = 0; i < 3; i++) {
        moved[i] = play->position[i] - still->anchor[i];
    }
    sync_turn_between(still->anchor_rotation, play->rotation, turned);
    float m = length3(moved), t = length3(turned);
    if (still->ticks > 0 && m <= SYNC_SOFT_HOLD_M && t <= SYNC_SOFT_HOLD_RAD) {
        still->ticks = still->ticks < UINT16_MAX ? (uint16_t)(still->ticks + 1u) : still->ticks;
        still->resting = still->resting || still->ticks > SYNC_SOFT_STILL_TICKS;
    } else {
        /* A creeping pose crosses a step's edge by at most one decoded step (a record's rounding
           moves it a cell at once); further is a move however long the step was held. */
        still->resting = still->resting && still->ticks >= SYNC_SOFT_CREEP_TICKS && m <= 2.0f * SYNC_SOFT_HOLD_M && t <= 2.0f * SYNC_SOFT_HOLD_RAD;
        memcpy(still->anchor, play->position, sizeof(still->anchor));
        memcpy(still->anchor_rotation, play->rotation, sizeof(still->anchor_rotation));
        still->ticks = 1;
    }
    return play->at_rest || still->resting;
}

void sync_soft_still_at(sync_soft_still_t *still, const float position[3], const float rotation[4]) {
    memcpy(still->anchor, position, sizeof(still->anchor));
    memcpy(still->anchor_rotation, rotation, sizeof(still->anchor_rotation));
    still->ticks = SYNC_SOFT_STILL_TICKS + 1u;
    still->resting = true;
}

bool sync_soft_rest_hold(bool owner_rests, const sync_soft_t *s, bool soft_now, bool marked) {
    return owner_rests && !marked && (!soft_now || s->pause == 0);
}

bool sync_soft_mover(const float velocity[3], const float angular[3], float extent, float dt, float *reach) {
    float speed = length3(velocity) + length3(angular) * extent;
    *reach = speed * dt * SYNC_SOFT_REACH_STEPS + SYNC_SOFT_REACH_M;
    return speed > SYNC_SOFT_TOUCH_SPEED;
}

bool sync_soft_pushed(float approach_speed) { return approach_speed > SYNC_SOFT_LEAN_SPEED; }

uint16_t sync_soft_pause(float round_trip_ticks, uint32_t delay_ticks) {
    float ticks = ceilf(round_trip_ticks > 0.0f ? round_trip_ticks : 0.0f) + (float)delay_ticks + 2.0f;
    ticks = ticks < (float)SYNC_SOFT_PAUSE_TICKS ? (float)SYNC_SOFT_PAUSE_TICKS : ticks > (float)SYNC_SOFT_PAUSE_MAX_TICKS ? (float)SYNC_SOFT_PAUSE_MAX_TICKS : ticks;
    return (uint16_t)ticks;
}

void sync_soft_enter(sync_soft_t *s, bool from_local, uint16_t pause_ticks) {
    memset(s, 0, sizeof(*s));
    if (from_local) {
        s->pause = pause_ticks ? pause_ticks : (uint16_t)SYNC_SOFT_PAUSE_TICKS;
    } else {
        s->correcting = true;
    }
}

bool sync_soft_hit(const sync_soft_t *s, const float velocity[3], const float angular[3], const float gravity_dv[3]) {
    float dv[3], dw[3];
    for (int i = 0; i < 3; i++) {
        dv[i] = velocity[i] - s->velocity[i] - gravity_dv[i];
        dw[i] = angular[i] - s->angular[i];
    }
    return length3(dv) >= SYNC_SOFT_HIT_SPEED || length3(dw) >= SYNC_SOFT_HIT_SPIN;
}

/* The speed limits, on whatever the body is about to move with. True when it had to slow. */
static bool limit(const sync_soft_in_t *now, float v[3], float w[3]) {
    float before_v = length3(v), before_w = length3(w);
    clamp3(v, now->max_speed);
    clamp3(w, now->max_spin);
    return before_v > now->max_speed || before_w > now->max_spin;
}

static void keep(sync_soft_t *s, const float v[3], const float w[3]) {
    memcpy(s->velocity, v, sizeof(s->velocity));
    memcpy(s->angular, w, sizeof(s->angular));
}

void sync_soft_decide(sync_soft_t *s, const sync_soft_in_t *now, bool hit, const sync_play_t *play, sync_soft_out_t *out) {
    memset(out, 0, sizeof(*out));
    float v[3], w[3], err[3], turn[3];
    memcpy(v, now->velocity, sizeof(v));
    memcpy(w, now->angular, sizeof(w));
    for (int i = 0; i < 3; i++) {
        err[i] = play->position[i] - now->position[i];
    }
    sync_turn_between(now->rotation, play->rotation, turn);
    float distance = length3(err), angle = length3(turn);
    /* A pose or an owner's motion that is no number has no error to close: the body is held still
       rather than sent off on a NaN velocity. */
    float owner_motion = length3(play->velocity) + length3(play->angular);
    if (!isfinite(distance) || !isfinite(angle) || !isfinite(owner_motion)) {
        s->correcting = false;
        s->stall = 0;
        out->action = SYNC_SOFT_VELOCITY;
        keep(s, out->velocity, out->angular);
        return;
    }

    /* A touch while the chain is spent is no longer a hit: the pull runs under it until the chain
       has drained. */
    bool touched = hit || now->touches_local;
    if (touched && !s->spent) {
        uint16_t pause = now->pause_ticks ? now->pause_ticks : (uint16_t)SYNC_SOFT_PAUSE_TICKS;
        s->pause = pause > s->pause ? pause : s->pause;
    }
    if (distance > SYNC_SOFT_SNAP_M) {
        s->pause = 0;
        s->chain = 0;
        s->spent = false;
        s->correcting = false;
        out->action = SYNC_SOFT_PLACE;
        memcpy(out->velocity, play->velocity, sizeof(out->velocity));
        memcpy(out->angular, play->angular, sizeof(out->angular));
        limit(now, out->velocity, out->angular);
        keep(s, out->velocity, out->angular);
        return;
    }
    if (s->pause > 0 && !s->spent) {
        out->paused = true;
        s->pause--;
        s->chain++;
        s->spent = s->chain >= SYNC_SOFT_PAUSE_MAX_TICKS;
    } else {
        s->pause = 0; /* the chain's cap ends a pause however it was renewed */
        if (!touched && s->chain > 0) {
            s->chain--;
        }
        s->spent = s->spent && s->chain > 0;
        float dead_rad = now->owner_rests ? SYNC_SOFT_REST_RAD : SYNC_SOFT_DEAD_RAD;
        bool outside = distance > SYNC_SOFT_DEAD_M || angle > dead_rad;
        bool inside_half = distance < 0.5f * SYNC_SOFT_DEAD_M && angle < 0.5f * dead_rad;
        bool was = s->correcting;
        s->correcting = s->correcting ? !inside_half : outside;
        /* A copy pressed by this side's body is not pinned: its stall waits while the press lasts. */
        s->stall = s->correcting && was ? (uint16_t)(s->stall + (touched ? 0u : 1u)) : 0u;
        bool stalled = s->correcting && s->stall >= SYNC_SOFT_STALL_TICKS;
        if (stalled) {
            s->correcting = false;
            s->stall = 0;
            out->action = SYNC_SOFT_PLACE;
            memcpy(out->velocity, play->velocity, sizeof(out->velocity));
            memcpy(out->angular, play->angular, sizeof(out->angular));
            limit(now, out->velocity, out->angular);
            keep(s, out->velocity, out->angular);
            return;
        }
        if (s->correcting) {
            float pull[3], spin[3];
            for (int i = 0; i < 3; i++) {
                pull[i] = err[i] / SYNC_SOFT_PULL_S;
                spin[i] = turn[i] / SYNC_SOFT_PULL_S;
            }
            clamp3(pull, SYNC_SOFT_PULL_SPEED);
            clamp3(spin, SYNC_SOFT_PULL_SPIN);
            for (int i = 0; i < 3; i++) {
                v[i] += SYNC_SOFT_BLEND * (play->velocity[i] + pull[i] - v[i]);
                w[i] += SYNC_SOFT_BLEND * (play->angular[i] + spin[i] - w[i]);
            }
            limit(now, v, w);
            out->action = SYNC_SOFT_VELOCITY;
            memcpy(out->velocity, v, sizeof(v));
            memcpy(out->angular, w, sizeof(w));
            keep(s, v, w);
            return;
        }
    }
    /* Left to its simulation, but never faster than the seam's speeds. */
    if (!now->asleep && limit(now, v, w)) {
        out->action = SYNC_SOFT_VELOCITY;
        memcpy(out->velocity, v, sizeof(v));
        memcpy(out->angular, w, sizeof(w));
    }
    keep(s, v, w);
}
