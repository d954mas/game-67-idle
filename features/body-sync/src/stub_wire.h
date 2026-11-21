#ifndef BODY_SYNC_STUB_WIRE_H
#define BODY_SYNC_STUB_WIRE_H

/* The stub's bytes: every field raw and little-endian, so a reader of a capture sees the
   numbers. Box3D-free, so its parser is fuzzed alone under sanitizers. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    STUB_WIRE_VERSION = 0xB7,
    STUB_HEADER_BYTES = 3, /* version, body count */
    /* id 4, flags 1, locks 1, position 12, rotation 16 | velocities 24 | seat 1, number 2, hold 1 | holder 4 */
    STUB_COPY_BYTES = 34,
    STUB_SIM_BYTES = 24,
    STUB_AUTHORITY_BYTES = 4,
    STUB_HELD_BYTES = 4,
    STUB_BODY_BYTES = STUB_COPY_BYTES + STUB_SIM_BYTES + STUB_AUTHORITY_BYTES + STUB_HELD_BYTES,
    /* The flags byte: the owner's four bits, then these; bits 5 and 6 are reserved and refused. */
    STUB_FLAG_KINEMATIC = 0x10,
    STUB_FLAG_ASLEEP = 0x80,
    STUB_FLAG_RESERVED = 0x60,
    /* The locks byte: linear x y z, angular x y z from bit 0; bits 6 and 7 are refused. */
    STUB_LOCKS_ALL = 0x3F,
    STUB_RECORD_BYTES_MAX = 18,
    STUB_SLOTS = 1024, /* BODY_SYNC_BODIES_MAX; asserted in the stub */
};

/* What a peer's numbers must stay within: the seam's config. */
typedef struct stub_limits {
    float bounds_min[3];
    float bounds_max[3];
    float max_speed;
    float max_spin;
} stub_limits_t;

typedef struct stub_body_state {
    uint32_t id;
    uint8_t seat;
    uint16_t number;
    uint8_t hold;
    uint32_t holder;
    uint8_t flags; /* the owner's low bits */
    bool kinematic;
    bool asleep;
    uint8_t locks; /* STUB_LOCKS_ALL bits: the owner's motion locks */
    float position[3];
    float rotation[4]; /* unit: the reader normalises what passed the tolerance */
    float velocity[3];
    float angular[3];
} stub_body_state_t;

/* Brings what this side is about to write inside the limits, so it always passes the peer's
   checks and FORMAT meets only a peer that breaks the rules: a position is clamped to the box,
   a velocity or spin scaled down to its limit, a velocity that is not a number sent as rest.
   False when the pose itself is not a number: that body is not written at all. */
bool stub_wire_limit(const stub_limits_t *limits, stub_body_state_t *body);

/* Bodies that fit a packet of `capacity` bytes. */
uint32_t stub_wire_bodies_fit(uint32_t capacity);
void stub_wire_put_header(uint8_t *buffer, uint16_t count);
void stub_wire_put_body(uint8_t *at, const stub_body_state_t *body);
/* Checks the whole packet before handing out any body: false names the first problem and sets
   *count to 0. */
bool stub_wire_read_state(const uint8_t *data, uint32_t size, uint32_t seats, const stub_limits_t *limits, stub_body_state_t *out,
                          uint32_t max, uint32_t *count, char *error, size_t error_size);

typedef enum stub_record_type {
    STUB_REC_CLAIM = 1, /* guest to host: id, hold, sequence */
    STUB_REC_RELEASE,   /* guest to host: id, the number it saw, its last claim's sequence */
    /* host to a guest: id, seat, number, hold, holder, forced, the claim of that guest it answers */
    STUB_REC_ASSIGN,
} stub_record_type_t;

typedef struct stub_record {
    stub_record_type_t type;
    uint32_t id;
    uint8_t seat;
    uint16_t number;   /* RELEASE: the sender's; ASSIGN: the new one */
    uint8_t hold;
    uint16_t sequence; /* CLAIM: the claim's; RELEASE: the last claim's; ASSIGN: the answered claim's, 0 when none */
    uint32_t holder;   /* ASSIGN */
    uint8_t forced;    /* ASSIGN: the move answers no claim */
    /* RELEASE and ASSIGN: the body as its owner left it, so a handover never waits for the
       unreliable state that would carry them. motion_known false: the sender never saw it. */
    uint8_t flags;
    bool motion_known;
    bool kinematic;
    uint8_t locks;
} stub_record_t;

uint32_t stub_wire_put_record(const stub_record_t *record, uint8_t *buffer, uint32_t capacity);
bool stub_wire_read_record(const uint8_t *data, uint32_t size, uint32_t seats, bool from_host, stub_record_t *out, char *error, size_t error_size);

#endif
