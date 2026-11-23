#include <math.h>
#include <string.h>

#include "sync_receiver.h"

void sync_receiver_init(sync_receiver_t *r, uint32_t tick_hz, float max_speed) {
    memset(r, 0, sizeof(*r));
    r->dt = 1.0f / (float)(tick_hz ? tick_hz : 60u);
    r->max_speed = max_speed;
}

void sync_receiver_forget(sync_receiver_t *r, uint16_t slot) {
    if (slot < BODY_SYNC_BODIES_MAX) {
        memset(&r->bodies[slot], 0, sizeof(r->bodies[slot]));
        for (uint8_t seat = 0; seat < BODY_SYNC_SEATS_MAX; seat++) {
            memset(&r->declared[seat][slot], 0, sizeof(r->declared[seat][slot]));
        }
    }
}

static bool tick_newer(uint32_t a, uint32_t b) { return (int32_t)(a - b) > 0; }

void sync_receiver_forget_seat(sync_receiver_t *r, uint8_t seat, bool keep_states) {
    if (seat >= BODY_SYNC_SEATS_MAX) {
        return;
    }
    memset(r->declared[seat], 0, sizeof(r->declared[seat]));
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        sync_received_t *b = &r->bodies[slot];
        if (b->has && b->from == seat) {
            if (keep_states) {
                b->ordered = false;
            } else {
                memset(b, 0, sizeof(*b));
            }
        }
    }
}

sync_intake_t sync_receiver_host_intake(sync_receiver_t *r, uint8_t seat, uint32_t tick, const sync_record_t *record, uint8_t owner_seat,
                                        uint16_t owner_number) {
    if (seat >= BODY_SYNC_SEATS_MAX || record->slot >= BODY_SYNC_BODIES_MAX) {
        return SYNC_INTAKE_STALE;
    }
    sync_declared_t *d = &r->declared[seat][record->slot];
    if (d->seen && !tick_newer(tick, d->tick)) {
        return SYNC_INTAKE_STALE;
    }
    d->seen = true;
    d->tick = tick;
    if (record->rare.groups & SYNC_RARE_AUTHORITY) {
        d->known = true;
        d->number = record->rare.number;
    }
    return owner_seat == seat && d->known && d->number == owner_number ? SYNC_INTAKE_TAKE : SYNC_INTAKE_REJECTED;
}

static void keep_rare(sync_received_t *b, const sync_rare_t *in) {
    if (in->groups & SYNC_RARE_MOTION) {
        b->rare.kinematic = in->kinematic;
        b->rare.locks = in->locks;
    }
    if (in->groups & SYNC_RARE_FLAGS) {
        b->rare.flags = in->flags;
    }
    if (in->groups & SYNC_RARE_AUTHORITY) {
        b->rare.seat = in->seat;
        b->rare.number = in->number;
        b->rare.hold = in->hold;
    }
    if (in->groups & SYNC_RARE_HOLDER) {
        b->rare.holder_slot = in->holder_slot;
        b->rare.holder_generation_low = in->holder_generation_low;
    }
    if (in->groups & SYNC_RARE_DATA) {
        b->rare.data_size = in->data_size;
        memcpy(b->rare.data, in->data, sizeof(b->rare.data));
    }
    b->known |= in->groups;
    b->rare.groups = b->known;
}

bool sync_receiver_store(sync_receiver_t *r, uint8_t from, uint32_t tick, const sync_record_t *record) {
    if (record->slot >= BODY_SYNC_BODIES_MAX) {
        return false;
    }
    sync_received_t *b = &r->bodies[record->slot];
    bool same_sender = b->has && b->ordered && b->from == from;
    if (same_sender && !tick_newer(tick, b->tick)) {
        return false;
    }
    if (record->sim || record->at_rest) {
        memcpy(b->velocity, record->velocity, sizeof(b->velocity));
        memcpy(b->angular, record->angular, sizeof(b->angular));
    } else if (same_sender) {
        float span = (float)(uint32_t)(tick - b->tick) * r->dt;
        float length = 0.0f;
        for (int i = 0; i < 3; i++) {
            b->velocity[i] = (record->position[i] - b->newest.position[i]) / span;
            length += b->velocity[i] * b->velocity[i];
        }
        length = sqrtf(length);
        if (!(length <= r->max_speed)) {
            float k = isfinite(length) && length > 0.0f ? r->max_speed / length : 0.0f;
            for (int i = 0; i < 3; i++) {
                b->velocity[i] *= k;
            }
        }
        memset(b->angular, 0, sizeof(b->angular));
    } else {
        memset(b->velocity, 0, sizeof(b->velocity));
        memset(b->angular, 0, sizeof(b->angular));
    }
    sync_rare_t rare = record->rare;
    /* Groups merged from a newer record are newer than these: every group still unacknowledged
       when that record went out went out in it again. */
    bool rare_newer = !same_sender || tick_newer(tick, b->rare_tick);
    b->newest = *record;
    b->has = true;
    b->fresh = true;
    b->from = from;
    b->tick = tick;
    b->ordered = true;
    if (rare_newer) {
        b->rare_tick = tick;
        keep_rare(b, &rare);
    }
    b->newest.rare = b->rare;
    return true;
}

bool sync_receiver_merge_rare(sync_receiver_t *r, uint8_t from, uint32_t tick, const sync_record_t *record) {
    if (record->slot >= BODY_SYNC_BODIES_MAX) {
        return false;
    }
    sync_received_t *b = &r->bodies[record->slot];
    if (!b->has || !b->ordered || b->from != from || !tick_newer(tick, b->rare_tick)) {
        return false;
    }
    b->rare_tick = tick;
    keep_rare(b, &record->rare);
    b->newest.rare = b->rare;
    return true;
}

sync_copy_move_t sync_receiver_decide(sync_receiver_t *r, uint16_t slot, const float now_position[3], const float now_rotation[4], bool placed,
                                      float target_position[3], float target_rotation[4]) {
    if (slot >= BODY_SYNC_BODIES_MAX) {
        return SYNC_COPY_HOLD;
    }
    sync_received_t *b = &r->bodies[slot];
    if (!b->has || !b->fresh) {
        return SYNC_COPY_HOLD;
    }
    b->fresh = false;
    const sync_record_t *in = &b->newest;
    memcpy(target_position, in->position, 3 * sizeof(float));
    memcpy(target_rotation, in->rotation, 4 * sizeof(float));
    float d[3] = {in->position[0] - now_position[0], in->position[1] - now_position[1], in->position[2] - now_position[2]};
    float distance = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    bool same = distance == 0.0f && memcmp(now_rotation, in->rotation, 4 * sizeof(float)) == 0;
    /* A body at rest at its owner is placed on that exact pose, so worlds at rest agree. */
    if (!placed || distance > SYNC_SNAP_DISTANCE || (in->at_rest && !same)) {
        return SYNC_COPY_PLACE;
    }
    return in->at_rest ? SYNC_COPY_HOLD : SYNC_COPY_DRIVE;
}
