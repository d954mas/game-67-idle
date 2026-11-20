#include "coop_session_internal.h"

#include <string.h>

/* The host: peers waiting for their hello, the handshake, seats, notices to the other guests
   and the baselines it cuts into pieces. */

static uint8_t seat_of_peer(const coop_session_t *session, net_link_peer_t peer) {
    for (uint8_t seat = 1; seat < session->seats; seat++) {
        if (session->seat[seat].seated && session->seat[seat].peer == peer) {
            return seat;
        }
    }
    return COOP_SEAT_NONE;
}

static int pending_of_peer(const coop_session_t *session, net_link_peer_t peer) { return coop_pending_find(session->pending, peer); }

static void drop_pending(coop_session_t *session, int slot, uint32_t reason) {
    net_link_close(session->link, session->pending[slot].peer, reason);
    session->pending[slot] = (coop_pending_t){0};
}

/* Sends `msg` to every seated guest but `except`; a guest whose queue refuses it is closed,
   since its view of the seats would differ from the host's from then on. */
static void tell_guests(coop_session_t *session, uint8_t except, const coop_wire_msg_t *msg) {
    uint32_t stalled = 0;
    for (uint8_t seat = 1; seat < session->seats; seat++) {
        if (seat != except && session->seat[seat].seated && !coop_session_send_wire(session, session->seat[seat].peer, msg)) {
            stalled |= 1U << seat;
        }
    }
    for (uint8_t seat = 1; seat < session->seats; seat++) {
        if ((stalled & (1U << seat)) && session->seat[seat].seated) {
            coop_host_leave(session, seat, COOP_CLOSE_STALLED, COOP_CLOSE_STALLED, true);
        }
    }
}

/* `reason` reaches the host's game, `told` the other guests. */
void coop_host_leave(coop_session_t *session, uint8_t seat, uint32_t reason, uint32_t told, bool close_link) {
    coop_seat_t *path = &session->seat[seat];
    if (close_link) {
        net_link_close(session->link, path->peer, reason);
    }
    coop_event_t event = {.type = COOP_EVENT_SEAT_LEFT, .seat = seat, .identity = path->identity, .reason = reason};
    coop_seat_reset(path, session->tick_hz);
    coop_session_push(session, &event);
    coop_wire_msg_t notice = {.kind = COOP_WIRE_LEFT, .seat = seat, .reason = told};
    tell_guests(session, COOP_SEAT_NONE, &notice);
}

/* What the other guests hear when a guest's own connection ends: link reasons as they are, a
   guest's own app code only when it is LEFT or a game code, since a guest could otherwise
   pass itself off as kicked or replaced. */
static uint32_t told_of(uint32_t reason) {
    bool app = reason >= NET_LINK_REASON_APP_FIRST && reason <= NET_LINK_REASON_APP_LAST;
    bool game = reason >= COOP_CLOSE_GAME_FIRST && reason <= COOP_CLOSE_GAME_LAST;
    return app && reason != COOP_CLOSE_LEFT && !game ? (uint32_t)COOP_CLOSE_LEFT : reason;
}

/* Identity zero is a transport that could not tell who connected: it never takes a seat over. */
static uint8_t returning_seat(const coop_session_t *session, uint64_t identity, uint64_t player_key) {
    if (identity == 0) {
        return COOP_SEAT_NONE;
    }
    for (uint8_t seat = 1; seat < session->seats; seat++) {
        const coop_seat_t *path = &session->seat[seat];
        if (path->seated && path->identity == identity && path->player_key == player_key) {
            return seat;
        }
    }
    return COOP_SEAT_NONE;
}

static uint8_t free_seat(const coop_session_t *session) {
    for (uint8_t seat = 1; seat < session->seats; seat++) {
        if (!session->seat[seat].seated) {
            return seat;
        }
    }
    return COOP_SEAT_NONE;
}

/* Welcome and roster for the newcomer: false when its queue refused them. */
static bool welcome(coop_session_t *session, uint8_t seat) {
    net_link_peer_t peer = session->seat[seat].peer;
    coop_wire_msg_t msg = {
        .kind = COOP_WIRE_WELCOME,
        .seat = seat,
        .seats = session->seats,
        .tick = session->tick,
        .tick_hz = session->tick_hz,
        .bytes = session->world_key,
        .size = session->world_key_size,
    };
    if (!coop_session_send_wire(session, peer, &msg)) {
        return false;
    }
    for (uint8_t other = 0; other < session->seats; other++) {
        const coop_seat_t *path = &session->seat[other];
        if (other == seat || !path->seated) {
            continue;
        }
        coop_wire_msg_t notice = {.kind = COOP_WIRE_JOINED, .seat = other, .identity = path->identity};
        if (!coop_session_send_wire(session, peer, &notice)) {
            return false;
        }
    }
    return true;
}

static void greet(coop_session_t *session, int slot, const coop_wire_msg_t *hello) {
    coop_pending_t pending = session->pending[slot];
    if (hello->protocol != COOP_SESSION_PROTOCOL) {
        drop_pending(session, slot, COOP_CLOSE_VERSION);
        return;
    }
    if (hello->build_key != session->build_key) {
        drop_pending(session, slot, COOP_CLOSE_BUILD);
        return;
    }
    uint8_t returning = returning_seat(session, pending.identity, hello->player_key);
    if (!session->open && returning == COOP_SEAT_NONE) {
        drop_pending(session, slot, COOP_CLOSE_SHUT);
        return;
    }
    if (hello->size > 0 && (hello->size != session->world_key_size || memcmp(hello->bytes, session->world_key, hello->size) != 0)) {
        drop_pending(session, slot, COOP_CLOSE_WORLD);
        return;
    }
    uint8_t seat = returning != COOP_SEAT_NONE ? returning : free_seat(session);
    if (seat == COOP_SEAT_NONE) {
        drop_pending(session, slot, COOP_CLOSE_FULL);
        return;
    }
    session->pending[slot] = (coop_pending_t){0};
    if (returning != COOP_SEAT_NONE) {
        coop_host_leave(session, seat, COOP_CLOSE_REPLACED, COOP_CLOSE_REPLACED, true);
    }
    coop_seat_t *path = &session->seat[seat];
    coop_seat_reset(path, session->tick_hz);
    path->seated = true;
    path->peer = pending.peer;
    path->identity = pending.identity;
    path->player_key = hello->player_key;
    path->accept_baseline = hello->baseline_max;
    if (!welcome(session, seat)) {
        net_link_close(session->link, pending.peer, COOP_CLOSE_STALLED);
        coop_seat_reset(path, session->tick_hz);
        return;
    }
    coop_event_t event = {.type = COOP_EVENT_SEAT_JOINED, .seat = seat, .identity = path->identity, .player_key = path->player_key};
    coop_session_push(session, &event);
    coop_wire_msg_t notice = {.kind = COOP_WIRE_JOINED, .seat = seat, .identity = path->identity};
    tell_guests(session, seat, &notice);
}

/* The peer pushed out of the waiting table is closed with HANDSHAKE, so FULL only ever means
   that every seat is taken. */
static void connected(coop_session_t *session, const net_link_event_t *event) {
    if (seat_of_peer(session, event->peer) != COOP_SEAT_NONE || pending_of_peer(session, event->peer) >= 0) {
        return;
    }
    net_link_peer_t pushed = coop_pending_admit(session->pending, event->peer, event->identity, session->now_s);
    if (pushed != NET_LINK_PEER_NONE) {
        net_link_close(session->link, pushed, COOP_CLOSE_HANDSHAKE);
    }
}

static void seated_message(coop_session_t *session, uint8_t seat, const net_link_event_t *event) {
    coop_wire_msg_t msg;
    if (!coop_wire_parse(event->data, event->size, event->lane, event->reliable, false, &msg) || msg.kind == COOP_WIRE_HELLO) {
        coop_host_leave(session, seat, COOP_CLOSE_FORMAT, COOP_CLOSE_FORMAT, true);
        return;
    }
    if (msg.kind == COOP_WIRE_RECORD) {
        coop_event_t record = {.type = COOP_EVENT_RECORD, .seat = seat, .tick = msg.tick, .data = msg.bytes, .size = msg.size};
        coop_session_push(session, &record);
        return;
    }
    coop_seat_take_state(session, &session->seat[seat], &msg, seat);
}

void coop_host_handle(coop_session_t *session, const net_link_event_t *event) {
    uint8_t seat = seat_of_peer(session, event->peer);
    int slot = seat == COOP_SEAT_NONE ? pending_of_peer(session, event->peer) : -1;
    switch (event->type) {
    case NET_LINK_EVENT_CONNECTED:
        connected(session, event);
        return;
    case NET_LINK_EVENT_DISCONNECTED:
        if (seat != COOP_SEAT_NONE) {
            coop_host_leave(session, seat, event->reason, told_of(event->reason), false);
        } else if (slot >= 0) {
            session->pending[slot] = (coop_pending_t){0};
        }
        return;
    case NET_LINK_EVENT_MESSAGE:
        if (seat != COOP_SEAT_NONE) {
            seated_message(session, seat, event);
        } else if (slot >= 0) {
            coop_wire_msg_t msg;
            if (!coop_wire_parse(event->data, event->size, event->lane, event->reliable, false, &msg) || msg.kind != COOP_WIRE_HELLO) {
                drop_pending(session, slot, COOP_CLOSE_FORMAT);
            } else {
                greet(session, slot, &msg);
            }
        }
        return;
    }
}

void coop_host_set_time(coop_session_t *session) {
    for (int slot = 0; slot < COOP_PENDING_MAX; slot++) {
        if (session->pending[slot].peer != NET_LINK_PEER_NONE && session->now_s - session->pending[slot].since_s > session->handshake_timeout_s) {
            drop_pending(session, slot, COOP_CLOSE_HANDSHAKE);
        }
    }
}

/* Hands the next pieces of a baseline to the link while it keeps little of it queued. */
static void feed_baseline(coop_session_t *session, coop_seat_t *path) {
    uint32_t piece_room = session->reliable_max;
    while (path->baseline_busy) {
        bool first = path->baseline_sent == 0;
        uint32_t header = first ? COOP_WIRE_BASELINE_FIRST_BYTES : COOP_WIRE_BASELINE_PIECE_BYTES;
        uint32_t chunk = path->baseline_size - path->baseline_sent;
        if (chunk > piece_room - header) {
            chunk = piece_room - header;
        }
        net_link_stats_t stats;
        if (!net_link_stats(session->link, path->peer, &stats)) {
            return;
        }
        if (stats.pending_reliable_bytes > 0 && (uint64_t)stats.pending_reliable_bytes + header + chunk > COOP_BASELINE_IN_FLIGHT) {
            return;
        }
        coop_wire_msg_t msg = {
            .kind = first ? COOP_WIRE_BASELINE_FIRST : COOP_WIRE_BASELINE_PIECE,
            .total = path->baseline_size,
            .tick = path->baseline_tick,
            .records_before = path->baseline_records_before,
            .bytes = path->baseline + path->baseline_sent,
            .size = chunk,
        };
        if (!coop_session_send_wire(session, path->peer, &msg)) {
            return;
        }
        path->baseline_sent += chunk;
        if (path->baseline_sent == path->baseline_size) {
            path->baseline_busy = false;
        }
    }
}

void coop_host_end_tick(coop_session_t *session) {
    for (uint8_t seat = 1; seat < session->seats; seat++) {
        if (session->seat[seat].seated && session->seat[seat].baseline_busy) {
            feed_baseline(session, &session->seat[seat]);
        }
    }
}

void coop_host_destroy(coop_session_t *session) {
    for (uint8_t seat = 1; seat < session->seats; seat++) {
        if (session->seat[seat].seated) {
            net_link_close(session->link, session->seat[seat].peer, COOP_CLOSE_HOST_LEFT);
        }
    }
    for (int slot = 0; slot < COOP_PENDING_MAX; slot++) {
        if (session->pending[slot].peer != NET_LINK_PEER_NONE) {
            net_link_close(session->link, session->pending[slot].peer, COOP_CLOSE_HOST_LEFT);
        }
    }
}
