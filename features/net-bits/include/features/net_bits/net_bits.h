#ifndef FEATURES_NET_BITS_NET_BITS_H
#define FEATURES_NET_BITS_NET_BITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Packing game state into the fewest bits a stated precision needs, and reading it back from
   bytes nobody vouches for. A packet from a peer is untrusted: a read past the end, a width
   out of range or a value outside its table clears `ok` and returns a harmless value; the
   caller checks `ok` once at the end and drops the packet. Nothing here asserts on data and
   nothing reads or writes outside the buffer it was given.

   The caller's side: `data` is valid for `size` (or `capacity`) bytes; a record layout is
   used only after its init returned true, its table outlives it, and a record passed with it
   is of the table's type. A writer or reader whose fields were set by hand past its buffer,
   or with no data and a non-zero size, refuses every call that would touch memory.

   Both sides run the same arithmetic on the same table, so they agree on every number:
   everything on the wire and every value read is bit-identical between builds that evaluate
   in IEEE single and double without contraction or fast-math. Fast-math, finite-math-only and
   excess precision are refused at compile time; contraction is turned off by the target's
   build flag or the clang pragma; the known-answer tests catch a build that still evaluates
   differently.

   Bit order on the wire: the first bit written is the least significant bit of byte 0, and a
   value of several bits goes low bits first. README.md states every rule a second
   implementation needs. */

typedef struct net_bits_writer {
    uint8_t *data;
    size_t capacity; /* bytes */
    size_t bits;     /* written so far */
    bool ok;
} net_bits_writer_t;

void net_bits_writer_init(net_bits_writer_t *writer, uint8_t *data, size_t capacity);

/* The low `bits` of `value`, 1..32. A value that does not fit the width, a width out of
   range or no room clears `ok`: a writer never truncates silently. */
void net_bits_write(net_bits_writer_t *writer, uint32_t value, unsigned bits);

/* Bits that still fit: a packet builder asks before it starts a record. */
size_t net_bits_writer_room(const net_bits_writer_t *writer);

/* Fills the last byte with zero bits and returns the size in bytes; zero when not ok. */
size_t net_bits_writer_finish(net_bits_writer_t *writer);

typedef struct net_bits_reader {
    const uint8_t *data;
    size_t size; /* bytes */
    size_t bits; /* read so far */
    bool ok;
} net_bits_reader_t;

/* Every reader returns a fixed harmless value on a refusal and after one, so a caller may
   rely on it: `net_bits_read` 0, `read_int` min, `read_ranged` min (0 for a malformed range), `read_quat` no rotation.
   A parsing loop runs on `ok` and `net_bits_reader_left`, never on `net_bits_reader_done`
   alone: done is the final check of a whole packet. */
void net_bits_reader_init(net_bits_reader_t *reader, const uint8_t *data, size_t size);
uint32_t net_bits_read(net_bits_reader_t *reader, unsigned bits);
size_t net_bits_reader_left(const net_bits_reader_t *reader);

/* Ok, and nothing is left but the zero bits that filled the last byte: spare bytes or set
   padding bits are a malformed packet, not slack. */
bool net_bits_reader_done(const net_bits_reader_t *reader);

/* Bits that tell `count` values apart; zero for a count of zero or one. */
unsigned net_bits_width(uint32_t count);

/* A whole number in [min, max] in the bits that range needs. Writing a value outside the
   range clears `ok`; so does reading a code past `max`, which the spare codes of a width
   allow on the wire, and either call with min above max. */
void net_bits_write_int(net_bits_writer_t *writer, int32_t value, int32_t min, int32_t max);
int32_t net_bits_read_int(net_bits_reader_t *reader, int32_t min, int32_t max);

/* A real number on a grid: min, min + step, ... through the first point at or past max,
   within rounding. Inside [min, max] the value read differs from the value written by at
   most half a step plus the float rounding of the value read: half a float spacing there,
   nothing when grid points are exact floats (a power-of-two step with min on it). A value
   midway between two points may take either, as double arithmetic rounds it. Below min it
   lands on min, past the last grid point on that point, and a value that is not a number on
   min. */
typedef struct net_bits_range {
    float min;
    float max;
    float step;
} net_bits_range_t;

/* Zero when the range is malformed: not finite, max not above min, step not positive, a grid
   point past the float range, or a step finer than two float spacings at the grid's largest
   magnitude (a float could not hold the grid points apart, and a relayed value would drift).
   The last rule caps a grid near 2^24 points. */
unsigned net_bits_range_width(net_bits_range_t range);

/* What the other side reads for `value`; zero for a malformed range. A grid point reads
   back as itself, so a relayed value never drifts. */
float net_bits_quantize(net_bits_range_t range, float value);
void net_bits_write_ranged(net_bits_writer_t *writer, net_bits_range_t range, float value);
/* A code past the last grid point clears `ok`: a read never yields a value outside the grid. */
float net_bits_read_ranged(net_bits_reader_t *reader, net_bits_range_t range);

/* A rotation as its three smaller components ("smallest three"): two bits name the dropped
   one, then `bits` for each of the others, 2 + 3 * bits in all. Quaternions are x, y, z, w.
   The writer accepts any length and treats zero or non-finite input as no rotation. Each
   component has an odd number of points on [-1/sqrt(2), 1/sqrt(2)] with zero on the grid;
   the one spare code clears `ok`. The reader always returns a unit quaternion when `ok`,
   whatever the bits were, and no rotation when not.

   A rotation read is not guaranteed to re-encode to the same codes: near a tie of the
   largest component the dropped index can flip, and the error then adds up per hop. A relay
   forwards the value it decoded once and never re-encodes a re-decoded one. */
enum { NET_BITS_QUAT_BITS_MIN = 4, NET_BITS_QUAT_BITS_MAX = 20 };
void net_bits_write_quat(net_bits_writer_t *writer, const float quat[4], unsigned bits);
void net_bits_read_quat(net_bits_reader_t *reader, float out[4], unsigned bits);

/* An upper bound, in radians, on the angle between a rotation and what the other side reads
   at this width: the number to pick `bits` by. Pi for a width outside the limits. */
float net_bits_quat_error(unsigned bits);

#endif
