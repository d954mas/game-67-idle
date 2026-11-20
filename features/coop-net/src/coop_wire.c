#include "coop_wire.h"

#include "features/coop_net/coop_session.h"
#include "features/net_bits/net_bits.h"

#include <string.h>

static void write_u64(net_bits_writer_t *writer, uint64_t value) {
    net_bits_write(writer, (uint32_t)value, 32);
    net_bits_write(writer, (uint32_t)(value >> 32), 32);
}

static uint64_t read_u64(net_bits_reader_t *reader) {
    uint64_t low = net_bits_read(reader, 32);
    uint64_t high = net_bits_read(reader, 32);
    return low | (high << 32);
}

/* The zero bits up to the next byte; false when one is set. */
static bool read_padding(net_bits_reader_t *reader) {
    unsigned pad = (unsigned)((8U - (reader->bits % 8U)) % 8U);
    if (pad > 0 && net_bits_read(reader, pad) != 0) {
        return false;
    }
    return reader->ok;
}

uint32_t coop_wire_header_bytes(coop_wire_kind_t kind) {
    switch (kind) {
    case COOP_WIRE_HELLO: return COOP_WIRE_HELLO_BYTES;
    case COOP_WIRE_WELCOME: return COOP_WIRE_WELCOME_BYTES;
    case COOP_WIRE_JOINED: return COOP_WIRE_JOINED_BYTES;
    case COOP_WIRE_LEFT: return COOP_WIRE_LEFT_BYTES;
    case COOP_WIRE_RECORD: return COOP_WIRE_RECORD_BYTES;
    case COOP_WIRE_STATE: return COOP_WIRE_STATE_BYTES;
    case COOP_WIRE_BASELINE_FIRST: return COOP_WIRE_BASELINE_FIRST_BYTES;
    case COOP_WIRE_BASELINE_PIECE: return COOP_WIRE_BASELINE_PIECE_BYTES;
    }
    return 0;
}

bool coop_wire_route(coop_wire_kind_t kind, uint8_t *lane, bool *reliable) {
    switch (kind) {
    case COOP_WIRE_HELLO:
    case COOP_WIRE_WELCOME:
    case COOP_WIRE_JOINED:
    case COOP_WIRE_LEFT:
    case COOP_WIRE_RECORD: *lane = COOP_LANE_RECORDS; *reliable = true; return true;
    case COOP_WIRE_STATE: *lane = COOP_LANE_STATE; *reliable = false; return true;
    case COOP_WIRE_BASELINE_FIRST:
    case COOP_WIRE_BASELINE_PIECE: *lane = COOP_LANE_BASELINE; *reliable = true; return true;
    }
    return false;
}

static bool from_host_only(coop_wire_kind_t kind) {
    return kind == COOP_WIRE_WELCOME || kind == COOP_WIRE_JOINED || kind == COOP_WIRE_LEFT || kind == COOP_WIRE_BASELINE_FIRST || kind == COOP_WIRE_BASELINE_PIECE;
}

/* Kinds whose bytes run to the end of the packet; the others end at their header. */
static bool carries_bytes(coop_wire_kind_t kind) {
    return kind != COOP_WIRE_JOINED && kind != COOP_WIRE_LEFT;
}

uint32_t coop_wire_write(const coop_wire_msg_t *msg, uint8_t *out, uint32_t capacity) {
    if (!msg || !out) {
        return 0;
    }
    uint32_t header = coop_wire_header_bytes(msg->kind);
    if (header == 0 || capacity < header) {
        return 0;
    }
    bool keyed = msg->kind == COOP_WIRE_HELLO || msg->kind == COOP_WIRE_WELCOME;
    if (carries_bytes(msg->kind)) {
        if (msg->size > 0 && !msg->bytes) {
            return 0;
        }
        if (!keyed && msg->kind != COOP_WIRE_STATE && msg->size == 0) {
            return 0;
        }
        if (keyed && msg->size > COOP_WORLD_KEY_MAX) {
            return 0;
        }
        if (msg->size > capacity - header) {
            return 0;
        }
    }
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, out, header);
    net_bits_write(&writer, (uint32_t)msg->kind, COOP_WIRE_KIND_BITS);
    switch (msg->kind) {
    case COOP_WIRE_HELLO:
        net_bits_write(&writer, COOP_WIRE_MAGIC, 32);
        net_bits_write(&writer, msg->protocol, 16);
        write_u64(&writer, msg->build_key);
        write_u64(&writer, msg->player_key);
        net_bits_write(&writer, msg->baseline_max, 32);
        net_bits_write_int(&writer, (int32_t)msg->size, 0, COOP_WORLD_KEY_MAX);
        break;
    case COOP_WIRE_WELCOME:
        if (msg->seats < 2 || msg->seat < 1 || msg->seat >= msg->seats || msg->tick_hz < 1) {
            return 0;
        }
        net_bits_write_int(&writer, msg->seat, 0, COOP_SEATS_MAX - 1);
        net_bits_write_int(&writer, msg->seats, 1, COOP_SEATS_MAX);
        net_bits_write(&writer, msg->tick, 32);
        net_bits_write(&writer, msg->tick_hz, 16);
        net_bits_write_int(&writer, (int32_t)msg->size, 0, COOP_WORLD_KEY_MAX);
        break;
    case COOP_WIRE_JOINED:
        net_bits_write_int(&writer, msg->seat, 0, COOP_SEATS_MAX - 1);
        write_u64(&writer, msg->identity);
        break;
    case COOP_WIRE_LEFT:
        if (msg->seat == COOP_SEAT_HOST) {
            return 0;
        }
        net_bits_write_int(&writer, msg->seat, 0, COOP_SEATS_MAX - 1);
        net_bits_write(&writer, msg->reason, 32);
        break;
    case COOP_WIRE_RECORD:
        net_bits_write(&writer, msg->tick, 32);
        break;
    case COOP_WIRE_STATE:
        if (!msg->has_ack && (msg->ack != 0 || msg->ack_mask != 0 || msg->ack_wait != 0)) {
            return 0;
        }
        net_bits_write(&writer, msg->number, 16);
        net_bits_write(&writer, msg->tick, 32);
        net_bits_write(&writer, msg->has_ack ? 1U : 0U, 1);
        net_bits_write(&writer, msg->ack, 16);
        net_bits_write(&writer, msg->ack_mask, 32);
        net_bits_write(&writer, msg->ack_wait, 16);
        break;
    case COOP_WIRE_BASELINE_FIRST:
        if (msg->total == 0 || msg->size > msg->total) {
            return 0;
        }
        net_bits_write(&writer, msg->total, 32);
        net_bits_write(&writer, msg->tick, 32);
        net_bits_write(&writer, msg->records_before, 32);
        break;
    case COOP_WIRE_BASELINE_PIECE:
        break;
    }
    if (net_bits_writer_finish(&writer) != header) {
        return 0;
    }
    if (!carries_bytes(msg->kind)) {
        return header;
    }
    if (msg->size > 0) {
        memcpy(out + header, msg->bytes, msg->size);
    }
    return header + msg->size;
}

bool coop_wire_parse(const uint8_t *data, uint32_t size, uint8_t lane, bool reliable, bool from_host, coop_wire_msg_t *out) {
    if (!out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (!data || size == 0) {
        return false;
    }
    net_bits_reader_t reader;
    net_bits_reader_init(&reader, data, size);
    coop_wire_kind_t kind = (coop_wire_kind_t)net_bits_read(&reader, COOP_WIRE_KIND_BITS);
    uint8_t kind_lane = 0;
    bool kind_reliable = false;
    if (!reader.ok || !coop_wire_route(kind, &kind_lane, &kind_reliable)) {
        return false;
    }
    bool direction_ok = from_host ? kind != COOP_WIRE_HELLO : !from_host_only(kind);
    if (kind_lane != lane || kind_reliable != reliable || !direction_ok) {
        return false;
    }
    out->kind = kind;
    uint32_t key_size = 0;
    switch (kind) {
    case COOP_WIRE_HELLO:
        if (net_bits_read(&reader, 32) != COOP_WIRE_MAGIC) {
            return false;
        }
        out->protocol = net_bits_read(&reader, 16);
        if (!reader.ok) {
            return false;
        }
        /* Kind, magic and protocol are the same in every protocol: a peer of another one is
           refused for its version, whatever follows. */
        if (out->protocol != COOP_SESSION_PROTOCOL) {
            return true;
        }
        out->build_key = read_u64(&reader);
        out->player_key = read_u64(&reader);
        out->baseline_max = net_bits_read(&reader, 32);
        key_size = (uint32_t)net_bits_read_int(&reader, 0, COOP_WORLD_KEY_MAX);
        break;
    case COOP_WIRE_WELCOME:
        out->seat = (uint8_t)net_bits_read_int(&reader, 0, COOP_SEATS_MAX - 1);
        out->seats = (uint8_t)net_bits_read_int(&reader, 1, COOP_SEATS_MAX);
        out->tick = net_bits_read(&reader, 32);
        out->tick_hz = net_bits_read(&reader, 16);
        key_size = (uint32_t)net_bits_read_int(&reader, 0, COOP_WORLD_KEY_MAX);
        if (out->seats < 2 || out->seat < 1 || out->seat >= out->seats || out->tick_hz < 1) {
            return false;
        }
        break;
    case COOP_WIRE_JOINED:
        out->seat = (uint8_t)net_bits_read_int(&reader, 0, COOP_SEATS_MAX - 1);
        out->identity = read_u64(&reader);
        break;
    case COOP_WIRE_LEFT:
        out->seat = (uint8_t)net_bits_read_int(&reader, 0, COOP_SEATS_MAX - 1);
        out->reason = net_bits_read(&reader, 32);
        if (out->seat == COOP_SEAT_HOST) {
            return false;
        }
        break;
    case COOP_WIRE_RECORD:
        out->tick = net_bits_read(&reader, 32);
        break;
    case COOP_WIRE_STATE:
        out->number = (uint16_t)net_bits_read(&reader, 16);
        out->tick = net_bits_read(&reader, 32);
        out->has_ack = net_bits_read(&reader, 1) != 0;
        out->ack = (uint16_t)net_bits_read(&reader, 16);
        out->ack_mask = net_bits_read(&reader, 32);
        out->ack_wait = (uint16_t)net_bits_read(&reader, 16);
        if (!out->has_ack && (out->ack != 0 || out->ack_mask != 0 || out->ack_wait != 0)) {
            return false;
        }
        break;
    case COOP_WIRE_BASELINE_FIRST:
        out->total = net_bits_read(&reader, 32);
        out->tick = net_bits_read(&reader, 32);
        out->records_before = net_bits_read(&reader, 32);
        if (out->total == 0) {
            return false;
        }
        break;
    case COOP_WIRE_BASELINE_PIECE:
        break;
    }
    if (!read_padding(&reader)) {
        return false;
    }
    uint32_t header = (uint32_t)(reader.bits / 8U);
    if (!carries_bytes(kind)) {
        return net_bits_reader_done(&reader);
    }
    uint32_t rest = size - header;
    if (kind == COOP_WIRE_HELLO || kind == COOP_WIRE_WELCOME) {
        if (rest != key_size) {
            return false;
        }
    } else if (rest == 0 && kind != COOP_WIRE_STATE) {
        return false;
    }
    if (kind == COOP_WIRE_BASELINE_FIRST && rest > out->total) {
        return false;
    }
    out->bytes = rest > 0 ? data + header : NULL;
    out->size = rest;
    return true;
}
