#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "features/body_sync/body_sync.h"
#include "sync_authority.h"
#include "sync_codec.h"
#include "sync_masks.h"
#include "sync_playback.h"
#include "sync_policy.h"
#include "sync_receiver.h"
#include "sync_sender.h"
#include "sync_soft.h"

#include "box3d/constants.h"

/* The real seam: the Box3D shell around the parts. It reads bodies into the sender's inputs,
   decodes packets into the receiver and the playback buffer, and moves bodies as they say. Who
   gets which record, whose poses count, when a hold settles, how a copy is held, where it is
   at playback time, how fast it may be driven and what the eye sees of a jump are the parts'
   decisions; what is left here is reading and writing Box3D and the contact test. */

enum { BODY_SLOTS_HASH = 4 * BODY_SYNC_BODIES_MAX };

typedef struct entry {
    b3BodyId body;
    body_sync_class_t body_class;
    b3BodyType bound_type;
    b3MotionLocks bound_locks; /* restored when a copy turns local with no state received */
    bool copy_mode;
    bool placed; /* placed as a copy since it became one: until then its first pose is a placement */
    bool shown;  /* it has a drawn pose: a placement goes to the render offset instead of jumping */
    /* At a take from a copy: the newest snapshot then, and whether the take is still an
       optimistic claim. Back as a copy once the take was confirmed (granted, then moved on),
       those snapshots are stale: this side simulated on. Back while the claim still waited (it
       was refused, withdrawn, or a forced move overtook it), they are still the owner's. */
    bool took;
    bool took_pending;
    bool took_any;
    bool lost; /* at its first apply as a copy again, a take whose claim lost while it waited */
    uint32_t took_tick;
    sync_masks_t masks;
    sync_offset_t offset;
    /* The second layer: light enough for it (class LIGHT, mass at most light_mass_max), held
       soft now, and what the last step did to it. */
    bool light;
    bool soft;
    bool soft_hit;
    bool soft_paused; /* its pull waited at the last apply (sync_soft_out_t.paused) */
    bool owner_flying;
    bool owner_rests; /* its owner rests at playback time (sync_soft_owner_rests), as of the last apply */
    sync_soft_still_t owner_still;
    bool rest_held;       /* driven in the rest hold this tick */
    bool pushed;          /* a body of this side's pushed it in the last step */
    bool leaned;          /* ...or only leaned on it (a contact that acted, closing slower) */
    float step_v[3], step_w[3]; /* its velocities as the last apply left them: the step's start */
    bool layer;           /* a layer copy now: it keeps the character bits */
    uint32_t marked;      /* tick + 1 a moving body of this side's last came within reach */
    uint32_t marked_own;  /* ...and one this side simulates itself, not a copy nor the walk */
    sync_soft_t soft_state;
    /* Authority follows interaction: its stillness, the tick of its last agent contact, and a
       guest's claim of it, which goes on from the pose this side shows. */
    sync_policy_still_t still;
    bool quiet;
    uint32_t agent_tick;
    bool keep_own;
    bool returned_at_rest; /* released at a rest pose the host acknowledged, nothing played since */
    bool returned_pressed; /* released under this side's own press (sync_policy_take_in_t), still pressed */
    /* A lost take's hand-back (body_sync_lost_take): how many, the latest one's drawn distance to
       the winner's pose, and whether the drawn pose still glides off it. */
    uint32_t lost_count;
    float lost_hand_back;
    bool lost_glide;
    /* The body that last carried it, as this side knows it, and the filter that keeps a throw off
       its thrower until it has cleared her (release_filters). */
    body_sync_id_t carrier;
    b3JointId release_filter;
    uint32_t release_end;
    float owner_speed; /* m/s, its owner's at the playback pose, as of the last apply as a copy */
} entry_t;

/* The light copies one apply marks (mark_reached), and the walk through their contacts. */
typedef struct marking {
    struct body_sync *s;
    uint16_t queue[BODY_SYNC_BODIES_MAX];
    uint32_t count;
    bool whole; /* a mover of this side's own or a copy in flight: the touched heap wakes whole */
    bool own;   /* the mover marking now is one this side simulates (entry_t.marked_own) */
} marking_t;

struct body_sync {
    body_sync_config_t config;
    marking_t marking;
    float dt;
    uint32_t now; /* the last apply's or collect's tick */
    sync_codec_t codec;
    sync_authority_t authority;
    sync_sender_t sender;
    sync_receiver_t receiver;
    sync_playback_t playback;
    entry_t entries[BODY_SYNC_BODIES_MAX];
    bool joined[BODY_SYNC_SEATS_MAX];
    body_sync_traffic_t traffic[BODY_SYNC_SEATS_MAX];
    sync_body_input_t inputs[BODY_SYNC_BODIES_MAX];
    uint8_t send_to[BODY_SYNC_BODIES_MAX];
    sync_record_t scratch[BODY_SYNC_BODIES_MAX];
    char error[128];
    /* A bound body's slot by its Box3D index, to tell a contact with a body this side simulates:
       open addressing, BODY_SLOTS_HASH entries for at most BODY_SYNC_BODIES_MAX bodies. */
    int32_t hash_index[BODY_SLOTS_HASH];
    uint16_t hash_slot[BODY_SLOTS_HASH];
    uint16_t agents[BODY_SYNC_BODIES_MAX]; /* gather_agents, once an apply */
    uint32_t agent_count;
};

const char *body_sync_implementation(void) { return "real 0.8.0"; }

const char *body_sync_traffic_kind_name(body_sync_traffic_kind_t kind) {
    static const char *names[] = {"header", "copy", "sim", "held", "authority"};
    return (unsigned)kind < BODY_SYNC_TRAFFIC_COUNT ? names[kind] : "unknown";
}

const char *body_sync_last_error(const body_sync_t *sync) { return sync ? sync->error : ""; }

static body_sync_result_t invalid(body_sync_t *s, const char *what) {
    snprintf(s->error, sizeof(s->error), "%s", what);
    return BODY_SYNC_INVALID;
}

static bool is_host(const body_sync_t *s) { return s->config.seat == BODY_SYNC_SEAT_HOST; }
static bool seat_ok(const body_sync_t *s, uint8_t seat) { return seat < s->config.seats && seat != s->config.seat; }

static float speed_of(b3BodyId body) {
    b3Vec3 v = b3Body_GetLinearVelocity(body);
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

/* Ticks the host's copy of `seat`'s agents may trail that guest's contact: a round trip, a
   playback delay and a tick, capped (SYNC_POLICY_TAKE_SLOP_M derives it). */
static float evidence_ticks(const body_sync_t *s, uint8_t seat) {
    return sync_policy_evidence_ticks(sync_sender_round_trip(&s->sender, seat), (float)s->playback.timelines[seat].delay, s->config.tick_hz);
}

/* The gap from `box` to the agent in slot `k`, where the host shows it or where its newest state
   puts it (a playback delay nearer the guest's present), whichever is nearer. *reach is what the
   two may travel apart over `age_s`, *agent_speed the faster of the agent's shown and sent
   speeds. */
static float agent_gap(const body_sync_t *s, uint32_t k, b3AABB box, float body_speed, float age_s, float *reach, float *agent_speed) {
    const entry_t *g = &s->entries[k];
    b3AABB other = b3Body_ComputeAABB(g->body);
    const sync_received_t *in = &s->receiver.bodies[k];
    b3Vec3 at = b3Body_GetPosition(g->body);
    float shift[2][3] = {{0.0f, 0.0f, 0.0f}, {in->newest.position[0] - at.x, in->newest.position[1] - at.y, in->newest.position[2] - at.z}};
    const float lo[3] = {box.lowerBound.x, box.lowerBound.y, box.lowerBound.z}, hi[3] = {box.upperBound.x, box.upperBound.y, box.upperBound.z};
    float sent = in->has ? sqrtf(in->velocity[0] * in->velocity[0] + in->velocity[1] * in->velocity[1] + in->velocity[2] * in->velocity[2]) : 0.0f;
    float shown = speed_of(g->body);
    *agent_speed = sent > shown ? sent : shown;
    int windows = in->has ? 2 : 1;
    *reach = sync_policy_take_reach(*agent_speed, body_speed, age_s);
    float best = INFINITY;
    for (int w = 0; w < windows; w++) {
        const float olo[3] = {other.lowerBound.x + shift[w][0], other.lowerBound.y + shift[w][1], other.lowerBound.z + shift[w][2]};
        const float ohi[3] = {other.upperBound.x + shift[w][0], other.upperBound.y + shift[w][1], other.upperBound.z + shift[w][2]};
        float d = sync_policy_box_gap(lo, hi, olo, ohi);
        best = d < best ? d : best;
    }
    return best;
}

/* The boxes a contact claim is measured with: the target's, and the agent's about its position. */
static void contact_boxes(b3AABB target, b3AABB agent, b3Vec3 at, sync_contact_scene_t *scene) {
    const float t_lo[3] = {target.lowerBound.x, target.lowerBound.y, target.lowerBound.z}, t_hi[3] = {target.upperBound.x, target.upperBound.y, target.upperBound.z};
    const float a_lo[3] = {agent.lowerBound.x - at.x, agent.lowerBound.y - at.y, agent.lowerBound.z - at.z};
    const float a_hi[3] = {agent.upperBound.x - at.x, agent.upperBound.y - at.y, agent.upperBound.z - at.z};
    memcpy(scene->target_lo, t_lo, sizeof(t_lo));
    memcpy(scene->target_hi, t_hi, sizeof(t_hi));
    memcpy(scene->agent_lo, a_lo, sizeof(a_lo));
    memcpy(scene->agent_hi, a_hi, sizeof(a_hi));
}

/* Host: a take's evidence (sync_policy_grants). The claimant's agents as the host shows them: its
   characters, what it carries, throws or owns by interaction, the nearest by its gap less its
   reach. */
static float take_gap(void *context, uint8_t seat, uint32_t slot, float *reach) {
    body_sync_t *s = context;
    const entry_t *e = &s->entries[slot];
    *reach = 0.0f;
    if (!b3Body_IsValid(e->body)) {
        return INFINITY;
    }
    b3AABB box = b3Body_ComputeAABB(e->body);
    float age_s = evidence_ticks(s, seat) * s->dt;
    float body_speed = speed_of(e->body), best = INFINITY;
    for (uint32_t k = 0; k < BODY_SYNC_BODIES_MAX; k++) {
        const sync_authority_body_t *a = &s->authority.bodies[k];
        const entry_t *g = &s->entries[k];
        /* The host's own hold says a take it granted at once; the owner's states say the rest
           (what it carries, a flight that went on owned by interaction). */
        bool held = a->holder != BODY_SYNC_ID_NONE || (a->has_in && a->in_holder != BODY_SYNC_ID_NONE);
        bool moving = a->hold == BODY_SYNC_HOLD_UNTIL_SETTLED || a->hold == BODY_SYNC_HOLD_UNTIL_REST ||
                      (a->has_in && (a->in_hold == BODY_SYNC_HOLD_UNTIL_SETTLED || a->in_hold == BODY_SYNC_HOLD_UNTIL_REST));
        if (k == slot || !a->bound || a->seat != seat || !b3Body_IsValid(g->body) || !(g->body_class == BODY_SYNC_CLASS_CHARACTER || held || moving)) {
            continue;
        }
        float r, speed;
        float d = agent_gap(s, k, box, body_speed, age_s, &r, &speed);
        if (d - r < best - *reach) {
            best = d;
            *reach = r;
        }
    }
    return best;
}

body_sync_t *body_sync_create(const body_sync_config_t *config) {
    if (!config || !b3World_IsValid(config->world) || config->seats < 1 || config->seats > BODY_SYNC_SEATS_MAX ||
        config->seat >= config->seats || config->tick_hz < 1 || config->tick_hz > 1000 || config->delay_min > 60 || config->delay_max > 60 ||
        !(config->light_mass_max >= 0.0f && config->light_mass_max <= 1e6f) || config->authority > BODY_SYNC_AUTHORITY_INTERACTION ||
        (config->authority == BODY_SYNC_AUTHORITY_INTERACTION && !(config->light_mass_max > 0.0f)) ||
        (config->delay_min && config->delay_max && config->delay_min > config->delay_max)) {
        return NULL;
    }
    sync_limits_t limits;
    memcpy(limits.bounds_min, config->bounds_min, sizeof(limits.bounds_min));
    memcpy(limits.bounds_max, config->bounds_max, sizeof(limits.bounds_max));
    limits.max_speed = config->max_speed;
    limits.max_spin = config->max_spin;
    body_sync_t *s = calloc(1, sizeof(*s));
    if (!s) {
        return NULL;
    }
    if (!sync_codec_init(&s->codec, &limits) || !sync_sender_budget_ok(&s->codec, (uint8_t)config->seats, config->state_budget)) {
        free(s);
        return NULL;
    }
    s->config = *config;
    s->dt = 1.0f / (float)config->tick_hz;
    sync_authority_init(&s->authority, config->seat, (uint8_t)config->seats, s->error, sizeof(s->error));
    s->authority.take_gap = take_gap;
    s->authority.take_context = s;
    s->authority.mode = config->authority == BODY_SYNC_AUTHORITY_INTERACTION ? SYNC_POLICY_INTERACTION : SYNC_POLICY_HOST;
    sync_sender_init(&s->sender, &s->codec, (uint8_t)config->seats, config->state_budget);
    sync_receiver_init(&s->receiver, config->tick_hz, config->max_speed);
    sync_playback_init(&s->playback, config->tick_hz, config->delay_min, config->delay_max);
    for (uint32_t i = 0; i < BODY_SYNC_BODIES_MAX; i++) {
        sync_offset_clear(&s->entries[i].offset);
    }
    for (uint32_t i = 0; i < BODY_SLOTS_HASH; i++) {
        s->hash_index[i] = -1;
    }
    return s;
}

void body_sync_destroy(body_sync_t *sync) {
    if (!sync) {
        return;
    }
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        entry_t *e = &sync->entries[slot];
        if (sync->authority.bodies[slot].bound && b3Body_IsValid(e->body)) {
            sync_masks_restore(&e->masks, e->body);
        }
        /* The world is the caller's and outlives the seam: its filters go with it. */
        if (b3Joint_IsValid(e->release_filter)) {
            b3DestroyJoint(e->release_filter, false);
        }
    }
    free(sync);
}

void body_sync_seat_joined(body_sync_t *s, uint8_t seat) {
    if (!s || !seat_ok(s, seat) || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST)) {
        return;
    }
    sync_authority_seat_joined(&s->authority, seat);
    sync_sender_seat(&s->sender, seat, true);
    sync_receiver_forget_seat(&s->receiver, seat, false);
    sync_playback_forget_seat(&s->playback, seat, false);
    s->joined[seat] = true;
}

void body_sync_seat_left(body_sync_t *s, uint8_t seat) {
    if (!s || !seat_ok(s, seat)) {
        return;
    }
    s->joined[seat] = false;
    sync_sender_seat(&s->sender, seat, false);
    sync_receiver_forget_seat(&s->receiver, seat, true);
    sync_playback_forget_seat(&s->playback, seat, true);
    sync_authority_seat_left(&s->authority, seat);
}

static void forget_slot(body_sync_t *s, uint32_t slot) {
    sync_sender_forget(&s->sender, (uint16_t)slot);
    sync_receiver_forget(&s->receiver, (uint16_t)slot);
    sync_playback_forget_body(&s->playback, (uint16_t)slot);
}

static uint32_t hash_of(int32_t index) { return ((uint32_t)index * 2654435761u) % BODY_SLOTS_HASH; }

static void hash_put(body_sync_t *s, b3BodyId body, uint32_t slot) {
    uint32_t h = hash_of(body.index1);
    /* An entry whose slot no longer holds that body is free again: rebinds never fill it. */
    for (uint32_t n = 0; n < BODY_SLOTS_HASH && s->hash_index[h] != -1 && s->hash_index[h] != body.index1; n++) {
        uint32_t was = s->hash_slot[h];
        if (!s->authority.bodies[was].bound || s->entries[was].body.index1 != s->hash_index[h]) {
            break;
        }
        h = (h + 1u) % BODY_SLOTS_HASH;
    }
    s->hash_index[h] = body.index1;
    s->hash_slot[h] = (uint16_t)slot;
}

/* The bound slot of a Box3D body, BODY_SYNC_BODIES_MAX when none. */
static uint32_t hash_get(const body_sync_t *s, b3BodyId body) {
    uint32_t h = hash_of(body.index1);
    for (uint32_t n = 0; n < BODY_SLOTS_HASH && s->hash_index[h] != -1; n++, h = (h + 1u) % BODY_SLOTS_HASH) {
        if (s->hash_index[h] == body.index1) {
            uint32_t slot = s->hash_slot[h];
            return s->authority.bodies[slot].bound && B3_ID_EQUALS(s->entries[slot].body, body) ? slot : BODY_SYNC_BODIES_MAX;
        }
    }
    return BODY_SYNC_BODIES_MAX;
}

body_sync_result_t body_sync_bind(body_sync_t *s, body_sync_id_t id, const body_sync_bind_t *bind) {
    uint32_t slot = body_sync_id_slot(id);
    if (!bind || slot >= BODY_SYNC_BODIES_MAX || body_sync_id_generation(id) == 0 || !b3Body_IsValid(bind->body) ||
        b3Body_GetType(bind->body) == b3_staticBody || bind->body_class > BODY_SYNC_CLASS_CHARACTER || bind->authority >= s->config.seats ||
        (s->config.character_category && sync_masks_shapes(bind->body) > BODY_SYNC_SHAPES_MAX)) {
        return invalid(s, "bind: id, body, class, authority or shape count out of range");
    }
    body_sync_result_t r = sync_authority_bind(&s->authority, id, bind->authority, bind->number);
    if (r != BODY_SYNC_OK) {
        return r;
    }
    entry_t *e = &s->entries[slot];
    memset(e, 0, sizeof(*e));
    e->body = bind->body;
    e->body_class = bind->body_class;
    e->bound_type = b3Body_GetType(bind->body);
    e->bound_locks = b3Body_GetMotionLocks(bind->body);
    e->light = s->config.light_mass_max > 0.0f && bind->body_class == BODY_SYNC_CLASS_LIGHT && b3Body_GetMass(bind->body) <= s->config.light_mass_max;
    s->authority.bodies[slot].takable = e->light;
    sync_offset_clear(&e->offset);
    forget_slot(s, slot);
    hash_put(s, bind->body, slot);
    return BODY_SYNC_OK;
}

body_sync_result_t body_sync_unbind(body_sync_t *s, body_sync_id_t id) {
    sync_authority_body_t *a = sync_authority_body(&s->authority, id);
    if (a) {
        entry_t *e = &s->entries[body_sync_id_slot(id)];
        if (b3Body_IsValid(e->body)) {
            sync_masks_restore(&e->masks, e->body); /* the caller gets its body back as it bound it */
        }
        if (b3Joint_IsValid(e->release_filter)) {
            b3DestroyJoint(e->release_filter, false);
        }
    }
    body_sync_result_t r = sync_authority_unbind(&s->authority, id);
    if (r == BODY_SYNC_OK) {
        forget_slot(s, body_sync_id_slot(id));
    }
    return r;
}

b3BodyId body_sync_body(const body_sync_t *s, body_sync_id_t id) {
    return sync_authority_body_const(&s->authority, id) ? s->entries[body_sync_id_slot(id)].body : b3_nullBodyId;
}

bool body_sync_authority(const body_sync_t *s, body_sync_id_t id, body_sync_authority_t *out) {
    const sync_authority_body_t *b = sync_authority_body_const(&s->authority, id);
    if (!b) {
        return false;
    }
    sync_authority_describe(&s->authority, b, out);
    return true;
}

bool body_sync_contact_number(const body_sync_t *s, body_sync_id_t id, uint16_t *number) {
    const sync_authority_body_t *b = sync_authority_body_const(&s->authority, id);
    return b && sync_authority_contact_number(&s->authority, b, number);
}

body_sync_contact_verdict_t body_sync_check_contact(const body_sync_t *s, uint8_t seat, body_sync_id_t agent, uint16_t number,
                                                    body_sync_id_t target, body_sync_contact_t *out) {
    body_sync_contact_t scratch;
    out = out ? out : &scratch;
    *out = (body_sync_contact_t){.gap = INFINITY};
    float ticks = seat < s->config.seats ? evidence_ticks(s, seat) : 0.0f;
    const sync_authority_ended_t *ended;
    body_sync_contact_verdict_t verdict = sync_authority_check_contact(&s->authority, seat, agent, number, (uint32_t)ceilf(ticks), &ended);
    if (verdict == BODY_SYNC_CONTACT_STALE || agent == target || !sync_authority_body_const(&s->authority, target)) {
        return BODY_SYNC_CONTACT_STALE;
    }
    const entry_t *g = &s->entries[body_sync_id_slot(agent)], *t = &s->entries[body_sync_id_slot(target)];
    if (!b3Body_IsValid(g->body) || !b3Body_IsValid(t->body)) {
        return BODY_SYNC_CONTACT_STALE;
    }
    out->ended = ended != NULL;
    out->ended_age_ticks = ended ? s->authority.now - ended->tick : 0u;
    sync_contact_scene_t scene = {.target_speed = speed_of(t->body), .tick_hz = s->config.tick_hz, .max_speed = s->config.max_speed,
                                  .slack_ticks = (float)s->playback.timelines[seat].delay + 1.0f, .window_ticks = (uint32_t)ceilf(ticks)};
    contact_boxes(b3Body_ComputeAABB(t->body), b3Body_ComputeAABB(g->body), b3Body_GetPosition(g->body), &scene);
    sync_authority_contact_measure(&s->authority, agent, seat, ended, &scene, &out->gap, &out->reach, &out->approach_max);
    return verdict;
}

bool body_sync_is_local(const body_sync_t *s, body_sync_id_t id) {
    const sync_authority_body_t *b = sync_authority_body_const(&s->authority, id);
    return b && sync_authority_local(&s->authority, b);
}

static uint8_t locks_bits(b3MotionLocks l);

/* A grant or a forced move carries the body as it is now, not as the last collect read it. */
static void note_live(body_sync_t *s, body_sync_id_t id) {
    sync_authority_body_t *a = sync_authority_body(&s->authority, id);
    b3BodyId body = s->entries[body_sync_id_slot(id)].body;
    if (a && b3Body_IsValid(body)) {
        sync_authority_note_motion(&s->authority, a, b3Body_GetType(body) == b3_kinematicBody, locks_bits(b3Body_GetMotionLocks(body)));
    }
}

static void resume(body_sync_t *s, uint32_t slot, bool was_copy);

body_sync_result_t body_sync_claim(body_sync_t *s, body_sync_id_t id, body_sync_hold_t hold) {
    body_sync_result_t r = sync_authority_claim(&s->authority, id, hold);
    /* The claim makes the body the caller's at once, and the caller may move it before the next
       apply (a grab carries it to the hand): it takes over now, from the pose the claimant shows,
       as a take does. A placement at the newest state would leave its jump in the render offset,
       and the caller's move would carry that offset off the hand. */
    uint32_t slot = body_sync_id_slot(id);
    if (r == BODY_SYNC_OK && s->entries[slot].copy_mode && body_sync_is_local(s, id)) {
        s->entries[slot].keep_own = !is_host(s); /* the host's claim is a forced move: from the owner's newest state */
        resume(s, slot, true);
    }
    return r;
}
body_sync_result_t body_sync_release(body_sync_t *s, body_sync_id_t id) { return sync_authority_release(&s->authority, id); }
bool body_sync_poll_claim(body_sync_t *s, body_sync_claim_event_t *out) { return sync_authority_poll(&s->authority, out); }
body_sync_result_t body_sync_answer(body_sync_t *s, const body_sync_claim_event_t *claim, bool granted) {
    if (claim) {
        note_live(s, claim->id);
    }
    return sync_authority_answer(&s->authority, claim, granted);
}
body_sync_result_t body_sync_assign(body_sync_t *s, body_sync_id_t id, uint8_t seat, body_sync_hold_t hold) {
    note_live(s, id);
    return sync_authority_assign(&s->authority, id, seat, hold);
}
body_sync_result_t body_sync_set_holder(body_sync_t *s, body_sync_id_t id, body_sync_id_t holder) {
    return sync_authority_set_holder(&s->authority, id, holder);
}
body_sync_result_t body_sync_set_flags(body_sync_t *s, body_sync_id_t id, uint8_t flags) { return sync_authority_set_flags(&s->authority, id, flags); }
uint8_t body_sync_flags(const body_sync_t *s, body_sync_id_t id) {
    const sync_authority_body_t *a = sync_authority_body_const(&s->authority, id);
    if (a && !sync_authority_local(&s->authority, a)) {
        uint8_t known = 0;
        const sync_rare_t *rare = sync_playback_rare(&s->playback, (uint16_t)body_sync_id_slot(id), &known);
        if (known & SYNC_RARE_FLAGS) {
            return rare->flags; /* as of the copy's playback time */
        }
    }
    return sync_authority_flags(&s->authority, id);
}

body_sync_result_t body_sync_set_data(body_sync_t *s, body_sync_id_t id, const void *data, uint32_t size) {
    return sync_authority_set_data(&s->authority, id, data, size);
}

uint32_t body_sync_data(const body_sync_t *s, body_sync_id_t id, void *out, uint32_t capacity) {
    const sync_authority_body_t *a = sync_authority_body_const(&s->authority, id);
    if (!a || !out) {
        return 0;
    }
    if (sync_authority_local(&s->authority, a)) {
        return sync_authority_data(&s->authority, id, out, capacity);
    }
    uint8_t known = 0;
    const sync_rare_t *rare = sync_playback_rare(&s->playback, (uint16_t)body_sync_id_slot(id), &known);
    uint32_t size = (known & SYNC_RARE_DATA) ? rare->data_size : 0u; /* as of playback time */
    size = size < capacity ? size : capacity;
    memcpy(out, rare->data, size);
    return size;
}

void body_sync_set_peer_tick(body_sync_t *s, uint8_t seat, double tick) {
    (void)s;
    (void)seat;
    (void)tick;
}

static body_sync_result_t format(body_sync_t *s, uint8_t seat, const char *what) {
    snprintf(s->error, sizeof(s->error), "seat %u: %s", (unsigned)seat, what);
    s->traffic[seat].refused++;
    return BODY_SYNC_FORMAT;
}

/* The timeline a record plays on and its sample tick there. */
static void timeline_of(const sync_record_t *r, uint8_t seat, uint32_t tick, uint8_t *timeline, uint32_t *sample) {
    *timeline = r->origin.relayed ? r->origin.seat : seat;
    *sample = r->origin.relayed ? tick - r->origin.age : tick;
}

/* The groups the receiver keeps for a body, as authority reads them; `hold` and `flags` stand in
   for a group never received. */
static void note_received(body_sync_t *s, sync_authority_body_t *a, const sync_received_t *in, uint8_t hold, uint8_t flags, bool motion) {
    sync_authority_received(a, (in->known & SYNC_RARE_AUTHORITY) ? in->rare.hold : hold,
                            (in->known & SYNC_RARE_HOLDER) ? sync_authority_holder_id(&s->authority, in->rare.holder_slot, in->rare.holder_generation_low, SYNC_GEN_BITS)
                                                           : BODY_SYNC_ID_NONE,
                            (in->known & SYNC_RARE_FLAGS) ? in->rare.flags : flags);
    if (motion) {
        sync_authority_received_motion(a, (in->known & SYNC_RARE_MOTION) != 0, in->rare.kinematic, in->rare.locks);
    }
    if (in->known & SYNC_RARE_DATA) {
        sync_authority_received_data(a, in->rare.data_size, in->rare.data);
    }
}

body_sync_result_t body_sync_receive_state(body_sync_t *s, uint8_t seat, uint32_t tick, const uint8_t *data, uint32_t size) {
    if (!seat_ok(s, seat) || !s->joined[seat] || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST)) {
        return invalid(s, "receive_state: not a seat this side hears");
    }
    uint32_t count = 0;
    char why[96];
    if (!sync_packet_read(&s->codec, (uint8_t)s->config.seats, data, size, s->scratch, BODY_SYNC_BODIES_MAX, &count, why, sizeof(why))) {
        return format(s, seat, why);
    }
    /* Only the host relays, and only a guest's body to another guest. */
    for (uint32_t i = 0; i < count; i++) {
        const sync_origin_t *o = &s->scratch[i].origin;
        if (o->relayed && (is_host(s) || o->seat == BODY_SYNC_SEAT_HOST || o->seat == s->config.seat)) {
            return format(s, seat, "a relayed record from a guest, or naming the host or this seat");
        }
    }
    body_sync_traffic_t *t = &s->traffic[seat];
    t->received_bytes[BODY_SYNC_TRAFFIC_HEADER] += 1;
    for (uint32_t i = 0; i < count; i++) {
        const sync_record_t *r = &s->scratch[i];
        t->received_bytes[r->sim ? BODY_SYNC_TRAFFIC_SIM : BODY_SYNC_TRAFFIC_COPY] += sync_codec_record_bits(&s->codec, r) / 8u;
        body_sync_id_t id = sync_authority_holder_id(&s->authority, r->slot, r->generation_low, SYNC_GEN_BITS);
        sync_authority_body_t *a = sync_authority_body(&s->authority, id);
        if (!a) {
            t->stale++;
            continue;
        }
        uint8_t timeline;
        uint32_t sample;
        timeline_of(r, seat, tick, &timeline, &sample);
        /* A tick far ahead of its timeline's clock would drag every copy of that owner with it:
           alone it is dropped, a run of them restarts the clock (sync_playback_admit). */
        if (!sync_playback_admit(&s->playback, timeline, sample, s->now)) {
            t->rejected++;
            continue;
        }
        if (is_host(s)) {
            sync_intake_t intake = sync_receiver_host_intake(&s->receiver, seat, tick, r, a->seat, a->number);
            if (intake != SYNC_INTAKE_TAKE) {
                t->stale += intake == SYNC_INTAKE_STALE ? 1u : 0u;
                t->rejected += intake == SYNC_INTAKE_REJECTED ? 1u : 0u;
                continue;
            }
        } else {
            if (r->rare.groups & SYNC_RARE_AUTHORITY) {
                sync_authority_from_state(&s->authority, a, r->rare.seat, r->rare.number, r->rare.hold);
            }
            if (sync_authority_local(&s->authority, a)) {
                /* Not applied, yet buffered: the host's state of a body this guest still holds
                   (its take-back is on the way) is what the copy plays once it is one again.
                   Once its claim is granted the host sends it nothing of the body: a state still
                   arriving is from before the grant, and played later (a rest pose at its spawn)
                   it would pull the body back there. */
                if (!a->claim_pending) {
                    t->stale++;
                    continue;
                }
                sync_playback_arrival(&s->playback, timeline, sample, s->now);
                sync_playback_file(&s->playback, timeline, sample, r);
                /* Its rare groups count as known at the host once this packet's fate comes back:
                   a refusal then leaves a copy holding what the host holds (a hold its agents set
                   going), not the groups from before the take. Its motion is the claimant's own
                   until then (sync_authority_note_motion). */
                if (sync_receiver_merge_rare(&s->receiver, seat, sample, r)) {
                    note_received(s, a, &s->receiver.bodies[r->slot], a->has_in ? a->in_hold : (uint8_t)BODY_SYNC_HOLD_UNTIL_RELEASE,
                                  a->has_in ? a->in_flags : a->flags, false);
                }
                continue;
            }
        }
        if (!sync_receiver_store(&s->receiver, seat, sample, r)) {
            t->stale++;
            continue;
        }
        /* The clock hears only what the seam took: a record dropped above moves no timeline. */
        sync_playback_arrival(&s->playback, timeline, sample, s->now);
        sync_playback_file(&s->playback, timeline, sample, r);
        body_sync_authority_t now;
        sync_authority_describe(&s->authority, a, &now);
        note_received(s, a, &s->receiver.bodies[r->slot], (uint8_t)now.hold, a->flags, true);
        if (is_host(s)) {
            sync_authority_received_pose(a, seat, tick, s->authority.now, s->playback.timelines[seat].delay,
                                         s->receiver.bodies[r->slot].newest.position);
        }
    }
    return BODY_SYNC_OK;
}

body_sync_result_t body_sync_receive_record(body_sync_t *s, uint8_t seat, uint32_t tick, const uint8_t *data, uint32_t size) {
    if (!seat_ok(s, seat) || !s->joined[seat] || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST)) {
        return invalid(s, "receive_record: not a seat this side hears");
    }
    char why[96];
    body_sync_traffic_t *t = &s->traffic[seat];
    if (sync_authority_receive(&s->authority, seat, tick, data, size, &t->stale, why, sizeof(why)) == BODY_SYNC_FORMAT) {
        return format(s, seat, why);
    }
    t->received_bytes[BODY_SYNC_TRAFFIC_AUTHORITY] += size;
    return BODY_SYNC_OK;
}

static b3Quat quat_of(const float q[4]) { return (b3Quat){{q[0], q[1], q[2]}, q[3]}; }

static uint8_t locks_bits(b3MotionLocks l) {
    return (uint8_t)(l.linearX | l.linearY << 1 | l.linearZ << 2 | l.angularX << 3 | l.angularY << 4 | l.angularZ << 5);
}

static b3MotionLocks locks_of(uint8_t bits) {
    return (b3MotionLocks){.linearX = (bits & 1u) != 0, .linearY = (bits & 2u) != 0, .linearZ = (bits & 4u) != 0,
                           .angularX = (bits & 8u) != 0, .angularY = (bits & 16u) != 0, .angularZ = (bits & 32u) != 0};
}

static void stop(b3BodyId body) {
    b3Body_SetLinearVelocity(body, (b3Vec3){0.0f, 0.0f, 0.0f});
    b3Body_SetAngularVelocity(body, (b3Vec3){0.0f, 0.0f, 0.0f});
}

/* This side's agents for the overlap rule (lands_in_agent): the bound bodies it simulates. */
static void gather_agents(body_sync_t *s) {
    s->agent_count = 0;
    if (s->config.light_mass_max <= 0.0f || is_host(s)) {
        return;
    }
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        const entry_t *e = &s->entries[slot];
        if (s->authority.bodies[slot].bound && b3Body_IsValid(e->body) && b3Body_GetType(e->body) == b3_dynamicBody &&
            sync_authority_local(&s->authority, &s->authority.bodies[slot])) {
            s->agents[s->agent_count++] = (uint16_t)slot;
        }
    }
}

/* How deep two shapes on these transforms overlap, as Box3D's contact manifold measures it: the
   depth its solver pushes out and contact data reports. 0 when apart; false for a pair without a
   convex manifold (a compound, a mesh). */
static bool overlap_depth(b3ShapeId a, b3WorldTransform ta, b3ShapeId b, b3WorldTransform tb, float *depth) {
    b3ShapeType kind_a = b3Shape_GetType(a), kind_b = b3Shape_GetType(b);
    int rank_a = kind_a == b3_hullShape ? 2 : kind_a == b3_capsuleShape ? 1 : kind_a == b3_sphereShape ? 0 : -1;
    int rank_b = kind_b == b3_hullShape ? 2 : kind_b == b3_capsuleShape ? 1 : kind_b == b3_sphereShape ? 0 : -1;
    if (rank_a < 0 || rank_b < 0) {
        return false;
    }
    /* Each collider takes the higher of hull, capsule, sphere as its first shape. */
    if (rank_a < rank_b) {
        b3ShapeId sa = a;
        b3WorldTransform st = ta;
        int sr = rank_a;
        a = b, ta = tb, rank_a = rank_b;
        b = sa, tb = st, rank_b = sr;
    }
    b3LocalManifoldPoint points[B3_MAX_MANIFOLD_POINTS];
    b3LocalManifold m = {0};
    m.points = points;
    b3Transform b_in_a = b3InvMulWorldTransforms(ta, tb);
    b3SimplexCache simplex = {0};
    b3SATCache sat = {0};
    b3Sphere sphere_a, sphere_b;
    b3Capsule capsule_a, capsule_b;
    switch (rank_a * 3 + rank_b) {
    case 0:
        sphere_a = b3Shape_GetSphere(a), sphere_b = b3Shape_GetSphere(b);
        b3CollideSpheres(&m, B3_MAX_MANIFOLD_POINTS, &sphere_a, &sphere_b, b_in_a);
        break;
    case 3:
        capsule_a = b3Shape_GetCapsule(a), sphere_b = b3Shape_GetSphere(b);
        b3CollideCapsuleAndSphere(&m, B3_MAX_MANIFOLD_POINTS, &capsule_a, &sphere_b, b_in_a);
        break;
    case 4:
        capsule_a = b3Shape_GetCapsule(a), capsule_b = b3Shape_GetCapsule(b);
        b3CollideCapsules(&m, B3_MAX_MANIFOLD_POINTS, &capsule_a, &capsule_b, b_in_a);
        break;
    case 6:
        sphere_b = b3Shape_GetSphere(b);
        b3CollideHullAndSphere(&m, B3_MAX_MANIFOLD_POINTS, b3Shape_GetHull(a), &sphere_b, b_in_a, &simplex);
        break;
    case 7:
        capsule_b = b3Shape_GetCapsule(b);
        b3CollideHullAndCapsule(&m, B3_MAX_MANIFOLD_POINTS, b3Shape_GetHull(a), &capsule_b, b_in_a, &simplex);
        break;
    default:
        b3CollideHulls(&m, B3_MAX_MANIFOLD_POINTS, b3Shape_GetHull(a), b3Shape_GetHull(b), b_in_a, &sat);
        break;
    }
    float deepest = 0.0f;
    for (int i = 0; i < m.pointCount; i++) {
        deepest = fmaxf(deepest, -points[i].separation);
    }
    *depth = deepest;
    return true;
}

/* Whether a copy set on the pose (p, q) lies inside one of this side's agents, shape against
   shape where the two collide, deeper than Box3D's linear slop. Two pictures of a resting
   contact differ by the copy grid (SYNC_SOFT_HOLD_M, 3.4 mm) and by how each world settled its
   pile: under the slop that is a resting contact, not an overlap. The depth is the manifold's,
   not a shrunk shape's: pulling a hull's corners in by the slop moves its faces in by less (a
   cube's by slop / sqrt(3)). A pair the manifold cannot measure counts as inside once the boxes
   meet. */
static bool lands_in_agent(const body_sync_t *s, b3BodyId copy, const float p[3], const float q[4]) {
    if (s->agent_count == 0) {
        return false;
    }
    b3ShapeId mine[4];
    int own = b3Body_GetShapes(copy, mine, 4);
    b3Vec3 at = b3Body_GetPosition(copy);
    b3AABB box = b3Body_ComputeAABB(copy);
    float half[3] = {0.5f * (box.upperBound.x - box.lowerBound.x), 0.5f * (box.upperBound.y - box.lowerBound.y), 0.5f * (box.upperBound.z - box.lowerBound.z)};
    float off[3] = {0.5f * (box.upperBound.x + box.lowerBound.x) - at.x, 0.5f * (box.upperBound.y + box.lowerBound.y) - at.y, 0.5f * (box.upperBound.z + box.lowerBound.z) - at.z};
    /* Its reach from its origin, whatever the target rotation. */
    float reach = sqrtf(half[0] * half[0] + half[1] * half[1] + half[2] * half[2]) + sqrtf(off[0] * off[0] + off[1] * off[1] + off[2] * off[2]);
    b3WorldTransform target = {{p[0], p[1], p[2]}, quat_of(q)};
    for (uint32_t k = 0; k < s->agent_count; k++) {
        b3BodyId body = s->entries[s->agents[k]].body;
        b3AABB ab = b3Body_ComputeAABB(body);
        const float lo[3] = {ab.lowerBound.x, ab.lowerBound.y, ab.lowerBound.z}, hi[3] = {ab.upperBound.x, ab.upperBound.y, ab.upperBound.z};
        float d2 = 0.0f;
        for (int i = 0; i < 3; i++) {
            float d = p[i] < lo[i] ? lo[i] - p[i] : p[i] > hi[i] ? p[i] - hi[i] : 0.0f;
            d2 += d * d;
        }
        if (d2 > reach * reach) {
            continue;
        }
        b3ShapeId shapes[4];
        if (b3Body_GetShapeCount(copy) > 4 || b3Body_GetShapeCount(body) > 4) {
            return true;
        }
        int n = b3Body_GetShapes(body, shapes, 4);
        b3WorldTransform t = b3Body_GetTransform(body);
        for (int j = 0; j < n; j++) {
            b3Filter f = b3Shape_GetFilter(shapes[j]);
            for (int i = 0; i < own; i++) {
                b3Filter g = b3Shape_GetFilter(mine[i]);
                if (b3Shape_IsSensor(shapes[j]) || b3Shape_IsSensor(mine[i]) || (f.categoryBits & g.maskBits) == 0 || (g.categoryBits & f.maskBits) == 0) {
                    continue;
                }
                float depth;
                if (!overlap_depth(shapes[j], t, mine[i], target, &depth) || depth > B3_LINEAR_SLOP) {
                    return true;
                }
            }
        }
    }
    return false;
}

static void pose_now(b3BodyId body, float position[3], float rotation[4]) {
    b3Vec3 p = b3Body_GetPosition(body);
    b3Quat q = b3Body_GetRotation(body);
    position[0] = p.x;
    position[1] = p.y;
    position[2] = p.z;
    rotation[0] = q.v.x;
    rotation[1] = q.v.y;
    rotation[2] = q.v.z;
    rotation[3] = q.s;
}

/* The fastest the other body of a contact closed on `target` at a point that acted in the last
   step, before the solve (sync_soft_pushed): the relative normal speed Box3D kept, less the
   target's own part of it at the velocities the apply left it with (a soft copy falling onto a
   still character is no push of hers). */
static float approach_speed(const body_sync_t *s, const b3ContactData *contact, b3BodyId target) {
    uint32_t slot = hash_get(s, target);
    const entry_t *t = slot < BODY_SYNC_BODIES_MAX ? &s->entries[slot] : NULL;
    bool target_b = B3_ID_EQUALS(b3Shape_GetBody(contact->shapeIdB), target);
    float fastest = 0.0f;
    for (int m = 0; m < contact->manifoldCount; m++) {
        b3Vec3 n = contact->manifolds[m].normal;
        for (int p = 0; p < contact->manifolds[m].pointCount; p++) {
            const b3ManifoldPoint *point = &contact->manifolds[m].points[p];
            if (point->totalNormalImpulse <= 0.0f) {
                continue;
            }
            /* normalVelocity is n.(vB - vA) at the point, n from A to B. */
            float own = 0.0f;
            if (t) {
                b3Vec3 r = target_b ? point->anchorB : point->anchorA;
                float v[3] = {t->step_v[0] + t->step_w[1] * r.z - t->step_w[2] * r.y, t->step_v[1] + t->step_w[2] * r.x - t->step_w[0] * r.z,
                              t->step_v[2] + t->step_w[0] * r.y - t->step_w[1] * r.x};
                own = n.x * v[0] + n.y * v[1] + n.z * v[2];
            }
            float approach = target_b ? own - point->normalVelocity : -(point->normalVelocity + own);
            fastest = approach > fastest ? approach : fastest;
        }
    }
    return fastest;
}

/* Whether a contact pushed its bodies apart at any point in the last step. */
static bool acted(const b3ContactData *contact) {
    for (int m = 0; m < contact->manifoldCount; m++) {
        for (int p = 0; p < contact->manifolds[m].pointCount; p++) {
            if (contact->manifolds[m].points[p].totalNormalImpulse > 0.0f) {
                return true;
            }
        }
    }
    return false;
}

/* Moves the physics pose and lets the render offset keep the drawn pose where it was. */
static void place(entry_t *e, const float position[3], const float rotation[4]) {
    float p0[3], q0[4];
    pose_now(e->body, p0, q0);
    b3Body_SetTransform(e->body, (b3Vec3){position[0], position[1], position[2]}, quat_of(rotation));
    if (e->shown) {
        sync_offset_absorb(&e->offset, p0, q0, position, rotation, e->lost ? SYNC_OFFSET_LOST_M : SYNC_OFFSET_SNAP_M);
    }
    e->placed = true;
    e->shown = true;
}

/* A body that turned local continues as authority says: the motion of the record that moved
   it, or of the state received, or of the bind; the pose and velocities of the newest state
   received, the drawn pose kept by the render offset. */
static void resume(body_sync_t *s, uint32_t slot, bool was_copy) {
    sync_authority_body_t *a = &s->authority.bodies[slot];
    entry_t *e = &s->entries[slot];
    const sync_received_t *in = &s->receiver.bodies[slot];
    e->copy_mode = false;
    a->resume = false;
    sync_masks_restore(&e->masks, e->body);
    if (was_copy) {
        e->took = true;
        e->took_pending = a->claim_pending;
        e->took_any = sync_playback_newest(&s->playback, (uint16_t)slot, &e->took_tick);
    }
    if (!in->has && !was_copy && !a->take_motion) {
        return;
    }
    bool kinematic, was_simulated = b3Body_GetType(e->body) == b3_dynamicBody;
    uint8_t locks;
    sync_authority_resume_motion(a, in->has && (in->known & SYNC_RARE_MOTION) != 0, in->rare.kinematic, in->rare.locks,
                                 e->bound_type == b3_kinematicBody, locks_bits(e->bound_locks), &kinematic, &locks);
    b3Body_SetType(e->body, kinematic ? b3_kinematicBody : b3_dynamicBody);
    b3Body_SetMotionLocks(e->body, locks_of(locks));
    sync_authority_applied(a, kinematic, locks);
    /* A body already local keeps the velocities it simulated (a throw made since the grant);
       only a copy taking over continues from the state received. */
    bool was_soft = e->soft;
    e->soft = false;
    /* A soft copy taken over keeps its own simulation: it is what this player saw, within the
       dead zone or the pause of its owner. */
    /* A take by interaction goes on from what this side shows: the newest state received may be
       older than the copy's pose (a body this side owned and returned, whose new rest pose from
       the host has not come yet). The host's take-back goes on from its own simulation alike. */
    bool keep_own = e->keep_own || (was_copy && a->hold == BODY_SYNC_HOLD_UNTIL_REST);
    e->keep_own = false;
    /* A copy this side simulated (driven dynamic) within the dead zone of that state keeps its
       pose, where its neighbours here let it lie: a body returned at rest, whose owner's pose sits
       a fraction of a millimetre off them, would drop or be pushed out as it is placed, waking the
       heap. The difference is the layer's, and nobody sees it; the velocities are the owner's. */
    bool keep_pose = false;
    if (in->has && was_copy && was_simulated && !keep_own) {
        float p[3], q[4], turn[3];
        pose_now(e->body, p, q);
        sync_turn_between(q, in->newest.rotation, turn);
        float dx = in->newest.position[0] - p[0], dy = in->newest.position[1] - p[1], dz = in->newest.position[2] - p[2];
        keep_pose = sqrtf(dx * dx + dy * dy + dz * dz) <= SYNC_SOFT_DEAD_M && sqrtf(turn[0] * turn[0] + turn[1] * turn[1] + turn[2] * turn[2]) <= SYNC_SOFT_DEAD_RAD;
    }
    if (in->has && was_copy && !was_soft && !keep_own) {
        if (!keep_pose) {
            place(e, in->newest.position, in->newest.rotation);
        }
        b3Body_SetLinearVelocity(e->body, (b3Vec3){in->velocity[0], in->velocity[1], in->velocity[2]});
        b3Body_SetAngularVelocity(e->body, (b3Vec3){in->angular[0], in->angular[1], in->angular[2]});
    }
}

/* Whether a soft copy touches a body this side simulates (its own character, its thrown body). */
/* Whether a body this side simulates and that moves faster than a lean touches it. */
static bool touches_local(const body_sync_t *s, b3BodyId body) {
    b3ContactData contacts[16];
    int n = b3Body_GetContactData(body, contacts, 16);
    for (int i = 0; i < n; i++) {
        bool touching = false;
        for (int m = 0; m < contacts[i].manifoldCount && !touching; m++) {
            touching = contacts[i].manifolds[m].pointCount > 0;
        }
        if (!touching || !b3Shape_IsValid(contacts[i].shapeIdA) || !b3Shape_IsValid(contacts[i].shapeIdB)) {
            continue;
        }
        b3BodyId a = b3Shape_GetBody(contacts[i].shapeIdA), b = b3Shape_GetBody(contacts[i].shapeIdB);
        b3BodyId toucher = B3_ID_EQUALS(a, body) ? b : a;
        uint32_t other = hash_get(s, toucher);
        b3Vec3 v = b3Body_GetLinearVelocity(toucher);
        /* A body of this side's that only leans on it (a character standing still) pauses nothing:
           the pull would wait for ever. */
        if (other < BODY_SYNC_BODIES_MAX && sync_authority_local(&s->authority, &s->authority.bodies[other]) &&
            v.x * v.x + v.y * v.y + v.z * v.z > SYNC_SOFT_TOUCH_SPEED * SYNC_SOFT_TOUCH_SPEED) {
            return true;
        }
    }
    return false;
}

/* The second layer's pause: the host's answer to this side's touch reaches its playback one
   round trip plus the playback delay later (2 ticks of margin for the host's step and the
   acknowledgement's wait), bounded by the layer's floor and cap. */
static uint16_t soft_pause(const body_sync_t *s) {
    return sync_soft_pause(sync_sender_round_trip(&s->sender, BODY_SYNC_SEAT_HOST), s->playback.timelines[BODY_SYNC_SEAT_HOST].delay);
}

/* A soft copy this tick: the part decides (sync_soft.h), the shell performs. */
static void drive_soft(body_sync_t *s, uint32_t slot, const float p[3], const float q[4], const sync_play_t *play) {
    entry_t *e = &s->entries[slot];
    sync_soft_in_t in;
    memset(&in, 0, sizeof(in));
    memcpy(in.position, p, sizeof(in.position));
    memcpy(in.rotation, q, sizeof(in.rotation));
    b3Vec3 v = b3Body_GetLinearVelocity(e->body), w = b3Body_GetAngularVelocity(e->body);
    in.velocity[0] = v.x;
    in.velocity[1] = v.y;
    in.velocity[2] = v.z;
    in.angular[0] = w.x;
    in.angular[1] = w.y;
    in.angular[2] = w.z;
    in.asleep = !b3Body_IsAwake(e->body);
    /* A push pauses the pull as a moving touch does: a slow one stops its pusher in the step. */
    in.touches_local = e->pushed || (!in.asleep && touches_local(s, e->body));
    in.max_speed = s->config.max_speed;
    in.max_spin = s->config.max_spin;
    in.pause_ticks = soft_pause(s);
    in.owner_rests = e->owner_rests;
    sync_soft_out_t out;
    sync_soft_decide(&e->soft_state, &in, e->soft_hit, play, &out);
    e->soft_hit = false;
    e->soft_paused = out.paused;
    switch (out.action) {
    case SYNC_SOFT_NONE:
        break;
    case SYNC_SOFT_PLACE:
        place(e, play->position, play->rotation);
        /* fall through: the owner's velocities go with the placement */
    case SYNC_SOFT_VELOCITY:
        b3Body_SetLinearVelocity(e->body, (b3Vec3){out.velocity[0], out.velocity[1], out.velocity[2]});
        b3Body_SetAngularVelocity(e->body, (b3Vec3){out.angular[0], out.angular[1], out.angular[2]});
        break;
    }
}

/* A copy this tick: its hold, then a move to its playback pose within the seam's speeds. */
static void drive_copy(body_sync_t *s, uint32_t slot) {
    sync_authority_body_t *a = &s->authority.bodies[slot];
    entry_t *e = &s->entries[slot];
    const sync_received_t *in = &s->receiver.bodies[slot];
    /* A body back from this side: what its owner sent before the take is stale once the take was
       confirmed (entry_t.took), and is dropped before anything here reads the playback. */
    if (!e->copy_mode && e->took && e->took_any && !e->took_pending) {
        sync_playback_forget_until(&s->playback, (uint16_t)slot, e->took_tick);
    }
    sync_play_t play;
    sync_playback_sample(&s->playback, (uint16_t)slot, &play);
    /* A body its owner moves fast is in flight there (an owner's own throw has no hold to say
       so): it is driven along the owner's path, and soft again once it has slowed. */
    float owner_speed = sqrtf(play.velocity[0] * play.velocity[0] + play.velocity[1] * play.velocity[1] + play.velocity[2] * play.velocity[2]);
    e->owner_flying = play.known && (e->owner_flying ? owner_speed >= SYNC_SOFT_LAND_SPEED : owner_speed >= SYNC_SOFT_FLIGHT_SPEED);
    e->owner_speed = play.known ? owner_speed : 0.0f;
    bool owner_rests = sync_soft_owner_rests(&e->owner_still, &play);
    /* A body this side returned at rest has no state of the host's yet (what was played before
       the take is stale and dropped): the host holds the very pose it shows, so it rests there. */
    if (play.known) {
        e->returned_at_rest = false;
    } else if (e->returned_at_rest) {
        float p[3], q[4];
        pose_now(e->body, p, q);
        sync_soft_still_at(&e->owner_still, p, q);
        owner_rests = true;
    }
    e->owner_rests = owner_rests;
    float home_p[3], home_q[4], home_e[3], home_r[3];
    pose_now(e->body, home_p, home_q);
    for (int i = 0; i < 3; i++) {
        home_e[i] = play.position[i] - home_p[i];
    }
    sync_turn_between(home_q, play.rotation, home_r);
    /* A soft copy joining the rest hold inside the dead zone is set on the pose exactly, a move
       too small for the offset to carry; one further off is placed, the offset taking it. */
    bool home = sqrtf(home_e[0] * home_e[0] + home_e[1] * home_e[1] + home_e[2] * home_e[2]) <= SYNC_SOFT_DEAD_M &&
                sqrtf(home_r[0] * home_r[0] + home_r[1] * home_r[1] + home_r[2] * home_r[2]) <= SYNC_SOFT_DEAD_RAD;
    bool rest_hold = e->light && sync_soft_rest_hold(owner_rests, &e->soft_state, e->soft, e->marked == s->now + 1u);
    /* In flight a copy stays soft while a body this side simulates presses it or is about to
       meet it: the owner may move it that fast only because that body's copy there pushes it,
       and placed kinematic on the owner's older path it would land inside that body and fling
       it. Marks from copies (its own flight among them) do not count, or no flight is driven. */
    /* Another seat's throw, held until it settles, is driven on its owner's path at any speed,
       with the character bits: it strikes this side's characters as the flight rule says, soft
       only near this side's agents, where a kinematic body would fling them. */
    uint8_t hold_in = a->has_in ? a->in_hold : a->hold;
    bool settling = hold_in == BODY_SYNC_HOLD_UNTIL_SETTLED;
    bool flight_driven = (e->owner_flying || settling) && e->marked_own != s->now + 1u && !e->leaned;
    /* A driven copy never goes where this side's agent is: placed or swept onto a pose inside one
       it would fling it. It stays soft that step; the contact parts them, and it is driven again
       once its pose is clear. */
    bool driven = flight_driven || rest_hold;
    bool lands = e->light && driven && play.known && lands_in_agent(s, e->body, play.position, play.rotation);
    sync_hold_kind_t kind = sync_authority_hold_kind(&s->authority, a, in->has && (in->known & SYNC_RARE_MOTION) && in->rare.kinematic,
                                                     e->light && (!driven || lands));
    bool dynamic = kind != SYNC_HOLD_DRIVEN, soft = kind == SYNC_HOLD_SOFT;
    e->rest_held = rest_hold && kind == SYNC_HOLD_DRIVEN;
    /* A layer copy keeps the character bits, soft, in the rest hold or in flight (a throw held
       until settled among them): this side's own character pushes it, meets it at rest or is
       struck by it; any other driven LIGHT copy (the layer off, or held) lets the character
       through. */
    /* On the host a guest's body owned by interaction keeps them too: the host's character meets
       it, and that touch is how the host's agents come first. So does a guest's throw the host
       holds dynamic: its flight strikes the host's character as it strikes the thrower's copy. */
    bool owned_there = is_host(s) && e->light && a->seat != BODY_SYNC_SEAT_HOST &&
                       ((s->authority.mode == SYNC_POLICY_INTERACTION && hold_in == BODY_SYNC_HOLD_UNTIL_REST) ||
                        (hold_in == BODY_SYNC_HOLD_UNTIL_SETTLED && kind == SYNC_HOLD_CORRECTED));
    bool bits = soft || owned_there ||
                (e->light && sync_authority_hold_kind(&s->authority, a, in->has && (in->known & SYNC_RARE_MOTION) && in->rare.kinematic, true) ==
                                 SYNC_HOLD_SOFT);
    e->layer = bits;
    if (bits) {
        sync_masks_restore(&e->masks, e->body);
    } else if (e->body_class == BODY_SYNC_CLASS_LIGHT && e->copy_mode) {
        sync_masks_clear(&e->masks, e->body, s->config.character_category);
    }
    /* A take that turns back into a copy while its claim still waited lost to the owner: what
       this side simulated since is the loser's picture, and the owner's is the playback pose. It
       is corrected at once, as a copy that strays (sync_soft_enter); a pause would let the loser's
       picture run on a round trip more. A touch of this side's pauses it as any copy's. Its
       placement keeps the drawn pose however far the pictures parted (SYNC_OFFSET_LOST_M). */
    bool lost = !e->copy_mode && e->took && e->took_pending;
    e->lost = lost;
    if (lost) {
        float d[3];
        for (int i = 0; i < 3; i++) {
            d[i] = play.known ? home_p[i] + e->offset.position[i] - play.position[i] : 0.0f;
        }
        e->lost_count++;
        e->lost_hand_back = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        e->lost_glide = true;
    }
    if (soft && (!e->soft || !e->copy_mode)) {
        sync_soft_enter(&e->soft_state, !e->copy_mode && !lost, soft_pause(s));
        e->soft_hit = false;
    }
    bool from_local = !e->copy_mode;
    /* A driven copy's velocity is its drive's sweep to the next pose (a whole step of the copy grid
       in one tick, in the rest hold): turned soft, it starts from its owner's motion instead. */
    bool soft_from_driven = soft && e->copy_mode && b3Body_GetType(e->body) == b3_kinematicBody;
    if (!e->copy_mode || (b3Body_GetType(e->body) == b3_dynamicBody) != dynamic) {
        /* A copy starts with a placement at its playback pose; a body that was local has a
           drawn pose, so that placement goes to the render offset instead of jumping. */
        if (!e->copy_mode) {
            e->placed = false;
            e->took = false;
            if (e->body_class == BODY_SYNC_CLASS_LIGHT && !bits) {
                sync_masks_clear(&e->masks, e->body, s->config.character_category);
            }
        }
        e->copy_mode = true;
        b3Body_SetType(e->body, dynamic ? b3_dynamicBody : b3_kinematicBody);
    }
    if (soft_from_driven) {
        b3Vec3 v = play.known ? (b3Vec3){play.velocity[0], play.velocity[1], play.velocity[2]} : b3Vec3_zero;
        b3Vec3 w = play.known ? (b3Vec3){play.angular[0], play.angular[1], play.angular[2]} : b3Vec3_zero;
        b3Body_SetLinearVelocity(e->body, v);
        b3Body_SetAngularVelocity(e->body, w);
    }
    if (soft && from_local) {
        e->placed = true; /* it goes on from its own simulation: no placement */
    } else if (!soft && e->soft && rest_hold && home) {
        b3Body_SetTransform(e->body, (b3Vec3){play.position[0], play.position[1], play.position[2]}, quat_of(play.rotation));
    } else if (!soft && e->soft) {
        /* Soft to driven is a placement, the offset taking it: a kinematic sweep from where this
           side's prediction left it would bat the soft copies in its path at the sweep's speed. */
        e->placed = false;
    }
    e->soft = soft;
    e->soft_paused = false; /* drive_soft says otherwise */
    /* A kinematic copy turns to its owner's rotation, which a lock would stop; a dynamic one
       keeps its owner's locks as they change (a character stays upright). */
    b3MotionLocks locks = dynamic && (in->known & SYNC_RARE_MOTION) ? locks_of(in->rare.locks) : (b3MotionLocks){0};
    if (locks_bits(b3Body_GetMotionLocks(e->body)) != locks_bits(locks)) {
        b3Body_SetMotionLocks(e->body, locks);
    }
    if (!play.known) {
        if (b3Body_IsAwake(e->body) && !dynamic) {
            stop(e->body); /* nothing to play yet: stay put */
        }
        return;
    }
    float p[3], q[4];
    pose_now(e->body, p, q);
    if (soft) {
        drive_soft(s, slot, p, q, &play);
        return;
    }
    /* A copy that would need more than the seam's speeds to reach its pose in one step is placed
       there, the render offset taking the jump; a dynamic one also when its error passes the
       offset's reach, since its correction is bounded far below max_speed. */
    float err[3] = {play.position[0] - p[0], play.position[1] - p[1], play.position[2] - p[2]};
    bool far = sqrtf(err[0] * err[0] + err[1] * err[1] + err[2] * err[2]) > SYNC_OFFSET_SNAP_M;
    bool reachable = !(dynamic && far) && sync_drive_reachable(p, q, &play, s->dt, s->config.max_speed, s->config.max_spin);
    /* A copy in the rest hold within a step of the copy grid of its pose lies where its owner rests:
       a body returned at rest meets the host's first state of it up to a step off, and a first
       placement would move it by that step out of the rest it was handed back in. */
    bool held_within_step = rest_hold && !dynamic && sqrtf(home_e[0] * home_e[0] + home_e[1] * home_e[1] + home_e[2] * home_e[2]) <= SYNC_SOFT_HOLD_M &&
                            sqrtf(home_r[0] * home_r[0] + home_r[1] * home_r[1] + home_r[2] * home_r[2]) <= SYNC_SOFT_HOLD_RAD;
    e->placed = e->placed || held_within_step;
    if (play.jumped || !e->placed || !reachable) {
        place(e, play.position, play.rotation);
        if (dynamic) {
            b3Body_SetLinearVelocity(e->body, (b3Vec3){play.velocity[0], play.velocity[1], play.velocity[2]});
            b3Body_SetAngularVelocity(e->body, (b3Vec3){play.angular[0], play.angular[1], play.angular[2]});
        } else {
            stop(e->body);
        }
        return;
    }
    if (dynamic) {
        /* A light corrected copy whose owner rests within a step of the copy grid of where it lies here
           stays as this world settled it: the owner's exact rest pose replacing its rounded one
           would be a correction of millimetres at a step's speed into the bodies it lies on. */
        if (kind == SYNC_HOLD_CORRECTED && e->light && owner_rests && sqrtf(home_e[0] * home_e[0] + home_e[1] * home_e[1] + home_e[2] * home_e[2]) <= SYNC_SOFT_HOLD_M &&
            sqrtf(home_r[0] * home_r[0] + home_r[1] * home_r[1] + home_r[2] * home_r[2]) <= SYNC_SOFT_HOLD_RAD) {
            return;
        }
        float v[3], w[3];
        sync_drive_dynamic(p, q, &play, s->dt, v, w);
        b3Body_SetLinearVelocity(e->body, (b3Vec3){v[0], v[1], v[2]});
        b3Body_SetAngularVelocity(e->body, (b3Vec3){w[0], w[1], w[2]});
        /* Locked axes ignore a spin: their part of the owner's rotation is set, as the owner sets
           it; the free axes follow the spin above. */
        float turned[4];
        uint8_t locked = (uint8_t)((locks_bits(locks) >> 3) & 7u);
        if (locked && sync_drive_locked_turn(q, play.rotation, locked, turned)) {
            b3Body_SetTransform(e->body, (b3Vec3){p[0], p[1], p[2]}, quat_of(turned));
        }
    } else if (play.at_rest) {
        /* At rest the worlds agree to the bit: the copy is set on the rest pose, not driven
           (a sleeping kinematic body ignores a target that small). */
        if (memcmp(p, play.position, sizeof(p)) != 0 || memcmp(q, play.rotation, sizeof(q)) != 0) {
            place(e, play.position, play.rotation);
        }
        if (b3Body_IsAwake(e->body)) {
            stop(e->body);
        }
    } else if (play.frozen || (rest_hold && sqrtf(home_e[0] * home_e[0] + home_e[1] * home_e[1] + home_e[2] * home_e[2]) <= SYNC_SOFT_HOLD_M &&
                                sqrtf(home_r[0] * home_r[0] + home_r[1] * home_r[1] + home_r[2] * home_r[2]) <= SYNC_SOFT_HOLD_RAD)) {
        /* A still, awake owner's pose is rounded to the copy grid and wanders by a step of it: a
           copy in the rest hold stays where it is until the owner is a step away. */
        if (b3Body_IsAwake(e->body)) {
            stop(e->body);
        }
    } else {
        b3Body_SetTargetTransform(e->body, (b3WorldTransform){{play.position[0], play.position[1], play.position[2]}, quat_of(play.rotation)}, s->dt, true);
    }
}

static void mark(marking_t *m, b3BodyId body) {
    uint32_t slot = hash_get(m->s, body);
    entry_t *e = slot < BODY_SYNC_BODIES_MAX ? &m->s->entries[slot] : NULL;
    if (e && e->light && m->own) {
        e->marked_own = m->s->now + 1u;
    }
    if (e && e->light && e->marked != m->s->now + 1u) {
        e->marked = m->s->now + 1u;
        m->queue[m->count++] = (uint16_t)slot;
    }
}

static bool mark_near(b3ShapeId shape, void *context) {
    mark(context, b3Shape_GetBody(shape));
    return true;
}

/* Whether a body of this side's moves only as a copy in the rest hold under it drags it: that copy
   closes its owner's creep a step of the copy grid in one tick (0.2 m/s and 1.2 rad/s a step),
   which drags what lies on it past a lean's speed, and taken for this side's motion the drag
   would turn the held copy soft under it. A body dragged or shoved by a contact moves no faster
   than the surface that moves it (it slips behind it), so one no faster than that copy's fastest
   point is carried: the copy's centre speed plus its spin times its own half-diagonal, whatever
   the size of the body it moves. This side's own motion past it still marks. A side contact is
   counted too: a step that shoves a neighbour also spins it, and read along the normal alone
   that shove would turn a held heap soft around a body that only leans on it. */
static bool carried_by_hold_step(const body_sync_t *s, b3BodyId body, b3Vec3 v, b3Vec3 w, float extent) {
    b3ContactData contacts[16];
    int n = b3Body_GetContactData(body, contacts, 16);
    for (int i = 0; i < n; i++) {
        if (!b3Shape_IsValid(contacts[i].shapeIdA) || !b3Shape_IsValid(contacts[i].shapeIdB)) {
            continue;
        }
        b3BodyId a = b3Shape_GetBody(contacts[i].shapeIdA), b = b3Shape_GetBody(contacts[i].shapeIdB);
        b3BodyId under = B3_ID_EQUALS(a, body) ? b : a;
        uint32_t slot = hash_get(s, under);
        if (slot >= BODY_SYNC_BODIES_MAX || !s->entries[slot].rest_held || !s->entries[slot].copy_mode || b3Body_GetType(under) != b3_kinematicBody) {
            continue;
        }
        b3AABB box = b3Body_ComputeAABB(under);
        b3Vec3 half = {0.5f * (box.upperBound.x - box.lowerBound.x), 0.5f * (box.upperBound.y - box.lowerBound.y), 0.5f * (box.upperBound.z - box.lowerBound.z)};
        float drag = b3Length(b3Body_GetLinearVelocity(under)) + b3Length(b3Body_GetAngularVelocity(under)) * sqrtf(half.x * half.x + half.y * half.y + half.z * half.z);
        if (drag > 0.0f && b3Length(v) + b3Length(w) * extent <= drag) {
            return true;
        }
    }
    return false;
}

/* Every moving body (this side's own, soft copies set going, a copy in flight) marks the light
   copies within reach of its next steps, and through them every light copy they touch: those
   leave the rest hold before the contact, so a pile meets what comes at it as one simulated
   whole, never with a kinematic neighbour as a wall. */
static void mark_reached(body_sync_t *s) {
    if (s->config.light_mass_max <= 0.0f || sync_authority_is_host(&s->authority)) {
        return;
    }
    marking_t *m = &s->marking;
    m->s = s;
    m->count = 0;
    m->whole = false;
    m->own = false;
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        entry_t *e = &s->entries[slot];
        if (!s->authority.bodies[slot].bound || !b3Body_IsValid(e->body) || !b3Body_IsAwake(e->body) || b3Body_GetType(e->body) == b3_staticBody) {
            continue;
        }
        /* A soft copy marks only while something of this side's set it going (its pause): moved
           by the pull it is the correction, and marking from it would keep a whole pile soft and
           pulling, never resting, around a heroine who stands in it. */
        if (e->soft && e->copy_mode && e->soft_state.pause == 0) {
            continue;
        }
        /* A copy in the rest hold moves only on its owner's still or creeping pose, closing a step
           of the copy grid in one tick (0.2 m/s): no motion of this side's, and taken for one it
           would turn the resting heap soft around it. */
        if (e->rest_held && e->copy_mode) {
            continue;
        }
        b3Vec3 v = b3Body_GetLinearVelocity(e->body), w = b3Body_GetAngularVelocity(e->body);
        b3AABB box = b3Body_ComputeAABB(e->body);
        b3Vec3 half = {0.5f * (box.upperBound.x - box.lowerBound.x), 0.5f * (box.upperBound.y - box.lowerBound.y), 0.5f * (box.upperBound.z - box.lowerBound.z)};
        float reach, extent = sqrtf(half.x * half.x + half.y * half.y + half.z * half.z);
        if (!sync_soft_mover((const float[3]){v.x, v.y, v.z}, (const float[3]){w.x, w.y, w.z}, extent, s->dt, &reach)) {
            continue;
        }
        bool own = sync_authority_local(&s->authority, &s->authority.bodies[slot]);
        if (own && carried_by_hold_step(s, e->body, v, w, extent)) {
            continue;
        }
        m->whole = m->whole || !(e->soft && e->copy_mode);
        m->own = own;
        box.lowerBound = (b3Vec3){box.lowerBound.x - reach, box.lowerBound.y - reach, box.lowerBound.z - reach};
        box.upperBound = (b3Vec3){box.upperBound.x + reach, box.upperBound.y + reach, box.upperBound.z + reach};
        b3World_OverlapAABB(s->config.world, box, b3DefaultQueryFilter(), mark_near, m);
    }
    m->own = false;
    for (uint32_t k = 0; k < m->count; k++) {
        b3ContactData contacts[16];
        b3BodyId body = s->entries[m->queue[k]].body;
        int n = b3Body_GetContactData(body, contacts, 16);
        for (int i = 0; i < n; i++) {
            if (b3Shape_IsValid(contacts[i].shapeIdA) && b3Shape_IsValid(contacts[i].shapeIdB)) {
                mark(m, b3Shape_GetBody(contacts[i].shapeIdA));
                mark(m, b3Shape_GetBody(contacts[i].shapeIdB));
            }
        }
        /* Copies held kinematic share no contacts: their neighbours are found by touch distance,
           so the whole resting heap turns soft at once, as the host's island wakes whole. Only a
           walk a real mover started does so: a soft copy moves by the pull or by settling as it
           leaves the hold, and a heap it woke whole would settle the same way again, never at
           rest. */
        if (m->whole && b3Body_GetType(body) == b3_kinematicBody) {
            b3AABB box = b3Body_ComputeAABB(body);
            box.lowerBound = (b3Vec3){box.lowerBound.x - SYNC_SOFT_TOUCH_M, box.lowerBound.y - SYNC_SOFT_TOUCH_M, box.lowerBound.z - SYNC_SOFT_TOUCH_M};
            box.upperBound = (b3Vec3){box.upperBound.x + SYNC_SOFT_TOUCH_M, box.upperBound.y + SYNC_SOFT_TOUCH_M, box.upperBound.z + SYNC_SOFT_TOUCH_M};
            b3World_OverlapAABB(s->config.world, box, b3DefaultQueryFilter(), mark_near, m);
        }
    }
}

/* A throw leaves its thrower's hand inside her reach: on another side her copy and the throw's play
   from different states, and one would strike the other. Each side keeps a throw (released in
   flight, SYNC_SOFT_FLIGHT_SPEED or faster) off its last carrier until it has travelled her radius
   and its own half-diagonal, each with Box3D's speculative distance, at its release speed: at the
   flight's 4 m/s floor about 0.61 m for a 0.3 m capsule and a 0.3 m box, 10 ticks; a drop is met
   as any body. */
static void release_filters(body_sync_t *s) {
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        const sync_authority_body_t *a = &s->authority.bodies[slot];
        entry_t *e = &s->entries[slot];
        if (b3Joint_IsValid(e->release_filter) && (!a->bound || s->now >= e->release_end)) {
            b3DestroyJoint(e->release_filter, false);
            e->release_filter = b3_nullJointId;
        }
        if (!a->bound || !b3Body_IsValid(e->body)) {
            e->carrier = BODY_SYNC_ID_NONE;
            continue;
        }
        bool local = sync_authority_local(&s->authority, a);
        body_sync_id_t holder = local || !a->has_in ? a->holder : a->in_holder;
        uint8_t hold = local || !a->has_in ? a->hold : a->in_hold;
        if (holder != BODY_SYNC_ID_NONE) {
            e->carrier = holder;
            continue;
        }
        body_sync_id_t carrier = e->carrier;
        if (hold == BODY_SYNC_HOLD_UNTIL_RELEASE) {
            continue; /* let go, its throw's hold not yet heard */
        }
        e->carrier = BODY_SYNC_ID_NONE;
        const sync_authority_body_t *c = sync_authority_body_const(&s->authority, carrier);
        if (hold != BODY_SYNC_HOLD_UNTIL_SETTLED || !c || b3Joint_IsValid(e->release_filter)) {
            continue;
        }
        b3BodyId her = s->entries[body_sync_id_slot(carrier)].body;
        b3Vec3 v = b3Body_GetLinearVelocity(e->body);
        /* A copy's own velocity is its drive's catching up; the throw's is its owner's. */
        float speed = local ? sqrtf(v.x * v.x + v.y * v.y + v.z * v.z) : e->owner_speed;
        if (!b3Body_IsValid(her) || !(speed >= SYNC_SOFT_FLIGHT_SPEED)) {
            continue;
        }
        /* Box3D's shape bounds carry its speculative distance on every side. */
        b3AABB hb = b3Body_ComputeAABB(her), bb = b3Body_ComputeAABB(e->body);
        float radius = 0.5f * fmaxf(hb.upperBound.x - hb.lowerBound.x, hb.upperBound.z - hb.lowerBound.z);
        b3Vec3 d = {bb.upperBound.x - bb.lowerBound.x, bb.upperBound.y - bb.lowerBound.y, bb.upperBound.z - bb.lowerBound.z};
        float clear = radius + 0.5f * sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
        b3FilterJointDef def = b3DefaultFilterJointDef();
        def.base.bodyIdA = e->body;
        def.base.bodyIdB = her;
        e->release_filter = b3CreateFilterJoint(s->config.world, &def);
        e->release_end = s->now + (uint32_t)ceilf(clear / (speed * s->dt));
    }
}

void body_sync_apply(body_sync_t *s, uint32_t tick) {
    s->now = tick;
    s->authority.now = tick;
    sync_playback_advance(&s->playback, tick);
    mark_reached(s);
    gather_agents(s);
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        sync_authority_body_t *a = &s->authority.bodies[slot];
        entry_t *e = &s->entries[slot];
        if (!a->bound || !b3Body_IsValid(e->body)) {
            continue;
        }
        sync_offset_decay(&e->offset);
        /* The glide is over once what is left of the hand-back is inside the layer's dead zone. */
        const float *o = e->offset.position;
        if (e->lost_glide && (sync_authority_local(&s->authority, a) || sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]) < SYNC_SOFT_DEAD_M)) {
            e->lost_glide = false;
        }
        if (sync_authority_local(&s->authority, a)) {
            if (e->copy_mode || a->resume) {
                resume(s, slot, e->copy_mode);
            }
            e->shown = true; /* its pose is drawn from here on */
            if (e->took && e->took_pending && !a->claim_pending) {
                /* The claim was granted while it simulated: what the owner sent until now, its
                   state before the grant, is stale from here on. */
                e->took_pending = false;
                e->took_any = sync_playback_newest(&s->playback, (uint16_t)slot, &e->took_tick) || e->took_any;
            }
            continue;
        }
        drive_copy(s, slot);
        e->lost = false;
        e->shown = true; /* drawn from its first apply on, a soft start too: later moves go to the offset */
    }
    release_filters(s);
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        entry_t *e = &s->entries[slot];
        if (s->authority.bodies[slot].bound && b3Body_IsValid(e->body)) {
            b3Vec3 v = b3Body_GetLinearVelocity(e->body), w = b3Body_GetAngularVelocity(e->body);
            e->step_v[0] = v.x, e->step_v[1] = v.y, e->step_v[2] = v.z;
            e->step_w[0] = w.x, e->step_w[1] = w.y, e->step_w[2] = w.z;
        }
    }
}

/* Whether this side may take `other` (sync_policy_takes without the agent): a body of its own
   owned by interaction, or a free light body of the host's. */
static bool takable_here(const body_sync_t *s, b3BodyId other) {
    uint32_t slot = hash_get(s, other);
    if (slot >= BODY_SYNC_BODIES_MAX) {
        return false;
    }
    const sync_authority_body_t *a = &s->authority.bodies[slot];
    if (!a->bound || !a->takable) {
        return false;
    }
    if (sync_authority_local(&s->authority, a)) {
        return a->hold == BODY_SYNC_HOLD_UNTIL_REST || (a->seat == BODY_SYNC_SEAT_HOST && a->holder == BODY_SYNC_ID_NONE);
    }
    const sync_received_t *in = &s->receiver.bodies[slot];
    uint8_t hold = a->has_in ? a->in_hold : a->hold;
    body_sync_id_t holder = a->has_in ? a->in_holder : a->holder;
    bool kinematic = in->has && (in->known & SYNC_RARE_MOTION) && in->rare.kinematic;
    return a->seat == BODY_SYNC_SEAT_HOST && hold == BODY_SYNC_HOLD_UNTIL_RELEASE && holder == BODY_SYNC_ID_NONE && !kinematic;
}

/* A begin-touch of `body` with a body that is neither static nor `ignore`; *untakable when one of
   them is a body this side may not take. */
static bool touched_moving(const body_sync_t *s, const b3ContactEvents *events, b3BodyId body, b3BodyId ignore, bool *untakable) {
    bool any = false;
    *untakable = false;
    for (int i = 0; i < events->beginCount; i++) {
        b3ShapeId a = events->beginEvents[i].shapeIdA, b = events->beginEvents[i].shapeIdB;
        if (!b3Shape_IsValid(a) || !b3Shape_IsValid(b)) {
            continue;
        }
        b3BodyId ba = b3Shape_GetBody(a), bb = b3Shape_GetBody(b);
        b3BodyId other;
        if (B3_ID_EQUALS(ba, body)) {
            other = bb;
        } else if (B3_ID_EQUALS(bb, body)) {
            other = ba;
        } else {
            continue;
        }
        if (b3Body_GetType(other) != b3_staticBody && !B3_ID_EQUALS(other, ignore)) {
            any = true;
            *untakable = *untakable || !takable_here(s, other);
        }
    }
    return any;
}

static void holder_fields(sync_rare_t *rare, body_sync_id_t holder) {
    if (holder == BODY_SYNC_ID_NONE) {
        rare->holder_slot = BODY_SYNC_BODIES_MAX;
        rare->holder_generation_low = 0;
    } else {
        rare->holder_slot = (uint16_t)body_sync_id_slot(holder);
        rare->holder_generation_low = (uint8_t)(body_sync_id_generation(holder) & SYNC_GEN_MASK);
    }
}

/* A local body as the sender wants it: full precision, every rare group's value. */
static void read_local(body_sync_t *s, uint32_t slot, sync_body_input_t *in) {
    sync_authority_body_t *a = &s->authority.bodies[slot];
    entry_t *e = &s->entries[slot];
    b3Vec3 v = b3Body_GetLinearVelocity(e->body);
    b3Vec3 w = b3Body_GetAngularVelocity(e->body);
    bool kinematic = b3Body_GetType(e->body) == b3_kinematicBody;
    uint8_t locks = locks_bits(b3Body_GetMotionLocks(e->body));
    sync_authority_note_motion(&s->authority, a, kinematic, locks);
    sync_record_t *r = &in->state;
    memset(r, 0, sizeof(*r));
    r->slot = (uint16_t)slot;
    r->generation_low = (uint8_t)(a->generation & SYNC_GEN_MASK);
    pose_now(e->body, r->position, r->rotation);
    r->velocity[0] = v.x;
    r->velocity[1] = v.y;
    r->velocity[2] = v.z;
    r->angular[0] = w.x;
    r->angular[1] = w.y;
    r->angular[2] = w.z;
    r->rare.kinematic = kinematic;
    r->rare.locks = locks;
    r->rare.flags = a->flags;
    r->rare.seat = a->seat;
    r->rare.number = a->number;
    r->rare.hold = a->hold;
    holder_fields(&r->rare, a->holder);
    r->rare.data_size = a->data_size;
    memcpy(r->rare.data, a->data, sizeof(r->rare.data));
    r->rare.number = sync_authority_declared_number(&s->authority, a);
    in->present = true;
    /* A quiet body owned by interaction goes out as a rest pose though awake (a pile leaning on a
       character never sleeps): the pose the host continues from at its release. */
    in->asleep = !b3Body_IsAwake(e->body) || (!is_host(s) && a->hold == BODY_SYNC_HOLD_UNTIL_REST && e->quiet);
    in->held = a->holder != BODY_SYNC_ID_NONE;
    in->flying = a->hold == BODY_SYNC_HOLD_UNTIL_SETTLED;
    in->character = e->body_class == BODY_SYNC_CLASS_CHARACTER;
}

/* Host: a guest's body relayed to the other guests from the state that guest sent, stamped
   with its age so it plays on that guest's timeline; too old a state is not relayed. */
static void read_relayed(body_sync_t *s, uint32_t slot, uint32_t tick, sync_body_input_t *in) {
    const sync_authority_body_t *a = &s->authority.bodies[slot];
    const sync_received_t *got = &s->receiver.bodies[slot];
    uint32_t arrived = 0;
    if (!sync_playback_line_tick(&s->playback, got->from, got->tick, &arrived)) {
        return;
    }
    int32_t age = (int32_t)(tick - arrived);
    if (age < 0 || age > SYNC_RELAY_AGE_MAX) {
        return;
    }
    in->state = got->newest;
    in->state.slot = (uint16_t)slot;
    in->state.generation_low = (uint8_t)(a->generation & SYNC_GEN_MASK);
    in->state.origin = (sync_origin_t){.relayed = true, .seat = a->seat, .age = (uint8_t)age};
    memcpy(in->state.velocity, got->velocity, sizeof(got->velocity));
    memcpy(in->state.angular, got->angular, sizeof(got->angular));
    in->state.rare = got->rare;
    body_sync_authority_t now;
    sync_authority_describe(&s->authority, a, &now);
    in->state.rare.seat = now.seat;
    in->state.rare.number = now.number;
    in->state.rare.hold = (uint8_t)now.hold;
    holder_fields(&in->state.rare, now.holder);
    in->state.rare.flags = (got->known & SYNC_RARE_FLAGS) ? got->rare.flags : 0;
    if (!(got->known & SYNC_RARE_DATA)) {
        in->state.rare.data_size = 0;
    }
    in->present = true;
    in->asleep = got->newest.at_rest;
    in->held = now.holder != BODY_SYNC_ID_NONE;
    in->flying = now.hold == BODY_SYNC_HOLD_UNTIL_SETTLED;
    in->character = s->entries[slot].body_class == BODY_SYNC_CLASS_CHARACTER;
}

/* An agent of this side's: a body it simulates that its player's action moves (its character,
   what it carries or threw, what it owns by interaction). */
static bool agent(const body_sync_t *s, uint32_t slot) {
    const sync_authority_body_t *a = &s->authority.bodies[slot];
    if (!sync_authority_local(&s->authority, a)) {
        return false;
    }
    return s->entries[slot].body_class == BODY_SYNC_CLASS_CHARACTER || a->holder != BODY_SYNC_ID_NONE || a->hold == BODY_SYNC_HOLD_UNTIL_SETTLED ||
           a->hold == BODY_SYNC_HOLD_UNTIL_REST;
}

/* Host: a body it took back goes on from its own simulation and counts as set going now. */
static void host_takes_back(body_sync_t *s, uint32_t slot, uint32_t tick) {
    entry_t *e = &s->entries[slot];
    /* The reclaim holds the body until rest on the host, so its resume goes on from the host's own
       simulation of it (resume). */
    if (sync_authority_reclaim(&s->authority, body_sync_id_make(slot, s->authority.bodies[slot].generation)) == BODY_SYNC_OK) {
        e->agent_tick = tick;
        e->still.ticks = 0;
    }
}

/* Authority follows interaction (sync_policy.h): every agent's touching contacts, after the step.
   A guest takes the free light copies its agents set moving; the host marks its own as set going
   by it, and takes back a guest's body its agent touches. */
static void interact(body_sync_t *s, uint32_t tick) {
    if (s->authority.mode != SYNC_POLICY_INTERACTION) {
        return;
    }
    bool host = is_host(s);
    uint32_t owned = host ? 0u : sync_authority_owned(&s->authority, s->config.seat), taken = 0;
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        entry_t *e = &s->entries[slot];
        if (!s->authority.bodies[slot].bound || !b3Body_IsValid(e->body) || !b3Body_IsAwake(e->body) || !agent(s, slot)) {
            continue;
        }
        b3Vec3 v = b3Body_GetLinearVelocity(e->body), w = b3Body_GetAngularVelocity(e->body);
        b3AABB box = b3Body_ComputeAABB(e->body);
        b3Vec3 half = {0.5f * (box.upperBound.x - box.lowerBound.x), 0.5f * (box.upperBound.y - box.lowerBound.y), 0.5f * (box.upperBound.z - box.lowerBound.z)};
        float speed = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z) + sqrtf(w.x * w.x + w.y * w.y + w.z * w.z) * sqrtf(half.x * half.x + half.y * half.y + half.z * half.z);
        /* A push from standstill counts for a character and what it carries, which the game moves
           after the apply; between bodies in a pile it would count their weight on each other. */
        const sync_authority_body_t *ab = &s->authority.bodies[slot];
        bool pusher = e->body_class == BODY_SYNC_CLASS_CHARACTER || ab->holder != BODY_SYNC_ID_NONE;
        b3ContactData contacts[32];
        int n = b3Body_GetContactData(e->body, contacts, 32);
        for (int i = 0; i < n; i++) {
            if (!b3Shape_IsValid(contacts[i].shapeIdA) || !b3Shape_IsValid(contacts[i].shapeIdB)) {
                continue;
            }
            uint32_t points = 0;
            for (int m = 0; m < contacts[i].manifoldCount; m++) {
                points += (uint32_t)contacts[i].manifolds[m].pointCount;
            }
            b3BodyId ba = b3Shape_GetBody(contacts[i].shapeIdA), bb = b3Shape_GetBody(contacts[i].shapeIdB);
            uint32_t other = hash_get(s, B3_ID_EQUALS(ba, e->body) ? bb : ba);
            if (points == 0 || other >= BODY_SYNC_BODIES_MAX || other == slot) {
                continue;
            }
            sync_authority_body_t *o = &s->authority.bodies[other];
            entry_t *oe = &s->entries[other];
            body_sync_id_t oid = body_sync_id_make(other, o->generation);
            bool pushed = pusher && sync_soft_pushed(approach_speed(s, &contacts[i], s->entries[other].body));
            /* A body it owns by interaction takes only what rests at its owner: a copy its owner
               moves was set going by someone else's action, and taking it on would hand a contested
               heap back and forth between two guests through the host, never letting it rest. */
            if (!host && !pusher && ab->holder == BODY_SYNC_ID_NONE && ab->hold == BODY_SYNC_HOLD_UNTIL_REST && !sync_authority_local(&s->authority, o) && !oe->owner_rests) {
                continue;
            }
            bool local = sync_authority_local(&s->authority, o);
            const sync_received_t *in = &s->receiver.bodies[other];
            sync_policy_take_in_t t = {.mode = s->authority.mode,
                                       .host = host,
                                       .light = o->takable,
                                       .local = local,
                                       .owner_seat = o->seat,
                                       .owner_hold = local || !o->has_in ? o->hold : o->in_hold,
                                       .held = (local || !o->has_in ? o->holder : o->in_holder) != BODY_SYNC_ID_NONE,
                                       .owner_kinematic = !local && in->has && (in->known & SYNC_RARE_MOTION) && in->rare.kinematic,
                                       .agent_speed = speed,
                                       .pushed = pushed,
                                       .owned = owned,
                                       .taken = taken,
                                       .returned_pressed = oe->returned_pressed};
            if (local && o->hold == BODY_SYNC_HOLD_UNTIL_REST && (speed > SYNC_POLICY_AGENT_SPEED || pushed)) {
                oe->agent_tick = tick; /* still being set going: its age starts again */
            }
            if (host && !local) {
                sync_policy_reclaim_in_t r = {.mode = s->authority.mode,
                                              .priority = SYNC_POLICY_PRIORITY,
                                              .owner_seat = o->seat,
                                              .owner_hold = o->has_in ? o->in_hold : o->hold,
                                              .agent_speed = speed,
                                              .pushed = pushed};
                if (sync_policy_host_reclaims(&r)) {
                    host_takes_back(s, other, tick);
                }
            } else if (sync_policy_takes(&t)) {
                if (host) {
                    sync_authority_host_active(&s->authority, oid, true);
                    oe->agent_tick = tick;
                    oe->still.ticks = 0;
                } else if (sync_authority_take(&s->authority, oid) == BODY_SYNC_OK) {
                    owned++;
                    taken++;
                    oe->agent_tick = tick;
                    oe->still.ticks = 0;
                }
            }
        }
    }
}

/* Host: copies of two guests' bodies owned by interaction that touch (sync_policy_host_takes_contested). */
static void contest(body_sync_t *s, uint32_t tick) {
    if (s->authority.mode != SYNC_POLICY_INTERACTION || !is_host(s)) {
        return;
    }
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        const sync_authority_body_t *a = &s->authority.bodies[slot];
        entry_t *e = &s->entries[slot];
        if (!a->bound || !b3Body_IsValid(e->body) || a->seat == BODY_SYNC_SEAT_HOST) {
            continue;
        }
        b3ContactData contacts[32];
        int n = b3Body_GetContactData(e->body, contacts, 32);
        for (int i = 0; i < n && a->seat != BODY_SYNC_SEAT_HOST; i++) {
            uint32_t points = 0;
            for (int m = 0; m < contacts[i].manifoldCount; m++) {
                points += (uint32_t)contacts[i].manifolds[m].pointCount;
            }
            if (!b3Shape_IsValid(contacts[i].shapeIdA) || !b3Shape_IsValid(contacts[i].shapeIdB) || points == 0) {
                continue;
            }
            b3BodyId ba = b3Shape_GetBody(contacts[i].shapeIdA), bb = b3Shape_GetBody(contacts[i].shapeIdB);
            uint32_t other = hash_get(s, B3_ID_EQUALS(ba, e->body) ? bb : ba);
            if (other >= BODY_SYNC_BODIES_MAX || other == slot || !s->authority.bodies[other].bound) {
                continue;
            }
            const sync_authority_body_t *o = &s->authority.bodies[other];
            sync_policy_contest_in_t c = {.mode = s->authority.mode,
                                          .seat = a->seat,
                                          .other_seat = o->seat,
                                          .hold = a->has_in ? a->in_hold : a->hold,
                                          .other_hold = o->has_in ? o->in_hold : o->hold};
            if (sync_policy_host_takes_contested(&c)) {
                host_takes_back(s, slot, tick);
                host_takes_back(s, other, tick);
            }
        }
    }
}

/* Bodies owned by interaction that rest go back: a guest releases them once the host holds their
   rest pose, the host ends its own hold (sync_policy_returns_at_rest). */
/* One of this side's characters or the bodies they carry presses `body` (a contact that acted):
   those the game moves, whose lean is the player's, not a body's weight in a heap. */
static bool pressed_by_own_pusher(const body_sync_t *s, b3BodyId body) {
    b3ContactData contacts[16];
    int n = b3Body_GetContactData(body, contacts, 16);
    for (int i = 0; i < n; i++) {
        if (!b3Shape_IsValid(contacts[i].shapeIdA) || !b3Shape_IsValid(contacts[i].shapeIdB) || !acted(&contacts[i])) {
            continue;
        }
        b3BodyId a = b3Shape_GetBody(contacts[i].shapeIdA), b = b3Shape_GetBody(contacts[i].shapeIdB);
        uint32_t slot = hash_get(s, B3_ID_EQUALS(a, body) ? b : a);
        if (slot >= BODY_SYNC_BODIES_MAX) {
            continue;
        }
        const sync_authority_body_t *o = &s->authority.bodies[slot];
        if (o->bound && sync_authority_local(&s->authority, o) &&
            (s->entries[slot].body_class == BODY_SYNC_CLASS_CHARACTER || o->holder != BODY_SYNC_ID_NONE)) {
            return true;
        }
    }
    return false;
}

static void rest_return(body_sync_t *s, uint32_t tick) {
    if (s->authority.mode != SYNC_POLICY_INTERACTION) {
        return;
    }
    bool host = is_host(s);
    float farthest = 0.0f;
    for (uint8_t seat = 1; host && seat < s->config.seats; seat++) {
        float rtt = s->joined[seat] ? sync_sender_round_trip(&s->sender, seat) : 0.0f;
        farthest = rtt > farthest ? rtt : farthest;
    }
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        const sync_authority_body_t *a = &s->authority.bodies[slot];
        entry_t *e = &s->entries[slot];
        e->quiet = false;
        if (e->returned_pressed && (!a->bound || !b3Body_IsValid(e->body) ||
                                    (sync_authority_local(&s->authority, a) && a->hold == BODY_SYNC_HOLD_UNTIL_REST) ||
                                    !pressed_by_own_pusher(s, e->body))) {
            e->returned_pressed = false;
        }
        if (!a->bound || !b3Body_IsValid(e->body) || !sync_authority_local(&s->authority, a) || a->hold != BODY_SYNC_HOLD_UNTIL_REST) {
            continue;
        }
        float p[3], q[4];
        pose_now(e->body, p, q);
        e->quiet = sync_policy_quiet(&e->still, p, q, !b3Body_IsAwake(e->body));
    }
    uint32_t released = 0;
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        sync_authority_body_t *a = &s->authority.bodies[slot];
        entry_t *e = &s->entries[slot];
        if (!a->bound || !b3Body_IsValid(e->body) || !sync_authority_local(&s->authority, a) || a->hold != BODY_SYNC_HOLD_UNTIL_REST ||
            a->claim_pending) {
            continue;
        }
        /* A body this side carries is its agent's, moved by the game: it neither rests nor ages
           out while carried, and its age starts when the carrier lets go. */
        if (a->holder != BODY_SYNC_ID_NONE) {
            e->agent_tick = tick;
            continue;
        }
        bool neighbours = true;
        b3ContactData contacts[32];
        int n = b3Body_GetContactData(e->body, contacts, 32);
        for (int i = 0; i < n && neighbours; i++) {
            if (!b3Shape_IsValid(contacts[i].shapeIdA) || !b3Shape_IsValid(contacts[i].shapeIdB)) {
                continue;
            }
            b3BodyId ba = b3Shape_GetBody(contacts[i].shapeIdA), bb = b3Shape_GetBody(contacts[i].shapeIdB);
            uint32_t other = hash_get(s, B3_ID_EQUALS(ba, e->body) ? bb : ba);
            const sync_authority_body_t *o = other < BODY_SYNC_BODIES_MAX ? &s->authority.bodies[other] : NULL;
            if (o && other != slot && o->bound && sync_authority_local(&s->authority, o) && o->hold == BODY_SYNC_HOLD_UNTIL_REST) {
                neighbours = s->entries[other].quiet;
            }
        }
        float p[3], q[4];
        pose_now(e->body, p, q);
        bool out = false;
        for (int i = 0; i < 3; i++) {
            out = out || p[i] < s->config.bounds_min[i] || p[i] > s->config.bounds_max[i];
        }
        sync_policy_rest_in_t r = {.host = host,
                                   .quiet = e->quiet,
                                   .neighbours_quiet = neighbours,
                                   .rest_acked = !host && sync_sender_rest_acked(&s->sender, BODY_SYNC_SEAT_HOST, (uint16_t)slot),
                                   .age = tick - e->agent_tick,
                                   .out_of_box = out,
                                   .released = released,
                                   .round_trip_ticks = farthest};
        if (!sync_policy_returns_at_rest(&r)) {
            continue;
        }
        body_sync_id_t id = body_sync_id_make(slot, a->generation);
        e->returned_pressed = pressed_by_own_pusher(s, e->body);
        if (host) {
            sync_authority_host_active(&s->authority, id, false);
        } else if (sync_authority_release(&s->authority, id) == BODY_SYNC_OK) {
            released++;
            s->authority.returns_sent++;
            e->returned_at_rest = r.quiet && r.rest_acked;
        }
    }
}

/* How a body of this side's (bound and local, or not bound at all) pressed a layer copy in the
   step: the game moves its bodies after the apply, so a push from standstill is stopped once by
   the rest hold and seen here, and a slow push keeps the copy soft while it lasts; a lean keeps
   a copy in flight soft (drive_copy). */
typedef enum pressed { PRESSED_NONE = 0, PRESSED_LEAN, PRESSED_PUSH } pressed_t;
static pressed_t pressed_by_this_side(const body_sync_t *s, b3BodyId body) {
    pressed_t pressed = PRESSED_NONE;
    b3ContactData contacts[16];
    int n = b3Body_GetContactData(body, contacts, 16);
    for (int i = 0; i < n; i++) {
        if (!b3Shape_IsValid(contacts[i].shapeIdA) || !b3Shape_IsValid(contacts[i].shapeIdB)) {
            continue;
        }
        b3BodyId a = b3Shape_GetBody(contacts[i].shapeIdA), b = b3Shape_GetBody(contacts[i].shapeIdB);
        b3BodyId pusher = B3_ID_EQUALS(a, body) ? b : a;
        if (b3Body_GetType(pusher) != b3_dynamicBody) {
            continue;
        }
        uint32_t slot = hash_get(s, pusher);
        if (slot < BODY_SYNC_BODIES_MAX && !sync_authority_local(&s->authority, &s->authority.bodies[slot])) {
            continue; /* a copy pushes nothing on its own account */
        }
        /* A body this side owns by interaction leans on what it lies on with its weight; only its
           motion pushes, as only its motion takes (interact). */
        if (slot < BODY_SYNC_BODIES_MAX && s->entries[slot].body_class != BODY_SYNC_CLASS_CHARACTER &&
            s->authority.bodies[slot].holder == BODY_SYNC_ID_NONE && s->authority.bodies[slot].hold == BODY_SYNC_HOLD_UNTIL_REST) {
            if (speed_of(pusher) > SYNC_SOFT_TOUCH_SPEED) {
                return PRESSED_PUSH;
            }
            continue;
        }
        if (sync_soft_pushed(approach_speed(s, &contacts[i], body))) {
            return PRESSED_PUSH;
        }
        if (acted(&contacts[i])) {
            pressed = PRESSED_LEAN;
        }
    }
    return pressed;
}

/* A push reaches through a pile: every layer copy touching a pushed one, and on through them, is
   pressed too. Pulled back toward its owner's older pose it would be pushed into the body that
   presses and placed there again and again; pressed, its pull waits (sync_soft_decide). */
static void press_through(body_sync_t *s, uint32_t tick) {
    if (s->config.light_mass_max <= 0.0f || sync_authority_is_host(&s->authority)) {
        return;
    }
    marking_t *m = &s->marking;
    m->count = 0;
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        if (s->authority.bodies[slot].bound && s->entries[slot].pushed) {
            m->queue[m->count++] = (uint16_t)slot;
        }
    }
    for (uint32_t k = 0; k < m->count; k++) {
        b3ContactData contacts[16];
        int n = b3Body_GetContactData(s->entries[m->queue[k]].body, contacts, 16);
        for (int i = 0; i < n; i++) {
            if (!b3Shape_IsValid(contacts[i].shapeIdA) || !b3Shape_IsValid(contacts[i].shapeIdB)) {
                continue;
            }
            for (int side = 0; side < 2; side++) {
                uint32_t other = hash_get(s, b3Shape_GetBody(side ? contacts[i].shapeIdB : contacts[i].shapeIdA));
                entry_t *o = other < BODY_SYNC_BODIES_MAX ? &s->entries[other] : NULL;
                if (o && o->copy_mode && o->layer && (o->soft || o->rest_held) && !o->pushed) {
                    o->pushed = true;
                    o->marked = tick + 2u;
                    m->queue[m->count++] = (uint16_t)other;
                }
            }
        }
    }
}

void body_sync_collect(body_sync_t *s, uint32_t tick) {
    s->now = tick;
    s->authority.now = tick;
    rest_return(s, tick);
    b3ContactEvents events = b3World_GetContactEvents(s->config.world);
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        sync_authority_body_t *a = &s->authority.bodies[slot];
        entry_t *e = &s->entries[slot];
        sync_body_input_t *in = &s->inputs[slot];
        in->present = false;
        s->send_to[slot] = 0;
        if (!a->bound || !b3Body_IsValid(e->body)) {
            continue;
        }
        if (sync_authority_local(&s->authority, a) && a->hold == BODY_SYNC_HOLD_UNTIL_SETTLED) {
            const sync_authority_body_t *h = sync_authority_body_const(&s->authority, a->last_holder);
            b3BodyId ignore = h ? s->entries[body_sync_id_slot(a->last_holder)].body : b3_nullBodyId;
            bool untakable;
            bool touched = touched_moving(s, &events, e->body, ignore, &untakable);
            sync_authority_settle(&s->authority, body_sync_id_make(slot, a->generation), !b3Body_IsAwake(e->body), touched, untakable);
            if (a->hold == BODY_SYNC_HOLD_UNTIL_REST) {
                e->agent_tick = tick;
            }
        }
        pressed_t pressed = (e->rest_held || e->soft) && e->copy_mode ? pressed_by_this_side(s, e->body) : PRESSED_NONE;
        e->pushed = pressed == PRESSED_PUSH;
        e->leaned = pressed != PRESSED_NONE;
        if (e->pushed) {
            e->marked = tick + 2u; /* soft for the next apply, and kept soft while the push lasts */
        }
        if (e->soft && e->copy_mode && b3Body_IsAwake(e->body)) {
            b3Vec3 v = b3Body_GetLinearVelocity(e->body), w = b3Body_GetAngularVelocity(e->body), g = b3World_GetGravity(s->config.world);
            float k = b3Body_GetGravityScale(e->body) * s->dt;
            const float vv[3] = {v.x, v.y, v.z}, ww[3] = {w.x, w.y, w.z}, gg[3] = {g.x * k, g.y * k, g.z * k};
            e->soft_hit = sync_soft_hit(&e->soft_state, vv, ww, gg);
        }
        bool mine = sync_authority_local(&s->authority, a);
        bool have = s->receiver.bodies[slot].has;
        if (mine) {
            read_local(s, slot, in);
        } else if (is_host(s) && have) {
            read_relayed(s, slot, tick, in);
        }
        s->send_to[slot] = in->present ? sync_authority_route(&s->authority, slot, have) : 0u;
    }
    /* After the reads: a body taken now goes out from the next tick, once its resume has set it
       dynamic, never with the kinematic type its rest hold gave it here. */
    interact(s, tick);
    contest(s, tick);
    press_through(s, tick);
    sync_sender_tick(&s->sender, tick, s->inputs, s->send_to);
}

bool body_sync_write_state(body_sync_t *s, uint8_t seat, uint8_t *buffer, uint32_t capacity, uint32_t *size) {
    *size = 0;
    if (!seat_ok(s, seat) || !s->joined[seat] || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST) || capacity < 64 || !buffer) {
        return false;
    }
    *size = sync_sender_write(&s->sender, seat, sync_authority_sends_sim(&s->authority), buffer, capacity);
    return *size > 0;
}

void body_sync_state_sent(body_sync_t *s, uint8_t seat, uint16_t number) {
    if (seat < s->config.seats) {
        sync_sender_sent(&s->sender, seat, number);
        s->traffic[seat].packets_sent++;
    }
}

uint32_t body_sync_unresolved(const body_sync_t *s, uint8_t seat, uint16_t *numbers, uint32_t max) {
    return sync_sender_unresolved(&s->sender, seat, numbers, max);
}

void body_sync_resolve(body_sync_t *s, uint8_t seat, uint16_t number, bool arrived) { sync_sender_resolve(&s->sender, seat, number, arrived); }

bool body_sync_peek_record(const body_sync_t *s, uint8_t seat, uint8_t *buffer, uint32_t capacity, uint32_t *size) {
    *size = 0;
    if (seat >= s->config.seats || !buffer) {
        return false;
    }
    return sync_authority_peek(&s->authority, seat, buffer, capacity, size);
}

void body_sync_pop_record(body_sync_t *s, uint8_t seat) {
    if (seat >= s->config.seats) {
        return;
    }
    uint32_t size = sync_authority_pop(&s->authority, seat);
    if (size) {
        s->traffic[seat].sent_bytes[BODY_SYNC_TRAFFIC_AUTHORITY] += size;
        s->traffic[seat].records_sent++;
    }
}

bool body_sync_render_offset(const body_sync_t *s, body_sync_id_t id, float position[3], float rotation[4]) {
    if (!sync_authority_body_const(&s->authority, id)) {
        return false;
    }
    const sync_offset_t *o = &s->entries[body_sync_id_slot(id)].offset;
    memcpy(position, o->position, sizeof(o->position));
    memcpy(rotation, o->rotation, sizeof(o->rotation));
    return true;
}

bool body_sync_playback(const body_sync_t *s, body_sync_id_t id, double *owner_tick) {
    const sync_authority_body_t *a = sync_authority_body_const(&s->authority, id);
    if (!a) {
        return false;
    }
    if (sync_authority_local(&s->authority, a)) {
        *owner_tick = (double)s->now;
        return true;
    }
    /* A body that turned into a copy at a collect is drawn where this side simulated it until the
       next apply drives it, which also drops what its owner sent before a take: no playback yet. */
    if (!s->entries[body_sync_id_slot(id)].copy_mode) {
        return false;
    }
    return sync_playback_tick(&s->playback, (uint16_t)body_sync_id_slot(id), owner_tick);
}

bool body_sync_is_predicted(const body_sync_t *s, body_sync_id_t id) {
    const sync_authority_body_t *a = sync_authority_body_const(&s->authority, id);
    return a && !sync_authority_local(&s->authority, a) && s->entries[body_sync_id_slot(id)].soft;
}

bool body_sync_is_pull_paused(const body_sync_t *s, body_sync_id_t id) {
    return body_sync_is_predicted(s, id) && s->entries[body_sync_id_slot(id)].soft_paused;
}

bool body_sync_lost_take(const body_sync_t *s, body_sync_id_t id, uint32_t *count, float *hand_back_m) {
    if (!sync_authority_body_const(&s->authority, id)) {
        *count = 0;
        *hand_back_m = 0.0f;
        return false;
    }
    const entry_t *e = &s->entries[body_sync_id_slot(id)];
    *count = e->lost_count;
    *hand_back_m = e->lost_hand_back;
    return e->lost_glide;
}

float body_sync_round_trip(const body_sync_t *s, uint8_t seat) { return sync_sender_round_trip(&s->sender, seat); }

bool body_sync_is_layer_copy(const body_sync_t *s, body_sync_id_t id) {
    const sync_authority_body_t *a = sync_authority_body_const(&s->authority, id);
    return a && !sync_authority_local(&s->authority, a) && s->entries[body_sync_id_slot(id)].copy_mode && s->entries[body_sync_id_slot(id)].layer;
}

bool body_sync_traffic(const body_sync_t *s, uint8_t seat, body_sync_traffic_t *out) {
    if (seat >= s->config.seats) {
        return false;
    }
    *out = s->traffic[seat];
    const sync_sender_stats_t *st = &s->sender.stats[seat];
    for (int k = 0; k < BODY_SYNC_TRAFFIC_COUNT; k++) {
        out->sent_bytes[k] += st->bits[k] / 8u;
    }
    out->deferred = st->deferred;
    out->stale_max_ticks = st->stale_max_ticks;
    const sync_authority_t *a = &s->authority;
    if (sync_authority_is_host(a)) {
        out->takes = a->granted[seat];
        out->take_refusals = a->refused_takes[seat];
        out->reclaims = a->reclaimed[seat];
    } else if (seat == BODY_SYNC_SEAT_HOST) {
        out->takes = a->takes_sent;
        out->returns = a->returns_sent;
    }
    return true;
}
