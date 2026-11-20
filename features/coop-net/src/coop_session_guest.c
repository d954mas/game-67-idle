#include "coop_session_internal.h"

#include <string.h>

/* The guest: hello, welcome, the roster the host announces, and baselines put back together.
   Lanes do not wait for each other, so a state packet or a baseline may overtake the welcome:
   state that early is dropped as if lost, a baseline is held until the welcome. */

static void closed(coop_session_t *session, uint32_t reason) {
    coop_event_t event = {.type = COOP_EVENT_CLOSED, .seat = COOP_SEAT_HOST, .reason = reason};
    coop_session_push(session, &event);
    session->phase = COOP_GUEST_CLOSED;
    session->baseline_open = false;
    session->baseline_held = false;
}

static void fatal(coop_session_t *session, uint32_t reason) {
    if (session->host_peer != NET_LINK_PEER_NONE) {
        net_link_close(session->link, session->host_peer, reason);
    }
    closed(session, reason);
}

static void push_baseline(coop_session_t *session) {
    coop_event_t event = {
        .type = COOP_EVENT_BASELINE,
        .seat = COOP_SEAT_HOST,
        .tick = session->baseline_tick,
        .data = session->baseline_in,
        .size = session->baseline_total,
        .records_before = session->baseline_records_before,
    };
    coop_session_push(session, &event);
}

/* False on a protocol break. */
static bool take_piece(coop_session_t *session, const coop_wire_msg_t *msg) {
    if (msg->kind == COOP_WIRE_BASELINE_FIRST) {
        if (session->baseline_open || msg->total > session->baseline_max) {
            return false;
        }
        session->baseline_open = true;
        session->baseline_held = false;
        session->baseline_total = msg->total;
        session->baseline_got = 0;
        session->baseline_tick = msg->tick;
        session->baseline_records_before = msg->records_before;
    } else if (!session->baseline_open || msg->size > session->baseline_total - session->baseline_got) {
        return false;
    }
    memcpy(session->baseline_in + session->baseline_got, msg->bytes, msg->size);
    session->baseline_got += msg->size;
    if (session->baseline_got == session->baseline_total) {
        session->baseline_open = false;
        if (session->phase == COOP_GUEST_SEATED) {
            push_baseline(session);
        } else {
            session->baseline_held = true;
        }
    }
    return true;
}

static bool take_welcome(coop_session_t *session, const coop_wire_msg_t *msg) {
    if (session->phase != COOP_GUEST_GREETING) {
        return false;
    }
    session->phase = COOP_GUEST_SEATED;
    session->own_seat = msg->seat;
    session->seats = msg->seats;
    session->tick_hz = msg->tick_hz;
    session->handshake_rtt_s = session->now_s - session->greet_s;
    coop_seat_t *path = &session->seat[COOP_SEAT_HOST];
    coop_seat_reset(path, msg->tick_hz);
    path->seated = true;
    path->peer = session->host_peer;
    path->identity = net_link_identity(session->link, session->host_peer);
    coop_event_t event = {.type = COOP_EVENT_WELCOME, .seat = msg->seat, .tick = msg->tick, .data = msg->bytes, .size = msg->size};
    coop_session_push(session, &event);
    if (session->baseline_held) {
        session->baseline_held = false;
        push_baseline(session);
    }
    return true;
}

static bool take_joined(coop_session_t *session, const coop_wire_msg_t *msg) {
    uint8_t seat = msg->seat;
    if (session->phase != COOP_GUEST_SEATED || seat == session->own_seat || seat >= session->seats || (session->roster & (1U << seat))) {
        return false;
    }
    session->roster |= 1U << seat;
    coop_seat_t *path = &session->seat[seat];
    path->seated = true;
    /* The host does not know its own link identity; this side reads it from its link. */
    path->identity = seat == COOP_SEAT_HOST ? net_link_identity(session->link, session->host_peer) : msg->identity;
    coop_event_t event = {.type = COOP_EVENT_SEAT_JOINED, .seat = seat, .identity = path->identity};
    coop_session_push(session, &event);
    return true;
}

static bool take_left(coop_session_t *session, const coop_wire_msg_t *msg) {
    uint8_t seat = msg->seat;
    if (session->phase != COOP_GUEST_SEATED || seat == session->own_seat || seat >= session->seats || !(session->roster & (1U << seat))) {
        return false;
    }
    session->roster &= ~(1U << seat);
    coop_seat_t *path = &session->seat[seat];
    coop_event_t event = {.type = COOP_EVENT_SEAT_LEFT, .seat = seat, .identity = path->identity, .reason = msg->reason};
    path->seated = false;
    path->identity = 0;
    coop_session_push(session, &event);
    return true;
}

static void message(coop_session_t *session, const net_link_event_t *event) {
    coop_wire_msg_t msg;
    if (!coop_wire_parse(event->data, event->size, event->lane, event->reliable, true, &msg)) {
        fatal(session, COOP_CLOSE_FORMAT);
        return;
    }
    bool seated = session->phase == COOP_GUEST_SEATED;
    bool ok = true;
    switch (msg.kind) {
    case COOP_WIRE_WELCOME: ok = take_welcome(session, &msg); break;
    case COOP_WIRE_JOINED: ok = take_joined(session, &msg); break;
    case COOP_WIRE_LEFT: ok = take_left(session, &msg); break;
    case COOP_WIRE_RECORD:
        ok = seated;
        if (ok) {
            coop_event_t record = {.type = COOP_EVENT_RECORD, .seat = COOP_SEAT_HOST, .tick = msg.tick, .data = msg.bytes, .size = msg.size};
            coop_session_push(session, &record);
        }
        break;
    case COOP_WIRE_STATE:
        if (seated) {
            coop_seat_take_state(session, &session->seat[COOP_SEAT_HOST], &msg, COOP_SEAT_HOST);
        }
        break;
    case COOP_WIRE_BASELINE_FIRST:
    case COOP_WIRE_BASELINE_PIECE: ok = take_piece(session, &msg); break;
    case COOP_WIRE_HELLO: ok = false; break;
    }
    if (!ok) {
        fatal(session, COOP_CLOSE_FORMAT);
    }
}

void coop_guest_handle(coop_session_t *session, const net_link_event_t *event) {
    switch (event->type) {
    case NET_LINK_EVENT_CONNECTED:
        if (session->phase != COOP_GUEST_CONNECTING) {
            return;
        }
        session->host_peer = event->peer;
        session->phase = COOP_GUEST_GREETING;
        session->greet_s = session->now_s;
        coop_wire_msg_t hello = {
            .kind = COOP_WIRE_HELLO,
            .protocol = COOP_SESSION_PROTOCOL,
            .build_key = session->build_key,
            .player_key = session->player_key,
            .baseline_max = session->baseline_max,
            .bytes = session->world_key,
            .size = session->world_key_size,
        };
        if (!coop_session_send_wire(session, session->host_peer, &hello)) {
            fatal(session, COOP_CLOSE_STALLED);
        }
        return;
    case NET_LINK_EVENT_DISCONNECTED:
        if (session->phase == COOP_GUEST_CONNECTING || event->peer == session->host_peer) {
            session->host_peer = NET_LINK_PEER_NONE;
            closed(session, event->reason);
        }
        return;
    case NET_LINK_EVENT_MESSAGE:
        if (event->peer == session->host_peer) {
            message(session, event);
        }
        return;
    }
}

void coop_guest_set_time(coop_session_t *session) {
    if (session->phase == COOP_GUEST_GREETING && session->now_s - session->greet_s > session->handshake_timeout_s) {
        fatal(session, COOP_CLOSE_HANDSHAKE);
    }
}

void coop_guest_destroy(coop_session_t *session) {
    if ((session->phase == COOP_GUEST_GREETING || session->phase == COOP_GUEST_SEATED) && session->host_peer != NET_LINK_PEER_NONE) {
        net_link_close(session->link, session->host_peer, COOP_CLOSE_LEFT);
    }
}
