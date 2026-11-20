#if defined(_WIN32) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "features/net_link/net_link_capture.h"

#include "features/net_link/net_link_transport.h"
#include "net_link_capture_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CAPTURE_HEADER_BYTES = 8,
    CAPTURE_RECORD_BYTES = 24,
    /* No transport sends a message this large; a size past it marks a damaged file. */
    CAPTURE_RECORD_MAX = 1 << 24,
};

struct net_link_capture {
    FILE *file;
    uint32_t flags;
    uint64_t time_us;
    bool ok;
};

struct net_link_capture_reader {
    FILE *file;
    uint32_t flags;
    uint64_t last_time_us;
    uint8_t *payload;
    uint32_t payload_capacity;
    bool failed;
    bool ended;
};

static void put_u16(uint8_t *at, uint16_t value) {
    at[0] = (uint8_t)value;
    at[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *at, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        at[i] = (uint8_t)(value >> (8 * i));
    }
}

static void put_u64(uint8_t *at, uint64_t value) {
    for (int i = 0; i < 8; i++) {
        at[i] = (uint8_t)(value >> (8 * i));
    }
}

static uint16_t get_u16(const uint8_t *at) { return (uint16_t)(at[0] | (at[1] << 8)); }

static uint32_t get_u32(const uint8_t *at) {
    uint32_t value = 0;
    for (int i = 3; i >= 0; i--) {
        value = (value << 8) | at[i];
    }
    return value;
}

static uint64_t get_u64(const uint8_t *at) {
    uint64_t value = 0;
    for (int i = 7; i >= 0; i--) {
        value = (value << 8) | at[i];
    }
    return value;
}

net_link_capture_t *net_link_capture_open(const char *path, uint32_t flags) {
    if (!path || (flags & ~(uint32_t)NET_LINK_CAPTURE_PAYLOAD) != 0) {
        return NULL;
    }
    net_link_capture_t *capture = calloc(1, sizeof(*capture));
    if (!capture) {
        return NULL;
    }
    capture->file = fopen(path, "wb");
    uint8_t header[CAPTURE_HEADER_BYTES] = {'N', 'L', 'C', 'P'};
    put_u16(header + 4, NET_LINK_CAPTURE_VERSION);
    put_u16(header + 6, (uint16_t)flags);
    if (!capture->file || fwrite(header, 1, sizeof(header), capture->file) != sizeof(header)) {
        if (capture->file) {
            fclose(capture->file);
        }
        free(capture);
        return NULL;
    }
    capture->flags = flags;
    capture->ok = true;
    return capture;
}

void net_link_capture_close(net_link_capture_t *capture) {
    if (!capture) {
        return;
    }
    fclose(capture->file);
    free(capture);
}

void net_link_capture_set_time(net_link_capture_t *capture, double seconds) {
    if (!capture || !(seconds > 0.0)) {
        return;
    }
    const double us = seconds * 1e6;
    const uint64_t time_us = us >= 1.8e19 ? UINT64_MAX : (uint64_t)us;
    /* A reader may rely on order, so a clock that steps back holds the last time. */
    if (time_us > capture->time_us) {
        capture->time_us = time_us;
    }
}

bool net_link_capture_ok(const net_link_capture_t *capture) { return capture && capture->ok; }

void net_link_capture_attach(net_link_t *link, net_link_capture_t *capture, uint8_t tag) {
    if (link) {
        link->capture = capture;
        link->capture_tag = tag;
    }
}

void net_link_capture_record(net_link_capture_t *capture, uint8_t tag, net_link_capture_direction_t direction, uint64_t identity, uint8_t lane, bool reliable, const void *data, uint32_t size) {
    if (!capture->ok) {
        return;
    }
    uint8_t record[CAPTURE_RECORD_BYTES];
    put_u64(record, capture->time_us);
    put_u64(record + 8, identity);
    put_u32(record + 16, size);
    record[20] = tag;
    record[21] = (uint8_t)direction;
    record[22] = lane;
    record[23] = reliable ? 1 : 0;
    bool written = fwrite(record, 1, sizeof(record), capture->file) == sizeof(record);
    if (written && (capture->flags & NET_LINK_CAPTURE_PAYLOAD) != 0) {
        written = fwrite(data, 1, size, capture->file) == size;
    }
    capture->ok = written;
}

net_link_capture_reader_t *net_link_capture_reader_open(const char *path) {
    if (!path) {
        return NULL;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    uint8_t header[CAPTURE_HEADER_BYTES];
    if (fread(header, 1, sizeof(header), file) != sizeof(header) || memcmp(header, "NLCP", 4) != 0 || get_u16(header + 4) != NET_LINK_CAPTURE_VERSION ||
        (get_u16(header + 6) & ~(uint32_t)NET_LINK_CAPTURE_PAYLOAD) != 0) {
        fclose(file);
        return NULL;
    }
    net_link_capture_reader_t *reader = calloc(1, sizeof(*reader));
    if (!reader) {
        fclose(file);
        return NULL;
    }
    reader->file = file;
    reader->flags = get_u16(header + 6);
    return reader;
}

void net_link_capture_reader_close(net_link_capture_reader_t *reader) {
    if (!reader) {
        return;
    }
    fclose(reader->file);
    free(reader->payload);
    free(reader);
}

static bool reader_fail(net_link_capture_reader_t *reader) {
    reader->failed = true;
    return false;
}

bool net_link_capture_reader_next(net_link_capture_reader_t *reader, net_link_capture_record_t *out) {
    if (!reader || !out || reader->failed || reader->ended) {
        return false;
    }
    uint8_t record[CAPTURE_RECORD_BYTES];
    const size_t got = fread(record, 1, sizeof(record), reader->file);
    if (got == 0 && feof(reader->file)) {
        reader->ended = true;
        return false;
    }
    if (got != sizeof(record)) {
        return reader_fail(reader);
    }
    const uint64_t time_us = get_u64(record);
    const uint32_t size = get_u32(record + 16);
    if (time_us < reader->last_time_us || size == 0 || size > CAPTURE_RECORD_MAX || record[21] > NET_LINK_CAPTURE_IN || record[22] >= NET_LINK_LANES_MAX || record[23] > 1) {
        return reader_fail(reader);
    }
    const uint8_t *payload = NULL;
    if ((reader->flags & NET_LINK_CAPTURE_PAYLOAD) != 0) {
        if (size > reader->payload_capacity) {
            uint8_t *grown = realloc(reader->payload, size);
            if (!grown) {
                return reader_fail(reader);
            }
            reader->payload = grown;
            reader->payload_capacity = size;
        }
        if (fread(reader->payload, 1, size, reader->file) != size) {
            return reader_fail(reader);
        }
        payload = reader->payload;
    }
    reader->last_time_us = time_us;
    out->time_s = (double)time_us * 1e-6;
    out->identity = get_u64(record + 8);
    out->size = size;
    out->tag = record[20];
    out->direction = record[21];
    out->lane = record[22];
    out->reliable = record[23] != 0;
    out->payload = payload;
    return true;
}

bool net_link_capture_reader_failed(const net_link_capture_reader_t *reader) { return reader && reader->failed; }
