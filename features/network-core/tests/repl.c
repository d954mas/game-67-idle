/* The varint bounds and the replication outbox: what a refused packet keeps,
   what a sent one acknowledges, and the packet the client reads back. */
#include "net_codec.h"
#include "net_repl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) \
    do { if (!(condition)) { fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); exit(1); } } while (0)

static void varint_bounds(void) {
    static const uint32_t values[] = {0U, 1U, 127U, 128U, 16383U, 16384U, 0x0FFFFFFFU, 0x10000000U, UINT32_MAX};
    static const size_t sizes[] = {1U, 1U, 1U, 2U, 2U, 3U, 4U, 5U, 5U};
    for (size_t index = 0U; index < sizeof values / sizeof values[0]; ++index) {
        uint8_t bytes[NET_VARINT_MAX];
        uint32_t back = 0U;
        const size_t size = net_varint_put(bytes, values[index]);
        CHECK(size == sizes[index]);
        CHECK(net_varint_get(bytes, size, &back) == size && back == values[index]);
        CHECK(net_varint_get(bytes, size - 1U, &back) == 0U); /* truncated */
    }
    uint32_t value = 7U;
    CHECK(net_varint_get((const uint8_t[]){0x80U, 0x00U}, 2U, &value) == 0U); /* a zero-padded tail */
    CHECK(net_varint_get((const uint8_t[]){0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x10U}, 5U, &value) == 0U); /* past 32 bits */
    CHECK(net_varint_get((const uint8_t[]){0x80U, 0x80U, 0x80U, 0x80U, 0x80U, 0x01U}, 6U, &value) == 0U); /* a sixth byte */
    CHECK(net_varint_get((const uint8_t[]){0x00U}, 0U, &value) == 0U);
    CHECK(value == 7U);
}

static net_repl_event_t event_of(uint32_t tick, bool reliable, uint8_t audience, uint8_t mark) {
    net_repl_event_t event = {.tick = tick, .audience = audience, .reliable = reliable, .owner = 1U, .district = 2U, .size = 3U};
    memset(event.bytes, mark, event.size);
    return event;
}

typedef struct packet_t {
    uint32_t tick, input_seq, input_wait, baseline, events;
    uint32_t back[8];
    uint8_t mark[8];
} packet_t;

static packet_t read_packet(const uint8_t *bytes, size_t size, size_t rows) {
    packet_t packet = {0};
    size_t at = 0U;
    uint32_t *head[] = {&packet.tick, &packet.input_seq, &packet.input_wait, &packet.baseline};
    for (size_t index = 0U; index < 4U; ++index) {
        const size_t read = net_varint_get(bytes + at, size - at, head[index]);
        CHECK(read > 0U);
        at += read;
    }
    at += rows;
    size_t read = net_varint_get(bytes + at, size - at, &packet.events);
    CHECK(read > 0U && packet.events <= 8U);
    at += read;
    for (uint32_t index = 0U; index < packet.events; ++index) {
        read = net_varint_get(bytes + at, size - at, &packet.back[index]);
        CHECK(read > 0U && size - at >= read + 3U);
        packet.mark[index] = bytes[at + read];
        at += read + 3U;
    }
    CHECK(at == size);
    return packet;
}

static void outbox_ack_and_refuse(void) {
    static uint8_t out[NET_REPL_PACKET_CAP_MIN + 4U];
    static net_repl_viewer_t viewer;
    const net_repl_knobs_t knobs = {.bytes_per_tick = 1200U, .interest_radius = 10.0F};
    const uint8_t rows[4] = {9U, 9U, 9U, 9U};
    net_repl_viewer_init(&viewer, 1U);
    viewer.district = 2U;
    CHECK(!net_repl_admits(&viewer, &(net_repl_event_t){.audience = NET_REPL_AUDIENCE_OWNER, .owner = 2U}, &knobs));
    CHECK(!net_repl_admits(&viewer, &(net_repl_event_t){.audience = NET_REPL_AUDIENCE_NEARBY, .district = 2U, .x = 11.0F}, &knobs));
    CHECK(net_repl_admits(&viewer, &(net_repl_event_t){.audience = NET_REPL_AUDIENCE_ALL}, &knobs));

    const net_repl_event_t tick10[] = {event_of(10U, true, NET_REPL_AUDIENCE_OWNER, 0xA1U),
        event_of(10U, false, NET_REPL_AUDIENCE_NEARBY, 0xC1U), event_of(10U, true, NET_REPL_AUDIENCE_ALL, 0xA2U)};
    CHECK(net_repl_queue(&viewer, tick10, 3U, &knobs) && viewer.outbox_count == 2U);
    net_repl_built_t built;
    size_t size = net_repl_build(&viewer, 10U, 5U, NET_REPL_WAIT_UNKNOWN, rows, sizeof rows, tick10, 3U, &knobs, out, sizeof out, &built);
    CHECK(built.reliable_sent == 2U && built.cosmetic_sent == 1U && built.rows_at == 4U);
    CHECK(memcmp(out + built.rows_at, rows, sizeof rows) == 0);
    packet_t packet = read_packet(out, size, sizeof rows);
    CHECK(packet.tick == 10U && packet.input_seq == 5U && packet.baseline == 0U && packet.events == 3U);
    CHECK(packet.mark[0] == 0xA1U && packet.mark[1] == 0xA2U && packet.mark[2] == 0xC1U);

    /* Refused: the baseline and the reliable events stay, the cosmetic one is lost. */
    net_repl_refused(&viewer, &built);
    CHECK(viewer.cosmetic_dropped == 1U && viewer.outbox_count == 2U && !viewer.has_baseline);

    /* Tick 11 resends the waiting events with their origin tick, then is taken. */
    const net_repl_event_t tick11[] = {event_of(11U, true, NET_REPL_AUDIENCE_OWNER, 0xA3U)};
    CHECK(net_repl_queue(&viewer, tick11, 1U, &knobs) && viewer.outbox_count == 3U);
    size = net_repl_build(&viewer, 11U, 6U, 1U, rows, sizeof rows, tick11, 1U, &knobs, out, sizeof out, &built);
    packet = read_packet(out, size, sizeof rows);
    CHECK(packet.events == 3U && packet.back[0] == 1U && packet.back[2] == 0U && packet.mark[2] == 0xA3U);
    /* An event queued after the build is not acknowledged by its send. */
    const net_repl_event_t late[] = {event_of(11U, true, NET_REPL_AUDIENCE_ALL, 0xA4U)};
    CHECK(net_repl_queue(&viewer, late, 1U, &knobs));
    net_repl_sent(&viewer, 11U, &built);
    CHECK(viewer.has_baseline && viewer.baseline_tick == 11U && viewer.outbox_count == 1U && viewer.outbox[0].bytes[0] == 0xA4U);
    size = net_repl_build(&viewer, 12U, 6U, 1U, NULL, 0U, NULL, 0U, &knobs, out, sizeof out, &built);
    packet = read_packet(out, size, 0U);
    CHECK(packet.baseline == 12U && packet.events == 1U);

    /* A budget too small for a cosmetic event drops it; reliable ones never are. */
    const net_repl_knobs_t tight = {.bytes_per_tick = 8U, .interest_radius = 10.0F};
    const net_repl_event_t cosmetic[] = {event_of(12U, false, NET_REPL_AUDIENCE_ALL, 0xC2U)};
    size = net_repl_build(&viewer, 12U, 6U, 1U, NULL, 0U, cosmetic, 1U, &tight, out, sizeof out, &built);
    CHECK(built.reliable_sent == 1U && built.cosmetic_sent == 0U && viewer.cosmetic_dropped == 2U);

    /* An outbox that cannot take the tick's events asks for a resync. */
    net_repl_reset(&viewer);
    CHECK(viewer.outbox_count == 0U && !viewer.has_baseline && viewer.cosmetic_dropped == 2U);
    net_repl_event_t flood[NET_REPL_OUTBOX_EVENTS + 1U];
    for (uint32_t index = 0U; index <= NET_REPL_OUTBOX_EVENTS; ++index) { flood[index] = event_of(12U, true, NET_REPL_AUDIENCE_ALL, 1U); }
    CHECK(net_repl_queue(&viewer, flood, NET_REPL_OUTBOX_EVENTS, &knobs));
    CHECK(!net_repl_queue(&viewer, flood, 1U, &knobs));
    /* A full outbox of the largest events still fits the minimum cap. */
    net_repl_reset(&viewer);
    for (uint32_t index = 0U; index < NET_REPL_OUTBOX_EVENTS; ++index) { flood[index].size = NET_REPL_EVENT_MAX_BYTES; }
    CHECK(net_repl_queue(&viewer, flood, NET_REPL_OUTBOX_EVENTS, &knobs));
    size = net_repl_build(&viewer, UINT32_MAX, UINT32_MAX, NET_REPL_WAIT_MAX_MS + 1U, NULL, 0U, NULL, 0U, &knobs, out,
        NET_REPL_PACKET_CAP_MIN, &built);
    CHECK(size <= NET_REPL_PACKET_CAP_MIN && built.reliable_sent == NET_REPL_OUTBOX_EVENTS);
}

int main(void) {
    varint_bounds();
    outbox_ack_and_refuse();
    puts("repl: ok");
    return 0;
}
