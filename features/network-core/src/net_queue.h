#ifndef NETWORK_CORE_NET_QUEUE_H
#define NETWORK_CORE_NET_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Fixed-capacity byte ring of length-prefixed messages. The capacity is the
   whole memory a peer can make this side hold, so a slow or flooding peer is
   bounded by construction and the overflow is the caller's close signal. */

typedef struct net_queue_t {
    uint8_t *data;
    size_t capacity;
    size_t head;
    size_t used;
    size_t count;
} net_queue_t;

static inline bool net_queue_init(net_queue_t *queue, size_t capacity) {
    queue->data = (uint8_t *)malloc(capacity);
    queue->capacity = capacity;
    queue->head = 0U;
    queue->used = 0U;
    queue->count = 0U;
    return queue->data != NULL;
}

static inline void net_queue_free(net_queue_t *queue) {
    free(queue->data);
    queue->data = NULL;
    queue->capacity = 0U;
    queue->used = 0U;
    queue->count = 0U;
}

static inline void net_queue_write_at(net_queue_t *queue, size_t offset,
    const uint8_t *src, size_t size) {
    const size_t start = (queue->head + offset) % queue->capacity;
    const size_t first = size < queue->capacity - start ? size : queue->capacity - start;
    memcpy(queue->data + start, src, first);
    memcpy(queue->data, src + first, size - first);
}

static inline void net_queue_read_at(const net_queue_t *queue, size_t offset,
    uint8_t *dst, size_t size) {
    const size_t start = (queue->head + offset) % queue->capacity;
    const size_t first = size < queue->capacity - start ? size : queue->capacity - start;
    memcpy(dst, queue->data + start, first);
    memcpy(dst + first, queue->data, size - first);
}

static inline bool net_queue_push(net_queue_t *queue, const uint8_t *data, size_t size) {
    if (size > UINT32_MAX - 4U || queue->capacity - queue->used < size + 4U) { return false; }
    const uint32_t length = (uint32_t)size;
    uint8_t prefix[4] = {(uint8_t)(length & 0xFFU), (uint8_t)((length >> 8) & 0xFFU),
        (uint8_t)((length >> 16) & 0xFFU), (uint8_t)(length >> 24)};
    net_queue_write_at(queue, queue->used, prefix, 4U);
    net_queue_write_at(queue, queue->used + 4U, data, size);
    queue->used += size + 4U;
    queue->count += 1U;
    return true;
}

/* Drops every queued message whose first byte is `first_byte`, keeping the
   others in order: compaction in place, front to back, through a small
   window, so no message-sized buffer is needed. */
static inline void net_queue_drop_kind(net_queue_t *queue, uint8_t first_byte) {
    size_t read = 0U;
    size_t write = 0U;
    size_t kept = 0U;
    while (read < queue->used) {
        uint8_t prefix[5];
        net_queue_read_at(queue, read, prefix, 5U);
        const size_t size = (size_t)prefix[0] | ((size_t)prefix[1] << 8) | ((size_t)prefix[2] << 16) |
            ((size_t)prefix[3] << 24);
        const size_t total = size + 4U;
        if (size == 0U || prefix[4] != first_byte) {
            for (size_t offset = 0U; write != read && offset < total;) {
                uint8_t window[64];
                const size_t chunk = total - offset < sizeof window ? total - offset : sizeof window;
                net_queue_read_at(queue, read + offset, window, chunk);
                net_queue_write_at(queue, write + offset, window, chunk);
                offset += chunk;
            }
            write += total;
            kept += 1U;
        }
        read += total;
    }
    queue->used = write;
    queue->count = kept;
}

/* Size of the front message, or 0 when the queue is empty. */
static inline size_t net_queue_front_size(const net_queue_t *queue) {
    if (queue->count == 0U) { return 0U; }
    uint8_t prefix[4];
    net_queue_read_at(queue, 0U, prefix, 4U);
    return (size_t)prefix[0] | ((size_t)prefix[1] << 8) | ((size_t)prefix[2] << 16) |
        ((size_t)prefix[3] << 24);
}

/* Copies the front message; `dst` must hold net_queue_front_size() bytes. */
static inline void net_queue_front_copy(const net_queue_t *queue, uint8_t *dst, size_t size) {
    net_queue_read_at(queue, 4U, dst, size);
}

static inline void net_queue_pop(net_queue_t *queue) {
    const size_t size = net_queue_front_size(queue) + 4U;
    queue->head = (queue->head + size) % queue->capacity;
    queue->used -= size;
    queue->count -= 1U;
}

#endif
