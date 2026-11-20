#include "features/net_link/net_link_sim.h"

#include "features/net_link/net_link_transport.h"
#include "net_link_sim_fate.h"

#include <stdlib.h>
#include <string.h>

/* One allocation holds the network: the host, the guest places and, per guest place, a path of
   two directions. A direction owns fixed pools sized from the config: records (a message, its
   duplicate or a control packet), payload chunks, a heap of timed records and the arena that
   keeps polled payloads readable until the next poll. Nothing is allocated after create. */

#define SIM_NONE UINT32_MAX

enum {
    SIM_CHUNK = 64,
    SIM_CONTROL_RECORDS = 8,
    SIM_WINDOW_SLOTS = 10,
    SIM_WINDOW_SLOT_US = 100000, /* ten slots: a one-second window */
    SIM_SLOT_BITS = 5,
};

enum { KIND_UNRELIABLE = 0, KIND_RELIABLE = 1, KIND_CONTROL = 2 };
enum { CONTROL_CONNECT, CONTROL_ACCEPT, CONTROL_REFUSE, CONTROL_CLOSE };
enum { STAGE_FREE, STAGE_UNFLUSHED, STAGE_BACKLOG, STAGE_TIMED, STAGE_HELD, STAGE_INBOX };
/* LINGER: the link is destroyed but its close still owes the host messages and the reason. */
enum { END_FREE, END_CONNECTING, END_ALIVE, END_DEAD, END_LINGER };
typedef enum { PICK_NONE, PICK_CONNECTED, PICK_MESSAGE, PICK_DISCONNECTED } pick_t;

typedef struct sim_record {
    int64_t time_us; /* TIMED: when it arrives or is sent again; INBOX: when it was released */
    uint64_t serial; /* send order across the network */
    uint32_t next;
    uint32_t payload; /* first chunk; SIM_NONE for control */
    uint32_t size;
    uint32_t seq; /* reliable: order inside its lane */
    uint32_t reason;
    uint8_t lane;
    uint8_t kind;
    uint8_t control;
    uint8_t stage;
    bool on_wire;
} sim_record_t;

typedef struct sim_event {
    int64_t time_us;
    uint64_t serial;
    uint32_t reason;
    bool set;
    bool reported;
} sim_event_t;

typedef struct sim_window {
    int64_t slot[SIM_WINDOW_SLOTS];
    uint64_t bytes[SIM_WINDOW_SLOTS];
} sim_window_t;

typedef struct sim_dir {
    sim_rng_t rng;
    sim_conditions_t conditions;
    sim_bucket_t bucket;
    sim_record_t *records;
    uint32_t *heap;
    uint32_t *chunk_next;
    uint8_t *payload_refs; /* by first chunk: the message and its duplicate share a payload */
    uint8_t *chunks;
    uint8_t *arena;
    uint32_t free_record;
    uint32_t free_chunk;
    uint32_t free_chunks;
    uint32_t heap_len;
    uint32_t arena_used;
    uint32_t unflushed_head, unflushed_tail;
    uint32_t backlog_head[NET_LINK_LANES_MAX], backlog_tail[NET_LINK_LANES_MAX];
    uint32_t backlog_len;
    uint32_t held_head[NET_LINK_LANES_MAX], held_tail[NET_LINK_LANES_MAX];
    uint32_t inbox_head, inbox_tail;
    uint32_t next_send_seq[NET_LINK_LANES_MAX];
    uint32_t next_recv_seq[NET_LINK_LANES_MAX];
    uint32_t close_barrier[NET_LINK_LANES_MAX];
    uint32_t close_reason;
    uint32_t queued_messages; /* payloads taken by send and not yet polled */
    uint32_t queued_bytes[2];
    bool close_sent;
    bool close_arrived;
    bool receiver_gone;
    sim_window_t sent;
    sim_window_t received;
} sim_dir_t;

typedef struct sim_guest {
    net_link_t link; /* first: the link pointer is the guest */
    net_link_sim_t *sim;
    uint32_t index;
    uint32_t opens;
    uint8_t state;
    bool cut;
    bool timed_out;
    int64_t cut_since_us;
    uint64_t identity;
    uint64_t host_identity;   /* what the guest knows: set when the host's answer lands */
    uint64_t answer_identity; /* what the host put in its answer, still on the way */
    net_link_peer_t handle;
    sim_event_t connected;
    sim_event_t disconnected;
    uint32_t peer; /* host peer slot, SIM_NONE when not linked */
    sim_dir_t dir[2];
} sim_guest_t;

typedef struct sim_peer {
    bool used; /* until the host polls its DISCONNECTED or closes it */
    net_link_peer_t handle;
    uint64_t identity;
    uint32_t guest; /* SIM_NONE once the guest link is destroyed */
    sim_event_t connected;
    sim_event_t disconnected;
} sim_peer_t;

typedef struct sim_host {
    net_link_t link; /* first: the link pointer is the host */
    net_link_sim_t *sim;
    bool open;
    uint64_t identity;
    uint32_t max_peers;
} sim_host_t;

struct net_link_sim {
    net_link_sim_config_t config;
    int64_t now_us;
    int64_t timeout_us;
    uint64_t serial;
    uint32_t next_handle;
    uint32_t record_capacity;
    uint32_t chunk_capacity;
    uint32_t arena_capacity;
    sim_host_t host;
    sim_peer_t *peers;
    sim_guest_t *guests;
};

static const net_link_ops_t host_ops;
static const net_link_ops_t guest_ops;

// #region records and lists

static uint32_t record_take(sim_dir_t *dir) {
    const uint32_t index = dir->free_record;
    if (index == SIM_NONE) {
        return SIM_NONE;
    }
    sim_record_t *record = &dir->records[index];
    dir->free_record = record->next;
    memset(record, 0, sizeof(*record));
    record->next = SIM_NONE;
    record->payload = SIM_NONE;
    return index;
}

static void record_free(sim_dir_t *dir, uint32_t index) {
    sim_record_t *record = &dir->records[index];
    if (record->payload != SIM_NONE && --dir->payload_refs[record->payload] == 0) {
        uint32_t chunk = record->payload;
        while (chunk != SIM_NONE) {
            const uint32_t next = dir->chunk_next[chunk];
            dir->chunk_next[chunk] = dir->free_chunk;
            dir->free_chunk = chunk;
            dir->free_chunks++;
            chunk = next;
        }
        dir->queued_messages--;
        dir->queued_bytes[record->kind] -= record->size;
    }
    record->stage = STAGE_FREE;
    record->next = dir->free_record;
    dir->free_record = index;
}

static void list_push(sim_dir_t *dir, uint32_t *head, uint32_t *tail, uint32_t index) {
    dir->records[index].next = SIM_NONE;
    if (*head == SIM_NONE) {
        *head = index;
    } else {
        dir->records[*tail].next = index;
    }
    *tail = index;
}

static uint32_t list_pop(sim_dir_t *dir, uint32_t *head, uint32_t *tail) {
    const uint32_t index = *head;
    if (index != SIM_NONE) {
        *head = dir->records[index].next;
        if (*head == SIM_NONE) {
            *tail = SIM_NONE;
        }
    }
    return index;
}

static void list_free(sim_dir_t *dir, uint32_t *head, uint32_t *tail) {
    uint32_t index;
    while ((index = list_pop(dir, head, tail)) != SIM_NONE) {
        record_free(dir, index);
    }
}

static bool heap_before(const sim_dir_t *dir, uint32_t a, uint32_t b) {
    const sim_record_t *ra = &dir->records[a], *rb = &dir->records[b];
    if (ra->time_us != rb->time_us) {
        return ra->time_us < rb->time_us;
    }
    if (ra->serial != rb->serial) {
        return ra->serial < rb->serial;
    }
    return a < b;
}

static void heap_push(sim_dir_t *dir, uint32_t index) {
    dir->records[index].stage = STAGE_TIMED;
    uint32_t at = dir->heap_len++;
    while (at > 0) {
        const uint32_t parent = (at - 1) / 2;
        if (!heap_before(dir, index, dir->heap[parent])) {
            break;
        }
        dir->heap[at] = dir->heap[parent];
        at = parent;
    }
    dir->heap[at] = index;
}

static uint32_t heap_pop(sim_dir_t *dir) {
    const uint32_t top = dir->heap[0];
    const uint32_t last = dir->heap[--dir->heap_len];
    uint32_t at = 0;
    for (;;) {
        uint32_t child = 2 * at + 1;
        if (child >= dir->heap_len) {
            break;
        }
        if (child + 1 < dir->heap_len && heap_before(dir, dir->heap[child + 1], dir->heap[child])) {
            child++;
        }
        if (!heap_before(dir, dir->heap[child], last)) {
            break;
        }
        dir->heap[at] = dir->heap[child];
        at = child;
    }
    if (dir->heap_len > 0) {
        dir->heap[at] = last;
    }
    return top;
}

/* Frees what is still on its way; the inbox is what the receiver already has. */
static void dir_drop(sim_dir_t *dir, bool keep_inbox) {
    list_free(dir, &dir->unflushed_head, &dir->unflushed_tail);
    for (uint32_t lane = 0; lane < NET_LINK_LANES_MAX; lane++) {
        list_free(dir, &dir->backlog_head[lane], &dir->backlog_tail[lane]);
        list_free(dir, &dir->held_head[lane], &dir->held_tail[lane]);
    }
    dir->backlog_len = 0;
    while (dir->heap_len > 0) {
        record_free(dir, heap_pop(dir));
    }
    if (!keep_inbox) {
        list_free(dir, &dir->inbox_head, &dir->inbox_tail);
    }
}

static void dir_reset(net_link_sim_t *sim, sim_dir_t *dir, uint64_t stream) {
    for (uint32_t i = 0; i < sim->record_capacity; i++) {
        dir->records[i].stage = STAGE_FREE;
        dir->records[i].next = i + 1 < sim->record_capacity ? i + 1 : SIM_NONE;
    }
    for (uint32_t i = 0; i < sim->chunk_capacity; i++) {
        dir->chunk_next[i] = i + 1 < sim->chunk_capacity ? i + 1 : SIM_NONE;
    }
    dir->free_record = 0;
    dir->free_chunk = 0;
    dir->free_chunks = sim->chunk_capacity;
    dir->heap_len = 0;
    dir->arena_used = 0;
    dir->unflushed_head = dir->unflushed_tail = SIM_NONE;
    dir->inbox_head = dir->inbox_tail = SIM_NONE;
    for (uint32_t lane = 0; lane < NET_LINK_LANES_MAX; lane++) {
        dir->backlog_head[lane] = dir->backlog_tail[lane] = SIM_NONE;
        dir->held_head[lane] = dir->held_tail[lane] = SIM_NONE;
        dir->next_send_seq[lane] = dir->next_recv_seq[lane] = dir->close_barrier[lane] = 0;
    }
    dir->backlog_len = 0;
    dir->close_reason = 0;
    dir->queued_messages = 0;
    dir->queued_bytes[0] = dir->queued_bytes[1] = 0;
    dir->close_sent = dir->close_arrived = dir->receiver_gone = false;
    sim_rng_seed(&dir->rng, sim->config.seed, stream);
    sim_conditions_set(&dir->conditions, NULL);
    sim_bucket_reset(&dir->bucket, 0, sim->now_us);
    for (uint32_t s = 0; s < SIM_WINDOW_SLOTS; s++) {
        dir->sent.slot[s] = dir->received.slot[s] = INT64_MIN;
        dir->sent.bytes[s] = dir->received.bytes[s] = 0;
    }
}

// #endregion

// #region byte windows

static void window_add(sim_window_t *window, int64_t time_us, uint32_t bytes) {
    const int64_t id = time_us / SIM_WINDOW_SLOT_US;
    const uint32_t s = (uint32_t)(id % SIM_WINDOW_SLOTS);
    if (window->slot[s] != id) {
        window->slot[s] = id;
        window->bytes[s] = 0;
    }
    window->bytes[s] += bytes;
}

static float window_rate(const sim_window_t *window, int64_t now_us) {
    const int64_t id = now_us / SIM_WINDOW_SLOT_US;
    uint64_t sum = 0;
    for (uint32_t s = 0; s < SIM_WINDOW_SLOTS; s++) {
        if (window->slot[s] > id - SIM_WINDOW_SLOTS && window->slot[s] <= id) {
            sum += window->bytes[s];
        }
    }
    return (float)((double)sum * 1e6 / (double)(SIM_WINDOW_SLOTS * SIM_WINDOW_SLOT_US));
}

// #endregion

// #region path

static uint64_t next_serial(net_link_sim_t *sim) { return ++sim->serial; }

static net_link_peer_t mint_handle(net_link_sim_t *sim, uint32_t slot) {
    /* Wrapping takes 2^27 connections; restarting at one keeps NET_LINK_PEER_NONE impossible. */
    if (sim->next_handle >= (1U << (32 - SIM_SLOT_BITS))) {
        sim->next_handle = 1;
    }
    return (sim->next_handle++ << SIM_SLOT_BITS) | (slot & ((1U << SIM_SLOT_BITS) - 1));
}

static void event_set(net_link_sim_t *sim, sim_event_t *event, int64_t time_us, uint32_t reason) {
    event->time_us = time_us;
    event->serial = next_serial(sim);
    event->reason = reason;
    event->set = true;
    event->reported = false;
}

static bool guest_alive(const net_link_sim_t *sim, const sim_guest_t *guest) { return guest->state == END_ALIVE && !(guest->disconnected.set && guest->disconnected.time_us <= sim->now_us); }

static bool peer_alive(const net_link_sim_t *sim, const sim_peer_t *peer) { return peer->used && !(peer->disconnected.set && peer->disconnected.time_us <= sim->now_us); }

static void control_send(sim_guest_t *guest, uint32_t d, uint32_t index, int64_t time_us) {
    sim_dir_t *dir = &guest->dir[d];
    sim_record_t *record = &dir->records[index];
    /* Control packets are neither lost nor jittered; a cut makes them wait like reliable ones. */
    record->on_wire = !guest->cut;
    record->time_us = time_us + (guest->cut ? dir->conditions.retransmit_us : dir->conditions.delay_us);
    heap_push(dir, index);
}

static void control_reply(net_link_sim_t *sim, sim_guest_t *guest, uint8_t control, uint32_t reason, int64_t time_us) {
    sim_dir_t *dir = &guest->dir[NET_LINK_SIM_TO_GUEST];
    const uint32_t index = record_take(dir);
    if (index == SIM_NONE) {
        return;
    }
    dir->records[index].kind = KIND_CONTROL;
    dir->records[index].control = control;
    dir->records[index].reason = reason;
    dir->records[index].serial = next_serial(sim);
    control_send(guest, NET_LINK_SIM_TO_GUEST, index, time_us);
}

static void transmit(sim_guest_t *guest, uint32_t d, uint32_t index, int64_t time_us) {
    sim_dir_t *dir = &guest->dir[d];
    sim_record_t *record = &dir->records[index];
    window_add(&dir->sent, time_us, record->size);
    sim_fate_t fate = {.lost = true};
    if (!guest->cut) {
        sim_fate_draw(&dir->rng, &dir->conditions, record->kind == KIND_RELIABLE, &fate);
    }
    if (fate.lost) {
        if (record->kind == KIND_RELIABLE) {
            record->on_wire = false;
            record->time_us = time_us + dir->conditions.retransmit_us;
            heap_push(dir, index);
        } else {
            record_free(dir, index);
        }
        return;
    }
    record->on_wire = true;
    record->time_us = time_us + fate.delay_us;
    heap_push(dir, index);
    if (fate.twice) {
        const uint32_t copy = record_take(dir);
        if (copy != SIM_NONE) {
            dir->records[copy] = dir->records[index];
            dir->records[copy].time_us = time_us + fate.twice_delay_us;
            dir->payload_refs[dir->records[index].payload]++;
            heap_push(dir, copy);
        }
    }
}

static int64_t backlog_ready_us(const net_link_sim_t *sim, const sim_dir_t *dir) {
    const int64_t ready = sim_bucket_ready_us(&dir->bucket, dir->conditions.rate_bytes_per_s);
    return ready > sim->now_us ? ready : sim->now_us;
}

/* Under the rate, a lower priority number leaves first and ties go in send order. */
static void backlog_depart(net_link_sim_t *sim, sim_guest_t *guest, uint32_t d, int64_t time_us) {
    sim_dir_t *dir = &guest->dir[d];
    uint32_t best_lane = SIM_NONE;
    for (uint32_t lane = 0; lane < guest->link.lane_count; lane++) {
        const uint32_t head = dir->backlog_head[lane];
        if (head == SIM_NONE) {
            continue;
        }
        if (best_lane == SIM_NONE) {
            best_lane = lane;
            continue;
        }
        const uint8_t priority = sim->config.lanes.priority[lane], best_priority = sim->config.lanes.priority[best_lane];
        if (priority < best_priority || (priority == best_priority && dir->records[head].serial < dir->records[dir->backlog_head[best_lane]].serial)) {
            best_lane = lane;
        }
    }
    const uint32_t index = list_pop(dir, &dir->backlog_head[best_lane], &dir->backlog_tail[best_lane]);
    dir->backlog_len--;
    sim_bucket_take(&dir->bucket, dir->conditions.rate_bytes_per_s, time_us, dir->records[index].size);
    transmit(guest, d, index, time_us);
}

static void inbox_push(sim_dir_t *dir, uint32_t index, int64_t time_us) {
    sim_record_t *record = &dir->records[index];
    record->stage = STAGE_INBOX;
    record->time_us = time_us;
    window_add(&dir->received, time_us, record->size);
    list_push(dir, &dir->inbox_head, &dir->inbox_tail, index);
}

static void held_insert(sim_dir_t *dir, uint32_t index) {
    sim_record_t *record = &dir->records[index];
    const uint32_t lane = record->lane;
    record->stage = STAGE_HELD;
    if (dir->held_head[lane] == SIM_NONE || dir->records[dir->held_tail[lane]].seq < record->seq) {
        list_push(dir, &dir->held_head[lane], &dir->held_tail[lane], index);
        return;
    }
    uint32_t *link = &dir->held_head[lane];
    while (dir->records[*link].seq < record->seq) {
        link = &dir->records[*link].next;
    }
    record->next = *link;
    *link = index;
}

static void held_release(sim_dir_t *dir, uint32_t lane, int64_t time_us) {
    uint32_t head;
    while ((head = dir->held_head[lane]) != SIM_NONE && dir->records[head].seq <= dir->next_recv_seq[lane]) {
        list_pop(dir, &dir->held_head[lane], &dir->held_tail[lane]);
        if (dir->records[head].seq < dir->next_recv_seq[lane]) {
            record_free(dir, head);
            continue;
        }
        inbox_push(dir, head, time_us);
        dir->next_recv_seq[lane]++;
    }
}

/* The receiving end of direction `d` learns its peer is gone; what is still on its way is dropped. */
static void end_disconnect(net_link_sim_t *sim, sim_guest_t *guest, uint32_t d, int64_t time_us, uint32_t reason) {
    sim_dir_t *dir = &guest->dir[d];
    if (d == NET_LINK_SIM_TO_GUEST) {
        if ((guest->state == END_CONNECTING || guest->state == END_ALIVE) && !guest->disconnected.set) {
            if (guest->handle == NET_LINK_PEER_NONE) {
                guest->handle = mint_handle(sim, guest->index);
            }
            event_set(sim, &guest->disconnected, time_us, reason);
        }
    } else if (guest->peer != SIM_NONE) {
        sim_peer_t *peer = &sim->peers[guest->peer];
        if (!peer->disconnected.set) {
            event_set(sim, &peer->disconnected, time_us, reason);
        }
    }
    dir->receiver_gone = true;
    dir_drop(dir, true);
}

static void close_try(net_link_sim_t *sim, sim_guest_t *guest, uint32_t d, int64_t time_us) {
    sim_dir_t *dir = &guest->dir[d];
    if (!dir->close_arrived || dir->receiver_gone) {
        return;
    }
    for (uint32_t lane = 0; lane < NET_LINK_LANES_MAX; lane++) {
        if (dir->next_recv_seq[lane] < dir->close_barrier[lane]) {
            return;
        }
    }
    end_disconnect(sim, guest, d, time_us, dir->close_reason);
}

static void host_admit(net_link_sim_t *sim, sim_guest_t *guest, int64_t time_us) {
    sim_host_t *host = &sim->host;
    if (!host->open) {
        control_reply(sim, guest, CONTROL_REFUSE, NET_LINK_REASON_UNREACHABLE, time_us);
        return;
    }
    uint32_t used = 0, free_slot = SIM_NONE;
    for (uint32_t slot = 0; slot < sim->config.max_guests; slot++) {
        if (sim->peers[slot].used) {
            used++;
        } else if (free_slot == SIM_NONE) {
            free_slot = slot;
        }
    }
    guest->answer_identity = host->identity;
    if (used >= host->max_peers || free_slot == SIM_NONE) {
        control_reply(sim, guest, CONTROL_REFUSE, NET_LINK_REASON_REFUSED, time_us);
        return;
    }
    sim_peer_t *peer = &sim->peers[free_slot];
    memset(peer, 0, sizeof(*peer));
    peer->used = true;
    peer->handle = mint_handle(sim, free_slot);
    peer->identity = guest->identity;
    peer->guest = guest->index;
    event_set(sim, &peer->connected, time_us, 0);
    guest->peer = free_slot;
    control_reply(sim, guest, CONTROL_ACCEPT, 0, time_us);
}

static void guest_admitted(net_link_sim_t *sim, sim_guest_t *guest, int64_t time_us) {
    if (guest->state != END_CONNECTING || guest->disconnected.set) {
        return;
    }
    guest->state = END_ALIVE;
    guest->host_identity = guest->answer_identity;
    guest->handle = mint_handle(sim, guest->index);
    event_set(sim, &guest->connected, time_us, 0);
    sim_dir_t *dir = &guest->dir[NET_LINK_SIM_TO_GUEST];
    for (uint32_t lane = 0; lane < NET_LINK_LANES_MAX; lane++) {
        held_release(dir, lane, time_us);
    }
    close_try(sim, guest, NET_LINK_SIM_TO_GUEST, time_us);
}

static void arrive(net_link_sim_t *sim, sim_guest_t *guest, uint32_t d, uint32_t index, int64_t time_us) {
    sim_dir_t *dir = &guest->dir[d];
    sim_record_t *record = &dir->records[index];
    if (dir->receiver_gone) {
        record_free(dir, index);
        return;
    }
    if (record->kind == KIND_CONTROL) {
        const uint8_t control = record->control;
        const uint32_t reason = record->reason;
        record_free(dir, index);
        if (control == CONTROL_CONNECT) {
            host_admit(sim, guest, time_us);
        } else if (control == CONTROL_ACCEPT) {
            guest_admitted(sim, guest, time_us);
        } else if (control == CONTROL_REFUSE) {
            if (guest->state == END_CONNECTING) {
                guest->host_identity = guest->answer_identity;
                end_disconnect(sim, guest, NET_LINK_SIM_TO_GUEST, time_us, reason);
                guest->dir[NET_LINK_SIM_TO_HOST].receiver_gone = true;
                dir_drop(&guest->dir[NET_LINK_SIM_TO_HOST], true);
            }
        } else {
            dir->close_arrived = true;
            close_try(sim, guest, d, time_us);
        }
        return;
    }
    /* A guest has its peer only from the accept on; host messages that overtake it wait. */
    const bool ready = d == NET_LINK_SIM_TO_HOST || guest->state == END_ALIVE;
    if (record->kind == KIND_UNRELIABLE) {
        if (ready) {
            inbox_push(dir, index, time_us);
        } else {
            record_free(dir, index);
        }
        return;
    }
    const uint32_t lane = record->lane;
    if (record->seq < dir->next_recv_seq[lane]) {
        record_free(dir, index);
    } else if (ready && record->seq == dir->next_recv_seq[lane]) {
        inbox_push(dir, index, time_us);
        dir->next_recv_seq[lane]++;
        held_release(dir, lane, time_us);
        close_try(sim, guest, d, time_us);
    } else {
        held_insert(dir, index);
    }
}

static void timed_event(net_link_sim_t *sim, sim_guest_t *guest, uint32_t d, int64_t time_us) {
    sim_dir_t *dir = &guest->dir[d];
    const uint32_t index = heap_pop(dir);
    if (dir->records[index].on_wire) {
        arrive(sim, guest, d, index, time_us);
    } else if (dir->records[index].kind == KIND_CONTROL) {
        control_send(guest, d, index, time_us);
    } else {
        transmit(guest, d, index, time_us);
    }
}

static bool path_live(const net_link_sim_t *sim, const sim_guest_t *guest) {
    if ((guest->state == END_CONNECTING || guest->state == END_ALIVE) && !guest->disconnected.set) {
        return true;
    }
    return guest->peer != SIM_NONE && !sim->peers[guest->peer].disconnected.set;
}

static void path_timeout(net_link_sim_t *sim, sim_guest_t *guest, int64_t time_us) {
    guest->timed_out = true;
    end_disconnect(sim, guest, NET_LINK_SIM_TO_GUEST, time_us, NET_LINK_REASON_TIMEOUT);
    end_disconnect(sim, guest, NET_LINK_SIM_TO_HOST, time_us, NET_LINK_REASON_TIMEOUT);
}

static void dir_flush(net_link_sim_t *sim, sim_guest_t *guest, uint32_t d) {
    sim_dir_t *dir = &guest->dir[d];
    uint32_t index;
    while ((index = list_pop(dir, &dir->unflushed_head, &dir->unflushed_tail)) != SIM_NONE) {
        sim_record_t *record = &dir->records[index];
        if (record->kind == KIND_CONTROL) {
            control_send(guest, d, index, sim->now_us);
            continue;
        }
        record->stage = STAGE_BACKLOG;
        record->time_us = sim->now_us;
        list_push(dir, &dir->backlog_head[record->lane], &dir->backlog_tail[record->lane], index);
        dir->backlog_len++;
    }
    while (dir->backlog_len > 0 && backlog_ready_us(sim, dir) <= sim->now_us) {
        backlog_depart(sim, guest, d, sim->now_us);
    }
}

static void dir_close(net_link_sim_t *sim, sim_dir_t *dir, uint32_t reason) {
    if (dir->close_sent || dir->receiver_gone) {
        return;
    }
    const uint32_t index = record_take(dir);
    if (index == SIM_NONE) {
        return;
    }
    dir->records[index].kind = KIND_CONTROL;
    dir->records[index].control = CONTROL_CLOSE;
    dir->records[index].serial = next_serial(sim);
    dir->records[index].stage = STAGE_UNFLUSHED;
    list_push(dir, &dir->unflushed_head, &dir->unflushed_tail, index);
    dir->close_sent = true;
    dir->close_reason = reason >= NET_LINK_REASON_APP_FIRST && reason <= NET_LINK_REASON_APP_LAST ? reason : NET_LINK_REASON_REMOTE_CLOSE;
    memcpy(dir->close_barrier, dir->next_send_seq, sizeof(dir->close_barrier));
}

static net_link_send_result_t dir_send(net_link_sim_t *sim, sim_dir_t *dir, uint8_t lane, bool reliable, const void *data, uint32_t size) {
    if (dir->receiver_gone) {
        return NET_LINK_SEND_OK; /* the peer is leaving and will be told; the message is lost with it */
    }
    const uint32_t budget = reliable ? sim->config.reliable_queue_bytes : sim->config.unreliable_queue_bytes;
    const uint32_t chunk_count = (size + SIM_CHUNK - 1) / SIM_CHUNK;
    if (dir->queued_messages >= sim->config.queue_messages || size > budget - dir->queued_bytes[reliable] || chunk_count > dir->free_chunks) {
        return reliable ? NET_LINK_SEND_FULL : NET_LINK_SEND_DROPPED;
    }
    const uint32_t index = record_take(dir);
    if (index == SIM_NONE) {
        return reliable ? NET_LINK_SEND_FULL : NET_LINK_SEND_DROPPED;
    }
    sim_record_t *record = &dir->records[index];
    const uint8_t *bytes = data;
    uint32_t *link = &record->payload;
    for (uint32_t offset = 0; offset < size; offset += SIM_CHUNK) {
        const uint32_t chunk = dir->free_chunk;
        dir->free_chunk = dir->chunk_next[chunk];
        dir->free_chunks--;
        const uint32_t part = size - offset < SIM_CHUNK ? size - offset : SIM_CHUNK;
        memcpy(dir->chunks + (size_t)chunk * SIM_CHUNK, bytes + offset, part);
        *link = chunk;
        link = &dir->chunk_next[chunk];
    }
    *link = SIM_NONE;
    dir->payload_refs[record->payload] = 1;
    dir->queued_messages++;
    dir->queued_bytes[reliable] += size;
    record->kind = (uint8_t)(reliable ? KIND_RELIABLE : KIND_UNRELIABLE);
    record->lane = lane;
    record->size = size;
    record->seq = reliable ? dir->next_send_seq[lane]++ : 0;
    record->serial = next_serial(sim);
    record->stage = STAGE_UNFLUSHED;
    list_push(dir, &dir->unflushed_head, &dir->unflushed_tail, index);
    return NET_LINK_SEND_OK;
}

/* The arena holds the reliable budget plus twice the unreliable one (a payload and its
   duplicate), which bounds what one poll can hand out, so the copy always fits. */
static bool message_emit(const net_link_sim_t *sim, sim_dir_t *dir, net_link_event_t *event) {
    const uint32_t index = dir->inbox_head;
    const sim_record_t *record = &dir->records[index];
    if (record->size > sim->arena_capacity - dir->arena_used) {
        return false;
    }
    uint8_t *out = dir->arena + dir->arena_used;
    uint32_t offset = 0;
    for (uint32_t chunk = record->payload; chunk != SIM_NONE; chunk = dir->chunk_next[chunk]) {
        const uint32_t part = record->size - offset < SIM_CHUNK ? record->size - offset : SIM_CHUNK;
        memcpy(out + offset, dir->chunks + (size_t)chunk * SIM_CHUNK, part);
        offset += part;
    }
    dir->arena_used += record->size;
    event->type = NET_LINK_EVENT_MESSAGE;
    event->lane = record->lane;
    event->reliable = record->kind == KIND_RELIABLE;
    event->data = out;
    event->size = record->size;
    list_pop(dir, &dir->inbox_head, &dir->inbox_tail);
    record_free(dir, index);
    return true;
}

static void peer_retire(net_link_sim_t *sim, uint32_t slot) {
    sim_peer_t *peer = &sim->peers[slot];
    if (peer->guest != SIM_NONE) {
        sim_guest_t *guest = &sim->guests[peer->guest];
        guest->peer = SIM_NONE;
        guest->dir[NET_LINK_SIM_TO_HOST].receiver_gone = true;
        dir_drop(&guest->dir[NET_LINK_SIM_TO_HOST], false);
        if (guest->state == END_LINGER) {
            guest->state = END_FREE;
        }
    }
    peer->used = false;
    peer->guest = SIM_NONE;
}

// #endregion

// #region host link

static sim_peer_t *host_peer(const net_link_t *link, net_link_peer_t handle, uint32_t *slot_out) {
    const sim_host_t *host = (const sim_host_t *)link;
    net_link_sim_t *sim = host->sim;
    const uint32_t slot = handle & ((1U << SIM_SLOT_BITS) - 1);
    if (handle == NET_LINK_PEER_NONE || slot >= sim->config.max_guests) {
        return NULL;
    }
    sim_peer_t *peer = &sim->peers[slot];
    if (peer->handle != handle || !peer_alive(sim, peer)) {
        return NULL;
    }
    if (slot_out) {
        *slot_out = slot;
    }
    return peer;
}

static net_link_send_result_t host_send(net_link_t *link, net_link_peer_t handle, uint8_t lane, uint32_t flags, const void *data, uint32_t size) {
    net_link_sim_t *sim = ((sim_host_t *)link)->sim;
    const sim_peer_t *peer = host_peer(link, handle, NULL);
    if (!peer) {
        return NET_LINK_SEND_NO_PEER;
    }
    if (peer->guest == SIM_NONE) {
        return NET_LINK_SEND_OK; /* the guest link is gone and its DISCONNECTED is on the way */
    }
    return dir_send(sim, &sim->guests[peer->guest].dir[NET_LINK_SIM_TO_GUEST], lane, (flags & NET_LINK_SEND_RELIABLE) != 0, data, size);
}

static uint32_t host_poll(net_link_t *link, net_link_event_t *events, uint32_t capacity) {
    net_link_sim_t *sim = ((sim_host_t *)link)->sim;
    for (uint32_t g = 0; g < sim->config.max_guests; g++) {
        sim->guests[g].dir[NET_LINK_SIM_TO_HOST].arena_used = 0;
    }
    uint32_t count = 0;
    while (count < capacity) {
        pick_t best = PICK_NONE;
        uint32_t best_slot = 0;
        int64_t best_time = 0;
        uint64_t best_serial = 0;
        for (uint32_t slot = 0; slot < sim->config.max_guests; slot++) {
            const sim_peer_t *peer = &sim->peers[slot];
            if (!peer->used) {
                continue;
            }
            pick_t kind = PICK_NONE;
            int64_t time_us = 0;
            uint64_t serial = 0;
            if (!peer->connected.reported) {
                kind = PICK_CONNECTED;
                time_us = peer->connected.time_us;
                serial = peer->connected.serial;
            } else {
                const uint32_t head = peer->guest != SIM_NONE ? sim->guests[peer->guest].dir[NET_LINK_SIM_TO_HOST].inbox_head : SIM_NONE;
                if (head != SIM_NONE) {
                    const sim_record_t *record = &sim->guests[peer->guest].dir[NET_LINK_SIM_TO_HOST].records[head];
                    kind = PICK_MESSAGE;
                    time_us = record->time_us;
                    serial = record->serial;
                }
                if (peer->disconnected.set && peer->disconnected.time_us <= sim->now_us &&
                    (kind == PICK_NONE || peer->disconnected.time_us < time_us || (peer->disconnected.time_us == time_us && peer->disconnected.serial < serial))) {
                    kind = PICK_DISCONNECTED;
                    time_us = peer->disconnected.time_us;
                    serial = peer->disconnected.serial;
                }
            }
            if (kind != PICK_NONE && (best == PICK_NONE || time_us < best_time || (time_us == best_time && serial < best_serial))) {
                best = kind;
                best_slot = slot;
                best_time = time_us;
                best_serial = serial;
            }
        }
        if (best == PICK_NONE) {
            break;
        }
        sim_peer_t *peer = &sim->peers[best_slot];
        net_link_event_t *event = &events[count];
        memset(event, 0, sizeof(*event));
        event->peer = peer->handle;
        event->identity = peer->identity;
        if (best == PICK_CONNECTED) {
            event->type = NET_LINK_EVENT_CONNECTED;
            peer->connected.reported = true;
        } else if (best == PICK_MESSAGE) {
            if (!message_emit(sim, &sim->guests[peer->guest].dir[NET_LINK_SIM_TO_HOST], event)) {
                break;
            }
        } else {
            event->type = NET_LINK_EVENT_DISCONNECTED;
            event->reason = peer->disconnected.reason;
            peer_retire(sim, best_slot);
        }
        count++;
    }
    return count;
}

static void host_flush(net_link_t *link) {
    net_link_sim_t *sim = ((sim_host_t *)link)->sim;
    if (!((sim_host_t *)link)->open) {
        return;
    }
    for (uint32_t g = 0; g < sim->config.max_guests; g++) {
        if (sim->guests[g].state != END_FREE) {
            dir_flush(sim, &sim->guests[g], NET_LINK_SIM_TO_GUEST);
        }
    }
}

static void host_close(net_link_t *link, net_link_peer_t handle, uint32_t reason) {
    net_link_sim_t *sim = ((sim_host_t *)link)->sim;
    uint32_t slot = 0;
    const sim_peer_t *peer = host_peer(link, handle, &slot);
    if (!peer) {
        return;
    }
    if (peer->guest != SIM_NONE) {
        sim_guest_t *guest = &sim->guests[peer->guest];
        dir_close(sim, &guest->dir[NET_LINK_SIM_TO_GUEST], reason);
        dir_flush(sim, guest, NET_LINK_SIM_TO_GUEST);
    }
    peer_retire(sim, slot);
}

static void host_destroy(net_link_t *link) {
    sim_host_t *host = (sim_host_t *)link;
    net_link_sim_t *sim = host->sim;
    if (!host->open) {
        return;
    }
    for (uint32_t slot = 0; slot < sim->config.max_guests; slot++) {
        const sim_peer_t *peer = &sim->peers[slot];
        if (!peer->used) {
            continue;
        }
        if (peer->guest != SIM_NONE) {
            sim_guest_t *guest = &sim->guests[peer->guest];
            sim_dir_t *dir = &guest->dir[NET_LINK_SIM_TO_GUEST];
            if ((guest->state == END_CONNECTING || guest->state == END_ALIVE) && !guest->disconnected.set) {
                if (guest->handle == NET_LINK_PEER_NONE) {
                    guest->handle = mint_handle(sim, guest->index);
                }
                event_set(sim, &guest->disconnected, sim->now_us + dir->conditions.delay_us, NET_LINK_REASON_REMOTE_CLOSE);
            }
            dir->receiver_gone = true;
            dir_drop(dir, true);
        }
        peer_retire(sim, slot);
    }
    host->open = false;
}

static bool host_stats(const net_link_t *link, net_link_peer_t handle, net_link_stats_t *out);
static uint64_t host_identity(const net_link_t *link, net_link_peer_t handle) {
    const sim_peer_t *peer = host_peer(link, handle, NULL);
    return peer ? peer->identity : 0;
}

// #endregion

// #region guest link

static net_link_send_result_t guest_send(net_link_t *link, net_link_peer_t handle, uint8_t lane, uint32_t flags, const void *data, uint32_t size) {
    sim_guest_t *guest = (sim_guest_t *)link;
    if (handle == NET_LINK_PEER_NONE || handle != guest->handle || !guest_alive(guest->sim, guest)) {
        return NET_LINK_SEND_NO_PEER;
    }
    return dir_send(guest->sim, &guest->dir[NET_LINK_SIM_TO_HOST], lane, (flags & NET_LINK_SEND_RELIABLE) != 0, data, size);
}

static uint32_t guest_poll(net_link_t *link, net_link_event_t *events, uint32_t capacity) {
    sim_guest_t *guest = (sim_guest_t *)link;
    net_link_sim_t *sim = guest->sim;
    if (guest->state == END_FREE || guest->state == END_LINGER) {
        return 0;
    }
    sim_dir_t *dir = &guest->dir[NET_LINK_SIM_TO_GUEST];
    dir->arena_used = 0;
    uint32_t count = 0;
    while (count < capacity && guest->state != END_DEAD) {
        net_link_event_t *event = &events[count];
        memset(event, 0, sizeof(*event));
        event->peer = guest->handle;
        event->identity = guest->host_identity;
        if (guest->connected.set && !guest->connected.reported) {
            event->type = NET_LINK_EVENT_CONNECTED;
            guest->connected.reported = true;
            count++;
            continue;
        }
        const uint32_t head = guest->connected.set ? dir->inbox_head : SIM_NONE;
        const sim_event_t *gone = &guest->disconnected;
        const bool leave = gone->set && gone->time_us <= sim->now_us;
        if (head != SIM_NONE && (!leave || dir->records[head].time_us < gone->time_us || (dir->records[head].time_us == gone->time_us && dir->records[head].serial < gone->serial))) {
            if (!message_emit(sim, dir, event)) {
                break;
            }
            count++;
            continue;
        }
        if (!leave) {
            break;
        }
        event->type = NET_LINK_EVENT_DISCONNECTED;
        event->reason = gone->reason;
        guest->state = END_DEAD;
        dir->receiver_gone = true;
        dir_drop(dir, false);
        count++;
    }
    return count;
}

static void guest_flush(net_link_t *link) {
    sim_guest_t *guest = (sim_guest_t *)link;
    if (guest->state != END_FREE && guest->state != END_LINGER) {
        dir_flush(guest->sim, guest, NET_LINK_SIM_TO_HOST);
    }
}

static void guest_close(net_link_t *link, net_link_peer_t handle, uint32_t reason) {
    sim_guest_t *guest = (sim_guest_t *)link;
    if (handle == NET_LINK_PEER_NONE || handle != guest->handle || !guest_alive(guest->sim, guest)) {
        return;
    }
    dir_close(guest->sim, &guest->dir[NET_LINK_SIM_TO_HOST], reason);
    guest->state = END_DEAD;
    guest->dir[NET_LINK_SIM_TO_GUEST].receiver_gone = true;
    dir_drop(&guest->dir[NET_LINK_SIM_TO_GUEST], false);
    dir_flush(guest->sim, guest, NET_LINK_SIM_TO_HOST);
}

static void guest_destroy(net_link_t *link) {
    sim_guest_t *guest = (sim_guest_t *)link;
    net_link_sim_t *sim = guest->sim;
    if (guest->state == END_FREE || guest->state == END_LINGER) {
        return;
    }
    if (guest->dir[NET_LINK_SIM_TO_HOST].close_sent && guest->peer != SIM_NONE) {
        /* The close outlives the link: the place stays taken until the host has polled the leave. */
        guest->state = END_LINGER;
        return;
    }
    if (guest->peer != SIM_NONE) {
        sim_peer_t *peer = &sim->peers[guest->peer];
        const sim_dir_t *dir = &guest->dir[NET_LINK_SIM_TO_HOST];
        if (!peer->disconnected.set) {
            event_set(sim, &peer->disconnected, sim->now_us + dir->conditions.delay_us, dir->close_sent ? dir->close_reason : NET_LINK_REASON_REMOTE_CLOSE);
        }
        /* The host loses what it has not polled from this guest: the place is reused at once. */
        peer->guest = SIM_NONE;
        guest->peer = SIM_NONE;
    }
    guest->state = END_FREE;
}

static bool guest_stats(const net_link_t *link, net_link_peer_t handle, net_link_stats_t *out);

static uint64_t guest_identity(const net_link_t *link, net_link_peer_t handle) {
    const sim_guest_t *guest = (const sim_guest_t *)link;
    return handle != NET_LINK_PEER_NONE && handle == guest->handle && guest_alive(guest->sim, guest) ? guest->host_identity : 0;
}

// #endregion

// #region stats

static void path_stats(const net_link_sim_t *sim, const sim_guest_t *guest, uint32_t out_dir, net_link_stats_t *out) {
    const sim_dir_t *send = &guest->dir[out_dir], *recv = &guest->dir[1 - out_dir];
    const int64_t round_trip_us = send->conditions.delay_us + recv->conditions.delay_us + (send->conditions.jitter_us + recv->conditions.jitter_us) / 2;
    out->ping_ms = (float)((double)round_trip_us / 1000.0);
    out->quality = guest->cut ? 0.0F : (float)(1.0 - recv->conditions.loss);
    out->out_bytes_per_s = window_rate(&send->sent, sim->now_us);
    out->in_bytes_per_s = window_rate(&recv->received, sim->now_us);
    out->pending_reliable_bytes = send->queued_bytes[KIND_RELIABLE];
    out->pending_unreliable_bytes = send->queued_bytes[KIND_UNRELIABLE];
}

static bool host_stats(const net_link_t *link, net_link_peer_t handle, net_link_stats_t *out) {
    const net_link_sim_t *sim = ((const sim_host_t *)link)->sim;
    const sim_peer_t *peer = host_peer(link, handle, NULL);
    if (!peer) {
        return false;
    }
    if (peer->guest == SIM_NONE) {
        *out = (net_link_stats_t){.ping_ms = -1.0F, .quality = -1.0F};
        return true;
    }
    path_stats(sim, &sim->guests[peer->guest], NET_LINK_SIM_TO_GUEST, out);
    return true;
}

static bool guest_stats(const net_link_t *link, net_link_peer_t handle, net_link_stats_t *out) {
    const sim_guest_t *guest = (const sim_guest_t *)link;
    if (handle == NET_LINK_PEER_NONE || handle != guest->handle || !guest_alive(guest->sim, guest)) {
        return false;
    }
    path_stats(guest->sim, guest, NET_LINK_SIM_TO_HOST, out);
    return true;
}

// #endregion

static const net_link_ops_t host_ops = {host_send, host_poll, host_flush, host_close, host_destroy, host_stats, host_identity};
static const net_link_ops_t guest_ops = {guest_send, guest_poll, guest_flush, guest_close, guest_destroy, guest_stats, guest_identity};

// #region network

static size_t align16(size_t size) { return (size + 15) & ~(size_t)15; }

static uint64_t align16_64(uint64_t size) { return (size + 15) & ~(uint64_t)15; }

net_link_sim_t *net_link_sim_create(const net_link_sim_config_t *config) {
    if (!config || config->max_guests < 1 || config->max_guests > NET_LINK_SIM_GUESTS_MAX || config->lanes.count < 1 || config->lanes.count > NET_LINK_LANES_MAX || !(config->timeout_s >= 0.0F) ||
        config->timeout_s > 3600.0F) {
        return NULL;
    }
    net_link_sim_config_t c = *config;
    c.max_unreliable_bytes = c.max_unreliable_bytes ? c.max_unreliable_bytes : 1200;
    c.max_reliable_bytes = c.max_reliable_bytes ? c.max_reliable_bytes : 16384;
    c.reliable_queue_bytes = c.reliable_queue_bytes ? c.reliable_queue_bytes : 262144;
    c.unreliable_queue_bytes = c.unreliable_queue_bytes ? c.unreliable_queue_bytes : 262144;
    c.queue_messages = c.queue_messages ? c.queue_messages : 2048;
    c.timeout_s = c.timeout_s > 0.0F ? c.timeout_s : 10.0F;
    /* Every message the limits allow must fit its queue, and the pools stay inside uint32 indices. */
    if (c.reliable_queue_bytes < c.max_reliable_bytes || c.unreliable_queue_bytes < c.max_unreliable_bytes || c.reliable_queue_bytes > (1U << 28) || c.unreliable_queue_bytes > (1U << 28) ||
        c.queue_messages > (1U << 24)) {
        return NULL;
    }
    const uint64_t records = 2ULL * c.queue_messages + SIM_CONTROL_RECORDS;
    const uint64_t chunks = ((uint64_t)c.reliable_queue_bytes + c.unreliable_queue_bytes + SIM_CHUNK - 1) / SIM_CHUNK + c.queue_messages;
    const uint64_t arena = (uint64_t)c.reliable_queue_bytes + 2ULL * c.unreliable_queue_bytes;
    const uint64_t record_bytes = align16_64(records * sizeof(sim_record_t)), heap_bytes = align16_64(records * sizeof(uint32_t));
    const uint64_t next_bytes = align16_64(chunks * sizeof(uint32_t)), refs_bytes = align16_64(chunks), chunk_bytes = align16_64(chunks * SIM_CHUNK);
    const uint64_t arena_bytes = align16_64(arena);
    const uint64_t dir_bytes = record_bytes + heap_bytes + next_bytes + refs_bytes + chunk_bytes + arena_bytes;
    const uint64_t total =
        align16_64(sizeof(net_link_sim_t)) + align16_64((uint64_t)c.max_guests * sizeof(sim_guest_t)) + align16_64((uint64_t)c.max_guests * sizeof(sim_peer_t)) + 2ULL * c.max_guests * dir_bytes;
    if (total > (uint64_t)SIZE_MAX) {
        return NULL;
    }
    uint8_t *memory = malloc((size_t)total);
    if (!memory) {
        return NULL;
    }
    net_link_sim_t *sim = (net_link_sim_t *)memory;
    memset(sim, 0, sizeof(*sim));
    sim->config = c;
    sim->timeout_us = (int64_t)((double)c.timeout_s * 1e6 + 0.5);
    sim->next_handle = 1;
    sim->record_capacity = (uint32_t)records;
    sim->chunk_capacity = (uint32_t)chunks;
    sim->arena_capacity = (uint32_t)arena;
    size_t at = align16(sizeof(net_link_sim_t));
    sim->guests = (sim_guest_t *)(memory + at);
    at += align16(c.max_guests * sizeof(sim_guest_t));
    sim->peers = (sim_peer_t *)(memory + at);
    at += align16(c.max_guests * sizeof(sim_peer_t));
    memset(sim->guests, 0, c.max_guests * sizeof(sim_guest_t));
    memset(sim->peers, 0, c.max_guests * sizeof(sim_peer_t));
    for (uint32_t g = 0; g < c.max_guests; g++) {
        sim_guest_t *guest = &sim->guests[g];
        guest->sim = sim;
        guest->index = g;
        guest->state = END_FREE;
        guest->peer = SIM_NONE;
        for (uint32_t d = 0; d < 2; d++) {
            sim_dir_t *dir = &guest->dir[d];
            dir->records = (sim_record_t *)(memory + at);
            at += (size_t)record_bytes;
            dir->heap = (uint32_t *)(memory + at);
            at += (size_t)heap_bytes;
            dir->chunk_next = (uint32_t *)(memory + at);
            at += (size_t)next_bytes;
            dir->payload_refs = memory + at;
            at += (size_t)refs_bytes;
            dir->chunks = memory + at;
            at += (size_t)chunk_bytes;
            dir->arena = memory + at;
            at += (size_t)arena_bytes;
        }
    }
    for (uint32_t slot = 0; slot < c.max_guests; slot++) {
        sim->peers[slot].guest = SIM_NONE;
    }
    sim->host.sim = sim;
    return sim;
}

void net_link_sim_destroy(net_link_sim_t *sim) { free(sim); }

net_link_t *net_link_sim_host(net_link_sim_t *sim, uint64_t identity, uint32_t max_peers) {
    if (!sim || sim->host.open || max_peers < 1 || max_peers > sim->config.max_guests) {
        return NULL;
    }
    sim_host_t *host = &sim->host;
    net_link_init(&host->link, &host_ops, sim->config.lanes.count, sim->config.max_unreliable_bytes, sim->config.max_reliable_bytes);
    host->sim = sim;
    host->open = true;
    host->identity = identity;
    host->max_peers = max_peers;
    return &host->link;
}

net_link_t *net_link_sim_guest(net_link_sim_t *sim, uint64_t identity) {
    if (!sim) {
        return NULL;
    }
    sim_guest_t *guest = NULL;
    for (uint32_t g = 0; g < sim->config.max_guests && !guest; g++) {
        if (sim->guests[g].state == END_FREE) {
            guest = &sim->guests[g];
        }
    }
    if (!guest) {
        return NULL;
    }
    guest->opens++;
    net_link_init(&guest->link, &guest_ops, sim->config.lanes.count, sim->config.max_unreliable_bytes, sim->config.max_reliable_bytes);
    guest->state = END_CONNECTING;
    guest->cut = false;
    guest->timed_out = false;
    guest->cut_since_us = 0;
    guest->identity = identity;
    guest->host_identity = 0;
    guest->answer_identity = 0;
    guest->handle = NET_LINK_PEER_NONE;
    memset(&guest->connected, 0, sizeof(guest->connected));
    memset(&guest->disconnected, 0, sizeof(guest->disconnected));
    guest->peer = SIM_NONE;
    for (uint32_t d = 0; d < 2; d++) {
        const uint64_t stream = ((uint64_t)guest->index << 40) | ((uint64_t)guest->opens << 1) | d;
        dir_reset(sim, &guest->dir[d], stream);
    }
    sim_dir_t *dir = &guest->dir[NET_LINK_SIM_TO_HOST];
    const uint32_t index = record_take(dir);
    dir->records[index].kind = KIND_CONTROL;
    dir->records[index].control = CONTROL_CONNECT;
    dir->records[index].serial = next_serial(sim);
    dir->records[index].stage = STAGE_UNFLUSHED;
    list_push(dir, &dir->unflushed_head, &dir->unflushed_tail, index);
    return &guest->link;
}

void net_link_sim_advance(net_link_sim_t *sim, double seconds) {
    if (!sim || !(seconds >= 0.0)) {
        return;
    }
    /* A quarter of the int64 range leaves room for every delay and timeout added to the clock. */
    const int64_t clock_max_us = INT64_MAX / 4;
    int64_t step_us = seconds >= 1e12 ? clock_max_us : (int64_t)(seconds * 1e6 + 0.5);
    if (step_us > clock_max_us - sim->now_us) {
        step_us = clock_max_us - sim->now_us;
    }
    const int64_t target_us = sim->now_us + step_us;
    for (;;) {
        enum { NONE, TIMED, BACKLOG, TIMEOUT } best = NONE;
        sim_guest_t *best_guest = NULL;
        uint32_t best_dir = 0;
        int64_t best_time = INT64_MAX;
        for (uint32_t g = 0; g < sim->config.max_guests; g++) {
            sim_guest_t *guest = &sim->guests[g];
            if (guest->state == END_FREE) {
                continue;
            }
            for (uint32_t d = 0; d < 2; d++) {
                const sim_dir_t *dir = &guest->dir[d];
                if (dir->heap_len > 0 && dir->records[dir->heap[0]].time_us < best_time) {
                    best = TIMED;
                    best_guest = guest;
                    best_dir = d;
                    best_time = dir->records[dir->heap[0]].time_us;
                }
                if (dir->backlog_len > 0 && backlog_ready_us(sim, dir) < best_time) {
                    best = BACKLOG;
                    best_guest = guest;
                    best_dir = d;
                    best_time = backlog_ready_us(sim, dir);
                }
            }
            if (guest->cut && !guest->timed_out && path_live(sim, guest) && guest->cut_since_us + sim->timeout_us < best_time) {
                best = TIMEOUT;
                best_guest = guest;
                best_time = guest->cut_since_us + sim->timeout_us;
            }
        }
        if (best == NONE || best_time > target_us) {
            break;
        }
        if (best_time > sim->now_us) {
            sim->now_us = best_time;
        }
        if (best == TIMED) {
            timed_event(sim, best_guest, best_dir, sim->now_us);
        } else if (best == BACKLOG) {
            backlog_depart(sim, best_guest, best_dir, sim->now_us);
        } else {
            path_timeout(sim, best_guest, sim->now_us);
        }
    }
    sim->now_us = target_us;
}

double net_link_sim_time(const net_link_sim_t *sim) { return sim ? (double)sim->now_us * 1e-6 : 0.0; }

static sim_guest_t *sim_guest_of(net_link_sim_t *sim, const net_link_t *link) {
    if (!sim || !link || link->ops != &guest_ops) {
        return NULL;
    }
    for (uint32_t g = 0; g < sim->config.max_guests; g++) {
        if (&sim->guests[g].link == link && sim->guests[g].state != END_FREE && sim->guests[g].state != END_LINGER) {
            return &sim->guests[g];
        }
    }
    return NULL;
}

void net_link_sim_set_conditions(net_link_sim_t *sim, const net_link_t *guest_link, net_link_sim_direction_t direction, const net_link_conditions_t *conditions) {
    sim_guest_t *guest = sim_guest_of(sim, guest_link);
    if (!guest || (direction != NET_LINK_SIM_TO_HOST && direction != NET_LINK_SIM_TO_GUEST)) {
        return;
    }
    sim_dir_t *dir = &guest->dir[direction];
    const uint32_t old_rate = dir->conditions.rate_bytes_per_s;
    sim_conditions_set(&dir->conditions, conditions);
    sim_bucket_set_rate(&dir->bucket, old_rate, dir->conditions.rate_bytes_per_s, sim->now_us);
}

void net_link_sim_set_cut(net_link_sim_t *sim, const net_link_t *guest_link, bool cut) {
    sim_guest_t *guest = sim_guest_of(sim, guest_link);
    if (!guest) {
        return;
    }
    if (cut && !guest->cut) {
        guest->cut_since_us = sim->now_us;
    }
    guest->cut = cut;
}

// #endregion
