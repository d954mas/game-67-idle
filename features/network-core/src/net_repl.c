#include "net_repl.h"

#include <string.h>

#include "net_codec.h"

_Static_assert(NET_REPL_EVENT_MAX_BYTES <= UINT8_MAX, "an event's size is one byte");

void net_repl_viewer_init(net_repl_viewer_t *viewer, uint32_t player) {
    memset(viewer, 0, sizeof *viewer);
    viewer->player = player;
    viewer->district = NET_REPL_DISTRICT_NONE;
}

void net_repl_reset(net_repl_viewer_t *viewer) {
    viewer->has_baseline = false;
    viewer->baseline_tick = 0U;
    viewer->outbox_count = 0U;
}

bool net_repl_admits(const net_repl_viewer_t *viewer, const net_repl_event_t *event, const net_repl_knobs_t *knobs) {
    switch (event->audience) {
    case NET_REPL_AUDIENCE_OWNER: return event->owner == viewer->player;
    case NET_REPL_AUDIENCE_ALL: return true;
    case NET_REPL_AUDIENCE_NEARBY: {
        if (viewer->district == NET_REPL_DISTRICT_NONE || event->district != viewer->district) { return false; }
        const float dx = event->x - viewer->x;
        const float dz = event->z - viewer->z;
        return dx * dx + dz * dz <= knobs->interest_radius * knobs->interest_radius;
    }
    default: return false;
    }
}

bool net_repl_queue(net_repl_viewer_t *viewer, const net_repl_event_t *events, uint32_t count, const net_repl_knobs_t *knobs) {
    for (uint32_t index = 0U; index < count; ++index) {
        const net_repl_event_t *event = &events[index];
        if (!event->reliable || !net_repl_admits(viewer, event, knobs)) { continue; }
        if (viewer->outbox_count >= NET_REPL_OUTBOX_EVENTS) { return false; }
        viewer->outbox[viewer->outbox_count++] = *event;
    }
    return true;
}

/* An event as it rides the packet: its tick offset, then its bytes. */
static size_t put_event(uint8_t *out, uint32_t tick, const net_repl_event_t *event) {
    if (event->tick > tick || event->size > NET_REPL_EVENT_MAX_BYTES) { __builtin_trap(); }
    const size_t head = net_varint_put(out, tick - event->tick);
    memcpy(out + head, event->bytes, event->size);
    return head + event->size;
}

/* The event list goes last so its count is known when the header is written:
   the events are laid out after a reserved count field and moved up. */
size_t net_repl_build(net_repl_viewer_t *viewer, uint32_t tick, uint32_t input_seq, uint32_t input_wait, const uint8_t *rows,
    size_t rows_size, const net_repl_event_t *events, uint32_t count, const net_repl_knobs_t *knobs, uint8_t *out, size_t cap,
    net_repl_built_t *built) {
    if (cap < NET_REPL_PACKET_CAP_MIN + rows_size || input_wait > NET_REPL_WAIT_MAX_MS + 1U) { __builtin_trap(); }
    uint8_t header[5U * NET_VARINT_MAX];
    size_t head = net_varint_put(header, tick);
    head += net_varint_put(header + head, input_seq);
    head += net_varint_put(header + head, input_wait);
    head += net_varint_put(header + head, viewer->has_baseline ? viewer->baseline_tick + 1U : 0U);
    /* The row section rides whole, before the events: it is the state the
       baseline moves to. Up to 5 count bytes follow; the body starts after
       the widest. */
    const size_t lead = head + rows_size;
    const size_t body_at = lead + NET_VARINT_MAX;
    size_t body = body_at;
    /* The minimum cap holds a full outbox, so the reliable events always fit. */
    for (uint32_t index = 0U; index < viewer->outbox_count; ++index) { body += put_event(out + body, tick, &viewer->outbox[index]); }
    built->reliable_sent = viewer->outbox_count;
    built->cosmetic_sent = 0U;
    built->rows_at = head;
    for (uint32_t index = 0U; index < count; ++index) {
        const net_repl_event_t *event = &events[index];
        if (event->reliable || !net_repl_admits(viewer, event, knobs)) { continue; }
        const size_t packet = body_at + (body - body_at) + NET_VARINT_MAX + event->size;
        if (packet > knobs->bytes_per_tick || cap - body < NET_VARINT_MAX + (size_t)event->size) {
            viewer->cosmetic_dropped += 1U;
            continue;
        }
        body += put_event(out + body, tick, event);
        built->cosmetic_sent += 1U;
    }
    const size_t count_bytes = net_varint_put(header + head, built->reliable_sent + built->cosmetic_sent);
    memmove(out + lead + count_bytes, out + body_at, body - body_at);
    memcpy(out, header, head);
    if (rows_size > 0U) { memcpy(out + head, rows, rows_size); }
    memcpy(out + lead, header + head, count_bytes);
    return lead + count_bytes + (body - body_at);
}

void net_repl_sent(net_repl_viewer_t *viewer, uint32_t tick, const net_repl_built_t *built) {
    viewer->has_baseline = true;
    viewer->baseline_tick = tick;
    const uint32_t left = viewer->outbox_count - built->reliable_sent;
    memmove(viewer->outbox, viewer->outbox + built->reliable_sent, left * sizeof viewer->outbox[0]);
    viewer->outbox_count = left;
}

void net_repl_refused(net_repl_viewer_t *viewer, const net_repl_built_t *built) {
    viewer->cosmetic_dropped += built->cosmetic_sent;
}
