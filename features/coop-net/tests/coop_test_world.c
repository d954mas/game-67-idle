#include "coop_test_world.h"

#include "unity.h"

#include <string.h>

void coop_world_open(coop_world_t *world, uint64_t seed, uint32_t max_guests, const net_link_sim_config_t *config) {
    memset(world, 0, sizeof(*world));
    net_link_sim_config_t sim_config = config ? *config : (net_link_sim_config_t){0};
    sim_config.seed = seed;
    sim_config.max_guests = max_guests;
    sim_config.lanes = coop_session_lanes();
    if (sim_config.reliable_queue_bytes == 0) {
        sim_config.reliable_queue_bytes = 1U << 17;
    }
    if (sim_config.unreliable_queue_bytes == 0) {
        sim_config.unreliable_queue_bytes = 1U << 16;
    }
    if (sim_config.queue_messages == 0) {
        sim_config.queue_messages = 1024;
    }
    world->sim = net_link_sim_create(&sim_config);
    TEST_ASSERT_NOT_NULL(world->sim);
    world->max_guests = max_guests;
    world->dt = 1.0 / 60.0;
}

void coop_world_close(coop_world_t *world) {
    for (int guest = 0; guest < COOP_WORLD_GUESTS; guest++) {
        coop_world_drop(world, guest);
    }
    coop_session_destroy(world->host);
    if (world->host_link) {
        net_link_destroy(world->host_link);
    }
    net_link_sim_destroy(world->sim);
    memset(world, 0, sizeof(*world));
}

static coop_session_config_t session_config(net_link_t *link, const coop_side_config_t *side) {
    return (coop_session_config_t){
        .link = link,
        .seats = side->seats,
        .build_key = side->build_key,
        .player_key = side->player_key,
        .world_key = side->world_key,
        .world_key_size = side->world_key_size,
        .baseline_max = side->baseline_max,
        .handshake_timeout_s = side->handshake_timeout_s,
        .tick_hz = side->tick_hz ? side->tick_hz : 60,
        .tick = 1000,
    };
}

coop_session_t *coop_world_host(coop_world_t *world, const coop_side_config_t *side) {
    world->host_link = net_link_sim_host(world->sim, COOP_WORLD_HOST_IDENTITY, world->max_guests);
    TEST_ASSERT_NOT_NULL(world->host_link);
    coop_session_config_t config = session_config(world->host_link, side);
    world->host = coop_session_host(&config);
    TEST_ASSERT_NOT_NULL(world->host);
    return world->host;
}

coop_session_t *coop_world_join(coop_world_t *world, int guest, const coop_side_config_t *side) {
    world->guest_link[guest] = net_link_sim_guest(world->sim, side->identity);
    TEST_ASSERT_NOT_NULL(world->guest_link[guest]);
    coop_session_config_t config = session_config(world->guest_link[guest], side);
    world->guest[guest] = coop_session_join(&config);
    TEST_ASSERT_NOT_NULL(world->guest[guest]);
    coop_log_clear(&world->guest_log[guest]);
    return world->guest[guest];
}

void coop_world_drop(coop_world_t *world, int guest) {
    coop_session_destroy(world->guest[guest]);
    world->guest[guest] = NULL;
    if (world->guest_link[guest]) {
        net_link_destroy(world->guest_link[guest]);
        world->guest_link[guest] = NULL;
    }
}

coop_log_t *coop_world_log(coop_world_t *world, int side) { return side == COOP_WORLD_HOST ? &world->host_log : &world->guest_log[side]; }

static void log_event(coop_log_t *log, const coop_event_t *event) {
    if (log->count < COOP_WORLD_LOG) {
        coop_logged_t *item = &log->items[log->count];
        item->event = *event;
        uint32_t keep = event->size < COOP_WORLD_DATA ? event->size : COOP_WORLD_DATA;
        if (event->data && keep > 0) {
            memcpy(item->data, event->data, keep);
        }
    }
    log->count++;
}

static void step_side(coop_world_t *world, coop_session_t *session, int side, double now) {
    coop_session_set_time(session, now);
    coop_event_t event;
    while (coop_session_poll(session, &event)) {
        log_event(coop_world_log(world, side), &event);
        if (world->on_event) {
            world->on_event(world->context, world, side, &event);
        }
    }
    if (world->on_tick) {
        world->on_tick(world->context, world, side);
    }
    coop_session_end_tick(session);
}

void coop_world_step(coop_world_t *world) {
    net_link_sim_advance(world->sim, world->dt);
    double now = net_link_sim_time(world->sim);
    if (world->host) {
        step_side(world, world->host, COOP_WORLD_HOST, now);
    }
    for (int guest = 0; guest < COOP_WORLD_GUESTS; guest++) {
        if (world->guest[guest] && !(world->frozen & (1U << guest))) {
            step_side(world, world->guest[guest], guest, now);
        }
    }
}

void coop_world_run(coop_world_t *world, double seconds) {
    int steps = (int)(seconds / world->dt + 0.5);
    for (int step = 0; step < steps; step++) {
        coop_world_step(world);
    }
}

void coop_world_conditions(coop_world_t *world, int guest, const net_link_conditions_t *to_host, const net_link_conditions_t *to_guest) {
    net_link_sim_set_conditions(world->sim, world->guest_link[guest], NET_LINK_SIM_TO_HOST, to_host);
    net_link_sim_set_conditions(world->sim, world->guest_link[guest], NET_LINK_SIM_TO_GUEST, to_guest);
}

uint32_t coop_log_count(const coop_log_t *log, coop_event_type_t type) {
    uint32_t count = 0;
    uint32_t kept = log->count < COOP_WORLD_LOG ? log->count : COOP_WORLD_LOG;
    for (uint32_t i = 0; i < kept; i++) {
        count += log->items[i].event.type == type ? 1U : 0U;
    }
    return count;
}

const coop_logged_t *coop_log_find(const coop_log_t *log, coop_event_type_t type, uint8_t seat) {
    uint32_t kept = log->count < COOP_WORLD_LOG ? log->count : COOP_WORLD_LOG;
    for (uint32_t i = 0; i < kept; i++) {
        if (log->items[i].event.type == type && (seat == COOP_SEAT_NONE || log->items[i].event.seat == seat)) {
            return &log->items[i];
        }
    }
    return NULL;
}

void coop_log_clear(coop_log_t *log) { log->count = 0; }

void raw_poll(raw_end_t *raw) {
    net_link_event_t events[16];
    uint32_t count;
    while ((count = net_link_poll(raw->link, events, 16)) > 0) {
        for (uint32_t i = 0; i < count; i++) {
            if (events[i].type == NET_LINK_EVENT_CONNECTED) {
                raw->connected = true;
                raw->peer = events[i].peer;
            } else if (events[i].type == NET_LINK_EVENT_DISCONNECTED) {
                raw->disconnected = true;
                raw->reason = events[i].reason;
            } else {
                if (raw->messages == 0) {
                    raw->first_size = events[i].size < sizeof(raw->first) ? events[i].size : (uint32_t)sizeof(raw->first);
                    memcpy(raw->first, events[i].data, raw->first_size);
                    raw->first_lane = events[i].lane;
                }
                raw->messages++;
                raw->last_size = events[i].size < sizeof(raw->last) ? events[i].size : (uint32_t)sizeof(raw->last);
                memcpy(raw->last, events[i].data, raw->last_size);
                raw->last_lane = events[i].lane;
            }
        }
    }
    net_link_flush(raw->link);
}

uint32_t coop_test_rand(uint64_t *state) {
    *state = *state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (uint32_t)(*state >> 33);
}
