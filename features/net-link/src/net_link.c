#include "features/net_link/net_link.h"

#include "features/net_link/net_link_transport.h"
#include "net_link_capture_internal.h"

#include <string.h>

void net_link_init(net_link_t *link, const net_link_ops_t *ops, uint8_t lane_count, uint32_t max_unreliable, uint32_t max_reliable) {
    memset(link, 0, sizeof(*link));
    link->ops = ops;
    link->lane_count = lane_count;
    link->max_unreliable = max_unreliable;
    link->max_reliable = max_reliable;
}

net_link_send_result_t net_link_send(net_link_t *link, net_link_peer_t peer, uint8_t lane, uint32_t flags, const void *data, uint32_t size) {
    if (!link || lane >= link->lane_count || (flags & ~(uint32_t)NET_LINK_SEND_RELIABLE) != 0 || !data || size == 0) {
        return NET_LINK_SEND_INVALID;
    }
    const bool reliable = (flags & NET_LINK_SEND_RELIABLE) != 0;
    if (size > (reliable ? link->max_reliable : link->max_unreliable)) {
        return NET_LINK_SEND_TOO_LARGE;
    }
    const net_link_send_result_t result = link->ops->send(link, peer, lane, flags, data, size);
    if (result == NET_LINK_SEND_OK && link->capture) {
        net_link_capture_record(link->capture, link->capture_tag, NET_LINK_CAPTURE_OUT, link->ops->identity(link, peer), lane, reliable, data, size);
    }
    return result;
}

uint32_t net_link_poll(net_link_t *link, net_link_event_t *events, uint32_t capacity) {
    if (!link || !events || capacity == 0) {
        return 0;
    }
    const uint32_t count = link->ops->poll(link, events, capacity);
    if (link->capture) {
        for (uint32_t i = 0; i < count; i++) {
            const net_link_event_t *event = &events[i];
            if (event->type == NET_LINK_EVENT_MESSAGE) {
                net_link_capture_record(link->capture, link->capture_tag, NET_LINK_CAPTURE_IN, event->identity, event->lane, event->reliable, event->data, event->size);
            }
        }
    }
    return count;
}

void net_link_flush(net_link_t *link) {
    if (link) {
        link->ops->flush(link);
    }
}

void net_link_close(net_link_t *link, net_link_peer_t peer, uint32_t reason) {
    if (link) {
        link->ops->close(link, peer, reason);
    }
}

void net_link_destroy(net_link_t *link) {
    if (link) {
        link->ops->destroy(link);
    }
}

uint32_t net_link_max_unreliable(const net_link_t *link) { return link ? link->max_unreliable : 0; }

uint32_t net_link_max_reliable(const net_link_t *link) { return link ? link->max_reliable : 0; }

uint8_t net_link_lane_count(const net_link_t *link) { return link ? link->lane_count : 0; }

bool net_link_stats(const net_link_t *link, net_link_peer_t peer, net_link_stats_t *out) {
    if (!link || !out) {
        return false;
    }
    return link->ops->stats(link, peer, out);
}

uint64_t net_link_identity(const net_link_t *link, net_link_peer_t peer) { return link ? link->ops->identity(link, peer) : 0; }
