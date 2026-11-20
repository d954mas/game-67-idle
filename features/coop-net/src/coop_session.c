#include "coop_session_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Times past this are a caller bug; the clock's bucket arithmetic stays exact below it. */
#define COOP_TIME_MAX_S 1.0e12

net_link_lanes_t coop_session_lanes(void) {
    net_link_lanes_t lanes = {.count = COOP_LANE_COUNT};
    lanes.priority[COOP_LANE_RECORDS] = 0;
    lanes.priority[COOP_LANE_STATE] = 1;
    lanes.priority[COOP_LANE_BASELINE] = 2;
    return lanes;
}

const char *coop_close_name(uint32_t reason) {
    switch (reason) {
    case NET_LINK_REASON_NONE: return "none";
    case NET_LINK_REASON_REMOTE_CLOSE: return "remote close";
    case NET_LINK_REASON_TIMEOUT: return "timeout";
    case NET_LINK_REASON_REFUSED: return "refused";
    case NET_LINK_REASON_UNREACHABLE: return "unreachable";
    case NET_LINK_REASON_TRANSPORT: return "transport";
    case COOP_CLOSE_LEFT: return "left";
    case COOP_CLOSE_HOST_LEFT: return "host left";
    case COOP_CLOSE_VERSION: return "other session protocol";
    case COOP_CLOSE_BUILD: return "other build";
    case COOP_CLOSE_WORLD: return "other world";
    case COOP_CLOSE_FULL: return "full";
    case COOP_CLOSE_SHUT: return "shut";
    case COOP_CLOSE_FORMAT: return "protocol break";
    case COOP_CLOSE_HANDSHAKE: return "no hello";
    case COOP_CLOSE_KICKED: return "kicked";
    case COOP_CLOSE_REPLACED: return "replaced";
    case COOP_CLOSE_STALLED: return "stalled";
    default: break;
    }
    if (reason >= COOP_CLOSE_GAME_FIRST && reason <= COOP_CLOSE_GAME_LAST) {
        return "game";
    }
    return "unknown";
}

// #region create and destroy

void coop_seat_reset(coop_seat_t *seat, uint32_t tick_hz) {
    uint8_t *baseline = seat->baseline;
    memset(seat, 0, sizeof(*seat));
    seat->baseline = baseline;
    coop_send_window_reset(&seat->send);
    coop_recv_window_reset(&seat->recv);
    coop_rtt_reset(&seat->rtt);
    coop_clock_reset(&seat->clock, tick_hz);
}

static bool config_valid(const coop_session_config_t *config, bool host) {
    if (!config || !config->link || net_link_lane_count(config->link) != COOP_LANE_COUNT) {
        return false;
    }
    if (host && (config->tick_hz < 1 || config->tick_hz > COOP_WIRE_TICK_HZ_MAX)) {
        return false;
    }
    if (config->world_key_size > COOP_WORLD_KEY_MAX || (config->world_key_size > 0 && !config->world_key)) {
        return false;
    }
    if (!isfinite(config->handshake_timeout_s) || config->handshake_timeout_s < 0.0F) {
        return false;
    }
    /* A hello with the largest world key and an empty state packet must fit. */
    return net_link_max_reliable(config->link) >= COOP_WIRE_HELLO_BYTES + COOP_WORLD_KEY_MAX && net_link_max_unreliable(config->link) > COOP_WIRE_STATE_BYTES;
}

static coop_session_t *create(const coop_session_config_t *config, bool host, uint32_t baseline_buffers) {
    uint64_t wanted = (uint64_t)config->baseline_max * baseline_buffers;
    if (wanted != (uint64_t)(size_t)wanted) {
        return NULL;
    }
    coop_session_t *session = calloc(1, sizeof(*session));
    if (!session) {
        return NULL;
    }
    session->link = config->link;
    session->host = host;
    session->build_key = config->build_key;
    session->world_key_size = config->world_key_size;
    if (config->world_key_size > 0) {
        memcpy(session->world_key, config->world_key, config->world_key_size);
    }
    session->baseline_max = config->baseline_max;
    session->handshake_timeout_s = config->handshake_timeout_s > 0.0F ? (double)config->handshake_timeout_s : COOP_HANDSHAKE_TIMEOUT_DEFAULT_S;
    session->tick_hz = host ? config->tick_hz : 0;
    session->tick = config->tick;
    session->rtt_samples_ok = true;
    session->idle_ticks = 1;
    session->open = true;
    session->own_seat = COOP_SEAT_NONE;
    session->handshake_rtt_s = -1.0;
    uint32_t reliable = net_link_max_reliable(config->link);
    session->reliable_max = reliable < COOP_RELIABLE_CAP ? reliable : COOP_RELIABLE_CAP;
    uint32_t unreliable = net_link_max_unreliable(config->link);
    session->scratch_size = unreliable > session->reliable_max ? unreliable : session->reliable_max;
    session->scratch = malloc(session->scratch_size);
    size_t baseline_bytes = (size_t)wanted;
    uint8_t *baselines = baseline_bytes > 0 ? malloc(baseline_bytes) : NULL;
    if (!session->scratch || (baseline_bytes > 0 && !baselines)) {
        free(session->scratch);
        free(baselines);
        free(session);
        return NULL;
    }
    for (uint32_t seat = 0; seat < COOP_SEATS_MAX; seat++) {
        coop_seat_reset(&session->seat[seat], session->tick_hz);
    }
    if (host) {
        for (uint32_t seat = 1; seat <= baseline_buffers; seat++) {
            session->seat[seat].baseline = baselines + (size_t)(seat - 1) * config->baseline_max;
        }
    } else {
        session->baseline_in = baselines;
    }
    return session;
}

coop_session_t *coop_session_host(const coop_session_config_t *config) {
    if (!config_valid(config, true) || config->seats < 1 || config->seats > COOP_SEATS_MAX) {
        return NULL;
    }
    uint32_t buffers = config->baseline_max > 0 ? config->seats - 1 : 0;
    coop_session_t *session = create(config, true, buffers);
    if (!session) {
        return NULL;
    }
    session->seats = (uint8_t)config->seats;
    session->seat[COOP_SEAT_HOST].seated = true;
    return session;
}

coop_session_t *coop_session_join(const coop_session_config_t *config) {
    if (!config_valid(config, false)) {
        return NULL;
    }
    coop_session_t *session = create(config, false, config->baseline_max > 0 ? 1 : 0);
    if (!session) {
        return NULL;
    }
    session->player_key = config->player_key;
    session->seats = 0;
    session->phase = COOP_GUEST_CONNECTING;
    session->seat[COOP_SEAT_HOST].seated = true;
    return session;
}

void coop_session_destroy(coop_session_t *session) {
    if (!session) {
        return;
    }
    if (session->host) {
        coop_host_destroy(session);
        free(session->seat[1].baseline);
    } else {
        coop_guest_destroy(session);
        free(session->baseline_in);
    }
    free(session->scratch);
    free(session);
}

// #endregion

// #region seats and roles

bool coop_session_is_host(const coop_session_t *session) { return session && session->host; }

uint8_t coop_session_seat(const coop_session_t *session) {
    if (!session) {
        return COOP_SEAT_NONE;
    }
    if (session->host) {
        return COOP_SEAT_HOST;
    }
    return session->phase == COOP_GUEST_SEATED ? session->own_seat : (uint8_t)COOP_SEAT_NONE;
}

uint32_t coop_session_seats(const coop_session_t *session) { return session ? session->seats : 0; }

uint32_t coop_session_tick_hz(const coop_session_t *session) { return session ? session->tick_hz : 0; }

bool coop_session_set_world_key(coop_session_t *session, const void *world_key, uint32_t size) {
    if (!session || !session->host || size > COOP_WORLD_KEY_MAX || (size > 0 && !world_key)) {
        return false;
    }
    if (size > 0) {
        memcpy(session->world_key, world_key, size);
    }
    session->world_key_size = size;
    return true;
}

bool coop_session_seated(const coop_session_t *session, uint8_t seat) {
    if (!session || seat >= COOP_SEATS_MAX) {
        return false;
    }
    if (seat == COOP_SEAT_HOST) {
        return true;
    }
    if (!session->host && seat == session->own_seat) {
        return session->phase == COOP_GUEST_SEATED;
    }
    return session->seat[seat].seated;
}

void coop_session_set_open(coop_session_t *session, bool open) {
    if (session && session->host) {
        session->open = open;
    }
}

void coop_session_kick(coop_session_t *session, uint8_t seat, uint32_t reason) {
    if (!session || !session->host || seat == COOP_SEAT_HOST || seat >= session->seats || !session->seat[seat].seated) {
        return;
    }
    coop_host_leave(session, seat, reason, reason, true);
}

/* The path a send or a measurement of `seat` uses, or NULL. */
static coop_seat_t *path_of(const coop_session_t *session, uint8_t seat) {
    if (session->host) {
        if (seat == COOP_SEAT_HOST || seat >= session->seats || !session->seat[seat].seated) {
            return NULL;
        }
        return (coop_seat_t *)&session->seat[seat];
    }
    if (seat != COOP_SEAT_HOST || session->phase != COOP_GUEST_SEATED) {
        return NULL;
    }
    return (coop_seat_t *)&session->seat[COOP_SEAT_HOST];
}

// #endregion

// #region time

void coop_session_set_time(coop_session_t *session, double now_s) {
    if (!session || !isfinite(now_s) || now_s > COOP_TIME_MAX_S || now_s < session->now_s) {
        return;
    }
    double gap_max = COOP_RTT_GAP_S;
    if (session->tick_hz > 0 && 3.0 / (double)session->tick_hz > gap_max) {
        gap_max = 3.0 / (double)session->tick_hz;
    }
    /* Measured from the last poll that emptied the queue: a loop that keeps its clock and its
       sends going but stops reading would otherwise take its own wait for a round trip. */
    session->rtt_samples_ok = !session->drained || now_s - session->drained_s <= gap_max;
    session->now_s = now_s;
    if (session->host) {
        coop_host_set_time(session);
    } else {
        coop_guest_set_time(session);
    }
}

uint32_t coop_session_tick(const coop_session_t *session) { return session ? session->tick : 0; }

/* An empty state packet on every path the game left without one this tick. */
static void keep_acks_flowing(coop_session_t *session) {
    for (uint8_t seat = 0; seat < COOP_SEATS_MAX; seat++) {
        coop_seat_t *path = path_of(session, seat);
        if (!path) {
            continue;
        }
        path->idle = path->state_this_tick ? 0 : path->idle + 1;
        if (path->idle >= session->idle_ticks) {
            coop_wire_msg_t msg = {.kind = COOP_WIRE_STATE, .number = path->send.next, .tick = session->tick};
            coop_recv_window_ack(&path->recv, session->now_s, &msg.has_ack, &msg.ack, &msg.ack_mask, &msg.ack_wait);
            uint32_t size = coop_wire_write(&msg, session->scratch, session->scratch_size);
            net_link_send_result_t result = net_link_send(session->link, path->peer, COOP_LANE_STATE, NET_LINK_SEND_UNRELIABLE, session->scratch, size);
            if (result == NET_LINK_SEND_OK || result == NET_LINK_SEND_DROPPED) {
                coop_send_window_push(&path->send, session->now_s);
                path->idle = 0;
            }
        }
        path->state_this_tick = false;
    }
}

void coop_session_end_tick(coop_session_t *session) {
    if (!session) {
        return;
    }
    keep_acks_flowing(session);
    if (session->host) {
        coop_host_end_tick(session);
    }
    net_link_flush(session->link);
    session->tick++;
}

void coop_session_set_idle_ticks(coop_session_t *session, uint32_t ticks) {
    if (session) {
        session->idle_ticks = ticks < 1 ? 1 : ticks > COOP_IDLE_TICKS_MAX ? (uint32_t)COOP_IDLE_TICKS_MAX : ticks;
    }
}

double coop_session_peer_tick(const coop_session_t *session, uint8_t seat) {
    const coop_seat_t *path = session ? path_of(session, seat) : NULL;
    return path ? coop_clock_estimate(&path->clock, session->now_s) : -1.0;
}

// #endregion

// #region events

void coop_session_push(coop_session_t *session, const coop_event_t *event) {
    /* Never full: see COOP_EVENT_QUEUE for the bound, which test_coop_session drives. */
    if (session->queue_count >= COOP_EVENT_QUEUE) {
        return;
    }
    session->queue[(session->queue_head + session->queue_count) % COOP_EVENT_QUEUE] = *event;
    session->queue_count++;
}

bool coop_session_poll(coop_session_t *session, coop_event_t *out) {
    if (!session || !out) {
        return false;
    }
    for (;;) {
        if (session->queue_count > 0) {
            *out = session->queue[session->queue_head];
            session->queue_head = (session->queue_head + 1) % COOP_EVENT_QUEUE;
            session->queue_count--;
            return true;
        }
        if (!session->host && session->phase == COOP_GUEST_CLOSED) {
            return false;
        }
        if (session->batch_next >= session->batch_count) {
            session->batch_count = net_link_poll(session->link, session->batch, COOP_LINK_BATCH);
            session->batch_next = 0;
            if (session->batch_count == 0) {
                session->drained = true;
                session->drained_s = session->now_s;
                return false;
            }
        }
        const net_link_event_t *event = &session->batch[session->batch_next++];
        if (session->host) {
            coop_host_handle(session, event);
        } else {
            coop_guest_handle(session, event);
        }
    }
}

// #endregion

// #region sending

static coop_send_result_t from_link(net_link_send_result_t result) {
    switch (result) {
    case NET_LINK_SEND_OK: return COOP_SEND_OK;
    case NET_LINK_SEND_DROPPED: return COOP_SEND_DROPPED;
    case NET_LINK_SEND_FULL: return COOP_SEND_FULL;
    case NET_LINK_SEND_TOO_LARGE: return COOP_SEND_TOO_LARGE;
    case NET_LINK_SEND_NO_PEER: return COOP_SEND_NO_SEAT;
    case NET_LINK_SEND_INVALID: return COOP_SEND_INVALID;
    }
    return COOP_SEND_INVALID;
}

bool coop_session_send_wire(coop_session_t *session, net_link_peer_t peer, const coop_wire_msg_t *msg) {
    uint8_t lane = 0;
    bool reliable = false;
    uint32_t size = coop_wire_write(msg, session->scratch, session->scratch_size);
    if (size == 0 || !coop_wire_route(msg->kind, &lane, &reliable)) {
        return false;
    }
    return net_link_send(session->link, peer, lane, reliable ? NET_LINK_SEND_RELIABLE : NET_LINK_SEND_UNRELIABLE, session->scratch, size) == NET_LINK_SEND_OK;
}

uint32_t coop_session_state_max(const coop_session_t *session) { return session ? net_link_max_unreliable(session->link) - COOP_WIRE_STATE_BYTES : 0; }

uint32_t coop_session_record_max(const coop_session_t *session) { return session ? session->reliable_max - COOP_WIRE_RECORD_BYTES : 0; }

coop_send_result_t coop_session_send_state(coop_session_t *session, uint8_t seat, const void *data, uint32_t size, uint16_t *number) {
    if (!session || !data || size == 0) {
        return COOP_SEND_INVALID;
    }
    coop_seat_t *path = path_of(session, seat);
    if (!path) {
        return COOP_SEND_NO_SEAT;
    }
    if (size > coop_session_state_max(session)) {
        return COOP_SEND_TOO_LARGE;
    }
    coop_wire_msg_t msg = {.kind = COOP_WIRE_STATE, .number = path->send.next, .tick = session->tick, .bytes = data, .size = size};
    coop_recv_window_ack(&path->recv, session->now_s, &msg.has_ack, &msg.ack, &msg.ack_mask, &msg.ack_wait);
    uint32_t wire_size = coop_wire_write(&msg, session->scratch, session->scratch_size);
    if (wire_size == 0) {
        return COOP_SEND_TOO_LARGE;
    }
    net_link_send_result_t result = net_link_send(session->link, path->peer, COOP_LANE_STATE, NET_LINK_SEND_UNRELIABLE, session->scratch, wire_size);
    if (result == NET_LINK_SEND_OK || result == NET_LINK_SEND_DROPPED) {
        uint16_t sent = coop_send_window_push(&path->send, session->now_s);
        path->state_sent++;
        path->state_this_tick = true;
        if (number) {
            *number = sent;
        }
    }
    return from_link(result);
}

coop_send_result_t coop_session_send_record(coop_session_t *session, uint8_t seat, const void *data, uint32_t size) {
    if (!session || !data || size == 0) {
        return COOP_SEND_INVALID;
    }
    coop_seat_t *path = path_of(session, seat);
    if (!path) {
        return COOP_SEND_NO_SEAT;
    }
    if (size > coop_session_record_max(session)) {
        return COOP_SEND_TOO_LARGE;
    }
    coop_wire_msg_t msg = {.kind = COOP_WIRE_RECORD, .tick = session->tick, .bytes = data, .size = size};
    uint32_t wire_size = coop_wire_write(&msg, session->scratch, session->scratch_size);
    if (wire_size == 0) {
        return COOP_SEND_TOO_LARGE;
    }
    coop_send_result_t result = from_link(net_link_send(session->link, path->peer, COOP_LANE_RECORDS, NET_LINK_SEND_RELIABLE, session->scratch, wire_size));
    if (result == COOP_SEND_OK) {
        path->records_sent++;
    }
    return result;
}

coop_send_result_t coop_session_send_baseline(coop_session_t *session, uint8_t seat, const void *data, uint32_t size) {
    if (!session || !session->host || !data || size == 0) {
        return COOP_SEND_INVALID;
    }
    coop_seat_t *path = path_of(session, seat);
    if (!path) {
        return COOP_SEND_NO_SEAT;
    }
    if (size > session->baseline_max || size > path->accept_baseline) {
        return COOP_SEND_TOO_LARGE;
    }
    if (path->baseline_busy) {
        return COOP_SEND_FULL;
    }
    memcpy(path->baseline, data, size);
    path->baseline_busy = true;
    path->baseline_size = size;
    path->baseline_sent = 0;
    path->baseline_tick = session->tick;
    path->baseline_records_before = path->records_sent;
    return COOP_SEND_OK;
}

bool coop_session_baseline_progress(const coop_session_t *session, uint8_t seat, uint32_t *done, uint32_t *total) {
    if (!session || !done || !total) {
        return false;
    }
    if (session->host) {
        const coop_seat_t *path = path_of(session, seat);
        if (!path || !path->baseline_busy) {
            return false;
        }
        *done = path->baseline_sent;
        *total = path->baseline_size;
        return true;
    }
    if (seat != COOP_SEAT_HOST || session->phase == COOP_GUEST_CLOSED || !(session->baseline_open || session->baseline_held)) {
        return false;
    }
    *done = session->baseline_got;
    *total = session->baseline_total;
    return true;
}

coop_packet_fate_t coop_session_packet_fate(const coop_session_t *session, uint8_t seat, uint16_t number) {
    const coop_seat_t *path = session ? path_of(session, seat) : NULL;
    return path ? coop_send_window_fate(&path->send, number) : COOP_PACKET_LOST;
}

// #endregion

// #region measurements

bool coop_session_seat_stats(const coop_session_t *session, uint8_t seat, coop_seat_stats_t *out) {
    const coop_seat_t *path = session ? path_of(session, seat) : NULL;
    if (!path || !out) {
        return false;
    }
    bool measured = path->rtt.samples > 0;
    float rtt_ms = measured ? (float)(path->rtt.smoothed * 1000.0) : -1.0F;
    if (!measured && !session->host && session->handshake_rtt_s >= 0.0) {
        rtt_ms = (float)(session->handshake_rtt_s * 1000.0);
    }
    *out = (coop_seat_stats_t){
        .rtt_ms = rtt_ms,
        .rtt_jitter_ms = measured ? (float)(path->rtt.deviation * 1000.0) : -1.0F,
        .loss = coop_send_window_loss(&path->send),
        .state_sent = path->state_sent,
        .state_received = path->state_received,
        .state_stale = path->state_stale,
    };
    return true;
}

net_link_peer_t coop_session_peer(const coop_session_t *session, uint8_t seat) {
    if (!session || seat >= COOP_SEATS_MAX) {
        return NET_LINK_PEER_NONE;
    }
    if (session->host) {
        return seat != COOP_SEAT_HOST && session->seat[seat].seated ? session->seat[seat].peer : NET_LINK_PEER_NONE;
    }
    bool connected = session->phase == COOP_GUEST_GREETING || session->phase == COOP_GUEST_SEATED;
    return seat == COOP_SEAT_HOST && connected ? session->host_peer : NET_LINK_PEER_NONE;
}

// #endregion

// #region state packets on one path

void coop_seat_take_state(coop_session_t *session, coop_seat_t *seat, const coop_wire_msg_t *msg, uint8_t seat_index) {
    if (msg->has_ack) {
        double sample = coop_send_window_ack(&seat->send, msg->ack, msg->ack_mask, msg->ack_wait, session->now_s);
        if (sample >= 0.0 && session->rtt_samples_ok) {
            coop_rtt_sample(&seat->rtt, sample);
        }
    }
    if (coop_recv_window_accept(&seat->recv, msg->number, msg->tick, session->now_s) != COOP_RECV_NEW) {
        seat->state_stale++;
        return;
    }
    coop_clock_sample(&seat->clock, msg->tick, session->now_s);
    if (msg->size == 0) {
        return;
    }
    seat->state_received++;
    coop_event_t event = {.type = COOP_EVENT_STATE, .seat = seat_index, .tick = msg->tick, .data = msg->bytes, .size = msg->size};
    coop_session_push(session, &event);
}

// #endregion
