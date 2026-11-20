#ifndef FEATURES_NET_BITS_NET_BITS_RECORD_H
#define FEATURES_NET_BITS_NET_BITS_RECORD_H

#include "features/net_bits/net_bits.h"

/* A record is a C struct and one table that says how each member travels: its range and
   precision, hence its bits. Writing and reading both walk that table, so they cannot
   disagree, and a precision or a field changes in one place.

   The table's hash names the wire format. Two builds whose tables differ get different
   hashes; a session compares them in its handshake and refuses the peer with a reason
   instead of reading its packets as noise. The hash covers what is on the wire (kinds,
   order, ranges, widths) and nothing that is not (member offsets, names). */

typedef enum net_bits_field_kind {
    NET_BITS_FIELD_BOOL = 1, /* bool: 1 bit */
    NET_BITS_FIELD_UINT,     /* uint32_t: `bits` wide */
    NET_BITS_FIELD_INT,      /* int32_t in [int_min, int_max] */
    NET_BITS_FIELD_RANGED,   /* float on `range` */
    NET_BITS_FIELD_QUAT,     /* float[4] x, y, z, w: smallest three, `bits` per component */
} net_bits_field_kind_t;

typedef struct net_bits_field {
    uint8_t kind;    /* net_bits_field_kind_t */
    uint8_t bits;    /* UINT, QUAT */
    uint16_t offset; /* of the member in the record struct */
    int32_t int_min; /* INT */
    int32_t int_max;
    net_bits_range_t range; /* RANGED */
} net_bits_field_t;

/* One line of a table per member, as an initializer of a `net_bits_field_t` array, with
   constant arguments. The member's type must be the one its kind names (bool, uint32_t,
   int32_t, float, float[4]); the macros refuse any other type, an offset past 16 bits and a
   width out of range (UINT 1..32, QUAT within the quaternion limits) at compile time. */
#define NET_BITS_BOOL(type, member)                                                                                    \
    {.kind = NET_BITS_FIELD_BOOL, .offset = NET_BITS_OFFSET_(type, member, bool *, "bool", 1, "")}
#define NET_BITS_UINT(type, member, width)                                                                             \
    {.kind = NET_BITS_FIELD_UINT,                                                                                      \
     .bits = (width),                                                                                                  \
     .offset = NET_BITS_OFFSET_(type, member, uint32_t *, "uint32_t", (width) >= 1 && (width) <= 32,                   \
                                " needs a UINT width of 1..32")}
#define NET_BITS_INT(type, member, min, max)                                                                           \
    {.kind = NET_BITS_FIELD_INT,                                                                                       \
     .offset = NET_BITS_OFFSET_(type, member, int32_t *, "int32_t", 1, ""),                                            \
     .int_min = (min),                                                                                                 \
     .int_max = (max)}
#define NET_BITS_RANGED(type, member, min, max, step)                                                                  \
    {.kind = NET_BITS_FIELD_RANGED,                                                                                    \
     .offset = NET_BITS_OFFSET_(type, member, float *, "float", 1, ""),                                                \
     .range = {(float)(min), (float)(max), (float)(step)}}
#define NET_BITS_QUAT(type, member, width)                                                                             \
    {.kind = NET_BITS_FIELD_QUAT,                                                                                      \
     .bits = (width),                                                                                                  \
     .offset = NET_BITS_OFFSET_(type, member, float(*)[4], "float[4]",                                                 \
                                (width) >= NET_BITS_QUAT_BITS_MIN && (width) <= NET_BITS_QUAT_BITS_MAX,                \
                                " needs a QUAT width within the quaternion limits")}

/* The checks sit in a struct inside sizeof: the one place C11 lets _Static_assert stand in
   an expression. _Generic on the member's address tells float[4] from float *. */
#define NET_BITS_OFFSET_(type, member, pointer, name, width_ok, width_rule)                                            \
    (uint16_t)(offsetof(type, member) +                                                                                \
               0U * sizeof(struct {                                                                                    \
                   _Static_assert(_Generic(&((type *)0)->member, pointer: 1, default: 0),                              \
                                  #type "." #member " must be " name);                                                 \
                   _Static_assert(offsetof(type, member) <= UINT16_MAX, #type "." #member " lies past a 16-bit offset"); \
                   _Static_assert(width_ok, #type "." #member width_rule);                                             \
                   char unused;                                                                                        \
               }))

typedef struct net_bits_layout {
    const net_bits_field_t *fields; /* the caller keeps the table alive */
    uint32_t count;
    uint32_t bits; /* of one record */
    uint64_t hash;
} net_bits_layout_t;

/* False when a line is malformed (an unknown kind, a width out of range, an empty or
   inverted range, a member that does not lie inside `record_size` bytes) and for a table
   that is empty or puts no bits on the wire. A layout that failed is left with no bits, and
   the record calls refuse it. */
bool net_bits_layout_init(net_bits_layout_t *layout, const net_bits_field_t *fields, uint32_t count, size_t record_size);

/* Clears `ok` without writing when the record does not fit the room left or the layout has
   no bits; a member value its line does not allow clears it mid-record. */
void net_bits_write_record(net_bits_writer_t *writer, const net_bits_layout_t *layout, const void *record);

/* False, with the reader's `ok` cleared, when the bytes end early, carry a code outside its
   field's table, or the layout has no bits. A record whose read failed must not be used:
   members before the failure may be set, the failing member and those after it keep the
   caller's old bytes, and nothing outside the members is written. */
bool net_bits_read_record(net_bits_reader_t *reader, const net_bits_layout_t *layout, void *record);

/* Folds another number into a hash: several layouts and a protocol version become the one
   value a handshake compares. Order matters. */
uint64_t net_bits_hash_mix(uint64_t hash, uint64_t value);

#endif
