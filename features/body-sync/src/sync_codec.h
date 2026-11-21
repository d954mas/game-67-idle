#ifndef BODY_SYNC_SYNC_CODEC_H
#define BODY_SYNC_SYNC_CODEC_H

/* The real seam's state packet on net-bits: one table per part of a record, ranges taken from
   the seam's limits, precisions fixed here. Pure C, no Box3D: the sender and the receiver hand
   it records, and its tests run alone.

   A packet is a run of records, each preceded by one "more" bit; a zero bit ends it and the
   last byte is padded with zero bits (anything else is refused). The session's own prefix
   carries the packet number, the tick and the acknowledgements, so the seam adds no header.

   One record:
     head       slot 10 bits, generation low SYNC_GEN_BITS, sim 1, rest 1, rare 1
     pose       copy: position on a 1/512 m grid, rotation smallest three at 9 bits
                sim:  position on a 1/4096 m grid, rotation smallest three at 15 bits
     motion     sim and not at rest: velocity on a 1/32 m/s grid in [-max_speed, max_speed],
                spin on a 1/16 rad/s grid in [-max_spin, max_spin]; absent at rest (zero)
     rare       when the rare bit is set: a 4-bit group mask, then each group present in order
                  motion     kinematic 1, motion locks 6
                  flags      the caller's BODY_SYNC_FLAG_BITS
                  authority  seat 2, number 16, hold 1
                  holder     present 1, then slot 10 and generation low SYNC_GEN_BITS
   A position grid starts at the box's low edge and ends at its last point inside the high
   edge; a velocity grid is symmetric with zero on it and no point past the limit. The writer
   puts every value on its grid and keeps a velocity or spin vector no longer than its limit,
   so nothing a reader accepts lies outside the limits. Decoded and encoded again, positions,
   velocities and rare groups give the same bits; a rotation gives the same rotation within its
   step, not always the same bits (near a tie of its two largest components the dropped one may
   change, which net-bits does not promise against). A reader refuses a longer vector. A packet of the end bit alone is
   legal and holds nothing (the writer sends none). Records are not byte aligned.

   The grids hold while every coordinate of the box has a magnitude below 2048 m (a float
   cannot keep 1/4096 m apart past it), max_speed is below 262144 m/s and max_spin below
   524288 rad/s; sync_codec_init refuses limits past them. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "features/body_sync/body_sync_types.h"
#include "features/net_bits/net_bits_record.h"

enum {
    SYNC_SLOT_BITS = 10, /* BODY_SYNC_BODIES_MAX */
    /* Generation bits on the wire: a stale record whose generation matches modulo 16 reads as
       current. Records are newest-wins per sender, so only a pose sent before a respawn and
       read after it could pass. */
    SYNC_GEN_BITS = 4,
    SYNC_GEN_MASK = (1 << SYNC_GEN_BITS) - 1,
    SYNC_RARE_GROUPS = 4,
    SYNC_PROTOCOL = 1,
};
/* Macros, not enum members: gcc warns when one anonymous enum is compared with net-bits' own. */
#define SYNC_COPY_QUAT_BITS 9
#define SYNC_SIM_QUAT_BITS 15
#define SYNC_COPY_POSITION_STEP (1.0f / 512.0f)
#define SYNC_SIM_POSITION_STEP (1.0f / 4096.0f)
#define SYNC_VELOCITY_STEP (1.0f / 32.0f)
#define SYNC_SPIN_STEP (1.0f / 16.0f)

typedef enum sync_rare_group {
    SYNC_RARE_MOTION = 1u << 0,
    SYNC_RARE_FLAGS = 1u << 1,
    SYNC_RARE_AUTHORITY = 1u << 2,
    SYNC_RARE_HOLDER = 1u << 3,
    SYNC_RARE_ALL = 0x0Fu,
} sync_rare_group_t;

/* The seam's limits, as body_sync_config_t gives them. */
typedef struct sync_limits {
    float bounds_min[3];
    float bounds_max[3];
    float max_speed;
    float max_spin;
} sync_limits_t;

typedef struct sync_rare {
    uint8_t groups; /* sync_rare_group_t bits carried; the members of other groups are unset */
    bool kinematic;
    uint8_t locks; /* linear x y z, angular x y z from bit 0 */
    uint8_t flags; /* below 1 << BODY_SYNC_FLAG_BITS */
    uint8_t seat;
    uint16_t number;
    uint8_t hold; /* body_sync_hold_t */
    uint16_t holder_slot; /* BODY_SYNC_BODIES_MAX: no holder */
    uint8_t holder_generation_low;
} sync_rare_t;

typedef struct sync_record {
    uint16_t slot;
    uint8_t generation_low; /* the id's generation & SYNC_GEN_MASK */
    bool sim;               /* velocities follow unless at_rest */
    bool at_rest;           /* the owner's body is asleep: this pose holds until it wakes */
    float position[3];
    float rotation[4]; /* x, y, z, w */
    float velocity[3]; /* sim only; zero at rest */
    float angular[3];
    sync_rare_t rare;
} sync_record_t;

/* The tables of one seam, built once at create from its limits. */
typedef struct sync_codec {
    sync_limits_t limits;
    net_bits_field_t head_fields[5], copy_fields[4], sim_fields[4], motion_fields[6];
    net_bits_field_t mask_fields[1], rmotion_fields[2], rflags_fields[1], rauthority_fields[3], rholder_fields[3];
    net_bits_layout_t head, copy_pose, sim_pose, motion, rare_mask, rare_motion, rare_flags, rare_authority, rare_holder;
    uint32_t record_bits_max; /* a sim record in motion with every rare group, its "more" bit included */
    uint32_t record_bits_min; /* a copy record with no rare group */
    uint64_t hash;            /* every table, the generation width and the protocol */
} sync_codec_t;

/* False when a grid would not fit net-bits (see above), the limits are not finite or a speed
   is below one step, or the widest record exceeds BODY_SYNC_RECORD_MAX bytes: the seam's
   create then returns NULL. Owns no memory; the codec must not move after init. */
bool sync_codec_init(sync_codec_t *codec, const sync_limits_t *limits);

/* Brings a record inside the limits before it is written, as body_sync_config_t promises: a
   position clamped to the box, a velocity or spin scaled down to its limit, a velocity that is
   not a number sent as rest, and false (do not send) when the pose is not a number. */
bool sync_codec_limit(const sync_codec_t *codec, sync_record_t *record);

/* Bits the record takes on the wire, its "more" bit included. */
uint32_t sync_codec_record_bits(const sync_codec_t *codec, const sync_record_t *record);

/* What the other side reads for `record` (positions, rotation, velocities through their
   tables, clamped as the reader clamps): the sender keeps this as "last sent", and a relay
   forwards it without decoding twice. The record must have passed sync_codec_limit. */
void sync_codec_quantize(const sync_codec_t *codec, sync_record_t *record);

typedef struct sync_packet_writer {
    net_bits_writer_t bits;
    uint32_t count;
} sync_packet_writer_t;

void sync_packet_begin(sync_packet_writer_t *writer, uint8_t *buffer, uint32_t capacity);
/* False and nothing written when the record does not fit what is left with the end bit: a
   record never splits across packets. */
bool sync_packet_add(sync_packet_writer_t *writer, const sync_codec_t *codec, const sync_record_t *record);
/* Bytes written; zero when no record was added. */
uint32_t sync_packet_finish(sync_packet_writer_t *writer);

/* Decodes a whole packet before handing out any record: false, *count 0 and a reason when the
   bytes end early, a code lies outside its table, a rare bit comes with an empty mask, a seat
   or a speed is out of range, the end bit is missing, padding bits are set, or more than `max`
   records arrive. Reads nothing outside `data` and never asserts. The caller still checks
   slots and generations against its bodies. */
bool sync_packet_read(const sync_codec_t *codec, uint8_t seats, const uint8_t *data, uint32_t size, sync_record_t *out, uint32_t max, uint32_t *count,
                      char *error, size_t error_size);

#endif
