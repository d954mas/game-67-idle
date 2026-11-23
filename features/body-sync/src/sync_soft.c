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

void sync_history_put(sync_history_t *h, uint32_t tick, const float position[3], const float rotation[4], const float velocity[3], const float angular[3]) {
    uint32_t k = tick % SYNC_HISTORY_TICKS;
    h->known[k] = true;
    h->tick[k] = tick;
    memcpy(h->position[k], position, sizeof(h->position[k]));
    memcpy(h->rotation[k], rotation, sizeof(h->rotation[k]));
    memcpy(h->velocity[k], velocity, sizeof(h->velocity[k]));
    memcpy(h->angular[k], angular, sizeof(h->angular[k]));
}

static bool history_get(const sync_history_t *h, uint32_t tick, uint32_t *k) {
    *k = tick % SYNC_HISTORY_TICKS;
    return h->known[*k] && h->tick[*k] == tick;
}

bool sync_history_at(const sync_history_t *h, double tick, float position[3], float rotation[4], float velocity[3], float angular[3]) {
    double base = floor(tick);
    uint32_t t0 = (uint32_t)(int64_t)base, a, b;
    if (!history_get(h, t0, &a) || !history_get(h, t0 + 1u, &b)) {
        return false;
    }
    float u = (float)(tick - base), dot = 0.0f;
    for (int i = 0; i < 4; i++) {
        dot += h->rotation[a][i] * h->rotation[b][i];
    }
    float sign = dot < 0.0f ? -1.0f : 1.0f, n = 0.0f;
    for (int i = 0; i < 4; i++) {
        rotation[i] = h->rotation[a][i] + (sign * h->rotation[b][i] - h->rotation[a][i]) * u;
        n += rotation[i] * rotation[i];
    }
    n = sqrtf(n);
    for (int i = 0; i < 4; i++) {
        rotation[i] = n > 0.0f ? rotation[i] / n : (i == 3 ? 1.0f : 0.0f);
    }
    for (int i = 0; i < 3; i++) {
        position[i] = h->position[a][i] + (h->position[b][i] - h->position[a][i]) * u;
        velocity[i] = h->velocity[a][i] + (h->velocity[b][i] - h->velocity[a][i]) * u;
        angular[i] = h->angular[a][i] + (h->angular[b][i] - h->angular[a][i]) * u;
    }
    return true;
}

void sync_history_correct(sync_history_t *h, uint32_t tick, const float moved[3], const float dv[3], const float dw[3]) {
    uint32_t k = tick % SYNC_HISTORY_TICKS;
    h->corrected[k] = true;
    h->corrected_tick[k] = tick;
    memcpy(h->moved[k], moved, sizeof(h->moved[k]));
    memcpy(h->dv[k], dv, sizeof(h->dv[k]));
    memcpy(h->dw[k], dw, sizeof(h->dw[k]));
}

void sync_history_since(const sync_history_t *h, double at, uint32_t now, float dt, float error[3], float turn[3], float dv[3], float dw[3]) {
    double back = (double)now - at; /* the compared state is the one after the step of `at` */
    for (uint32_t k = 0; k < SYNC_HISTORY_TICKS; k++) {
        int32_t age = (int32_t)(now - h->corrected_tick[k]);
        /* Made at an apply after that step: the compared state lacks it, the body now has it, the
           velocity change over every step since. */
        if (!h->corrected[k] || age < 0 || (double)age >= back) {
            continue;
        }
        float run = (float)age * dt;
        for (int i = 0; i < 3; i++) {
            error[i] -= h->moved[k][i] + h->dv[k][i] * run;
            turn[i] -= h->dw[k][i] * run;
            dv[i] -= h->dv[k][i];
            dw[i] -= h->dw[k][i];
        }
    }
}

bool sync_soft_owner_rests(sync_soft_still_t *still, const sync_play_t *play) {
    float moved[3], turned[3];
    for (int i = 0; i < 3; i++) {
        moved[i] = play->position[i] - still->anchor[i];
    }
    sync_turn_between(still->anchor_rotation, play->rotation, turned);
    if (still->ticks > 0 && length3(moved) <= SYNC_SOFT_HOLD_M && length3(turned) <= SYNC_SOFT_HOLD_RAD) {
        still->ticks = still->ticks < UINT16_MAX ? (uint16_t)(still->ticks + 1u) : still->ticks;
    } else {
        memcpy(still->anchor, play->position, sizeof(still->anchor));
        memcpy(still->anchor_rotation, play->rotation, sizeof(still->anchor_rotation));
        still->ticks = 1;
    }
    return play->known && (play->at_rest || still->ticks > SYNC_SOFT_STILL_TICKS);
}

bool sync_soft_rest_hold(bool owner_rests, const sync_soft_t *s, bool soft_now, bool marked) {
    return owner_rests && !marked && (!soft_now || s->pause == 0);
}

bool sync_soft_mover(const float velocity[3], const float angular[3], float extent, float dt, float *reach) {
    float speed = length3(velocity) + length3(angular) * extent;
    *reach = speed * dt * SYNC_SOFT_REACH_STEPS + SYNC_SOFT_REACH_M;
    return speed > SYNC_SOFT_TOUCH_SPEED;
}

bool sync_soft_pushed(float normal_impulse, float pusher_mass) { return normal_impulse > pusher_mass * SYNC_SOFT_LEAN_SPEED; }

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

/* A placement: at the playback pose with the owner's velocities, or, compared at event time,
   at this copy's own pose and velocities plus their errors, so it is never set back in time. */
static void place_at(const sync_soft_in_t *now, const sync_play_t *play, const float err[3], const float turn[3], const float dv[3], const float dw[3],
                     sync_soft_out_t *out) {
    out->action = SYNC_SOFT_PLACE;
    if (!now->aligned) {
        memcpy(out->position, play->position, sizeof(out->position));
        memcpy(out->rotation, play->rotation, sizeof(out->rotation));
        memcpy(out->velocity, play->velocity, sizeof(out->velocity));
        memcpy(out->angular, play->angular, sizeof(out->angular));
    } else {
        float angle = length3(turn), k = angle > 1e-9f ? sinf(0.5f * angle) / angle : 0.5f;
        float e[4] = {turn[0] * k, turn[1] * k, turn[2] * k, cosf(0.5f * angle)};
        const float *q = now->rotation;
        out->rotation[0] = e[3] * q[0] + e[0] * q[3] + e[1] * q[2] - e[2] * q[1];
        out->rotation[1] = e[3] * q[1] - e[0] * q[2] + e[1] * q[3] + e[2] * q[0];
        out->rotation[2] = e[3] * q[2] + e[0] * q[1] - e[1] * q[0] + e[2] * q[3];
        out->rotation[3] = e[3] * q[3] - e[0] * q[0] - e[1] * q[1] - e[2] * q[2];
        float n = sqrtf(out->rotation[0] * out->rotation[0] + out->rotation[1] * out->rotation[1] + out->rotation[2] * out->rotation[2] + out->rotation[3] * out->rotation[3]);
        for (int i = 0; i < 4; i++) {
            out->rotation[i] /= n;
        }
        for (int i = 0; i < 3; i++) {
            out->position[i] = now->position[i] + err[i];
            out->velocity[i] = now->velocity[i] + dv[i];
            out->angular[i] = now->angular[i] + dw[i];
        }
    }
    limit(now, out->velocity, out->angular);
}

void sync_soft_decide(sync_soft_t *s, const sync_soft_in_t *now, bool hit, const sync_play_t *play, sync_soft_out_t *out) {
    memset(out, 0, sizeof(*out));
    float v[3], w[3], err[3], turn[3];
    memcpy(v, now->velocity, sizeof(v));
    memcpy(w, now->angular, sizeof(w));
    float dv[3], dw[3];
    for (int i = 0; i < 3; i++) {
        err[i] = now->aligned ? now->aligned_error[i] : play->position[i] - now->position[i];
        turn[i] = now->aligned ? now->aligned_turn[i] : 0.0f;
        dv[i] = now->aligned ? now->aligned_dv[i] : play->velocity[i] - v[i];
        dw[i] = now->aligned ? now->aligned_dw[i] : play->angular[i] - w[i];
    }
    if (!now->aligned) {
        sync_turn_between(now->rotation, play->rotation, turn);
    }
    float distance = length3(err), angle = length3(turn);


    /* A touch that outlasts a whole chain is no longer a hit: the pull runs under it until a tick
       without one. */
    bool touched = hit || now->touches_local;
    if (touched && s->chain < SYNC_SOFT_PAUSE_MAX_TICKS) {
        uint16_t pause = now->pause_ticks ? now->pause_ticks : (uint16_t)SYNC_SOFT_PAUSE_TICKS;
        s->pause = pause > s->pause ? pause : s->pause;
    }
    if (now->waiting) {
        s->correcting = false;
        keep(s, v, w);
        return;
    }
    if (now->aligned && distance > SYNC_SOFT_SNAP_M) {
        /* The two outcomes differ (something here or there met the motion that the other side
           lacks): that error added to this side's motion could put the copy anywhere, inside a
           wall too. It goes where the owner's body was, the playback pose, as a snap does, and
           follows the playback from there: compared at event time it would now look a delay
           behind and be placed again every tick. */
        sync_soft_in_t at_play = *now;
        at_play.aligned = false;
        s->apart = true;
        s->pause = 0;
        s->chain = 0;
        s->correcting = false;
        place_at(&at_play, play, err, turn, dv, dw, out);
        keep(s, out->velocity, out->angular);
        return;
    }
    if (distance > SYNC_SOFT_SNAP_M) {
        s->pause = 0;
        s->chain = 0;
        s->correcting = false;
        place_at(now, play, err, turn, dv, dw, out);
        keep(s, out->velocity, out->angular);
        return;
    }
    if (s->pause > 0 && s->chain < SYNC_SOFT_PAUSE_MAX_TICKS) {
        s->pause--;
        s->chain++;
    } else {
        s->pause = 0; /* the chain's cap ends a pause however it was renewed */
        if (!touched) {
            s->chain = 0;
        }
        bool outside = distance > SYNC_SOFT_DEAD_M || angle > SYNC_SOFT_DEAD_RAD;
        bool inside_half = distance < 0.5f * SYNC_SOFT_DEAD_M && angle < 0.5f * SYNC_SOFT_DEAD_RAD;
        bool was = s->correcting;
        s->correcting = s->correcting ? !inside_half : outside;
        /* A copy pressed by this side's body is not pinned: its stall waits while the press lasts. */
        s->stall = s->correcting && was ? (uint16_t)(s->stall + (touched ? 0u : 1u)) : 0u;
        /* At event time a long pull is this side's simulation against the host's, not a pinned
           body: placed only once the owner rests and the pull can no longer close it by motion. */
        bool stalled = s->correcting && s->stall >= SYNC_SOFT_STALL_TICKS && (!now->aligned || now->owner_rests);
        if (stalled) {
            s->correcting = false;
            s->stall = 0;
            place_at(now, play, err, turn, dv, dw, out);
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
                v[i] += SYNC_SOFT_BLEND * (dv[i] + pull[i]);
                w[i] += SYNC_SOFT_BLEND * (dw[i] + spin[i]);
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
