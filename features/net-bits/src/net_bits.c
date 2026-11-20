#include "features/net_bits/net_bits.h"

#include <float.h>
#include <math.h>

/* Peers must reconstruct the same bits: no fast-math, no excess precision, no fused
   multiply-add (GCC gets -ffp-contract=off from the build and warns on the pragma). */
#if defined(__FAST_MATH__) || defined(_M_FP_FAST) || defined(_M_FP_CONTRACT) ||                                         (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__)
#error "net_bits needs IEEE arithmetic: build it without fast-math, finite-math-only or fp:contract"
#endif
#if defined(FLT_EVAL_METHOD) && FLT_EVAL_METHOD != 0
#error "net_bits needs float and double evaluated in their own precision (FLT_EVAL_METHOD 0)"
#endif
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#endif

/* 1/sqrt(2) as the nearest double: the largest a non-dropped component of a unit quaternion
   can be when the dropped one is the largest. */
#define QUAT_REACH 0.70710678118654752440

static size_t capacity_bits(size_t bytes) { return bytes > SIZE_MAX / 8U ? SIZE_MAX : bytes * 8U; }

static unsigned bits_for(uint32_t span) {
    unsigned width = 0U;
    while (span != 0U) {
        width++;
        span >>= 1;
    }
    return width;
}

// #region bit stream

/* The structs are public, so a position or size set by hand is checked before memory is:
   room and left are zero for an insane struct, and every access goes through them. */
static bool writer_sane(const net_bits_writer_t *writer) {
    return (writer->data != NULL || writer->capacity == 0U) && writer->bits <= capacity_bits(writer->capacity);
}

static bool reader_sane(const net_bits_reader_t *reader) {
    return (reader->data != NULL || reader->size == 0U) && reader->bits <= capacity_bits(reader->size);
}

void net_bits_writer_init(net_bits_writer_t *writer, uint8_t *data, size_t capacity) {
    writer->data = data;
    writer->capacity = capacity;
    writer->bits = 0U;
    writer->ok = true;
}

size_t net_bits_writer_room(const net_bits_writer_t *writer) {
    const size_t capacity = capacity_bits(writer->capacity);
    return writer->ok && writer_sane(writer) ? capacity - writer->bits : 0U;
}

void net_bits_write(net_bits_writer_t *writer, uint32_t value, unsigned bits) {
    if (!writer->ok) {
        return;
    }
    if (bits == 0U || bits > 32U || (bits < 32U && (value >> bits) != 0U) || net_bits_writer_room(writer) < bits) {
        writer->ok = false;
        return;
    }
    size_t pos = writer->bits;
    unsigned left = bits;
    while (left > 0U) {
        const unsigned shift = (unsigned)(pos & 7U);
        const unsigned take = 8U - shift < left ? 8U - shift : left;
        uint8_t *byte = &writer->data[pos >> 3];
        /* Bits above the position are cleared, not merged: a writer restored from a copy
           writes over what was written after that copy. */
        const unsigned written = (1U << shift) - 1U;
        *byte = (uint8_t)(((unsigned)*byte & written) | ((value & ((1U << take) - 1U)) << shift));
        value >>= take;
        pos += take;
        left -= take;
    }
    writer->bits = pos;
}

size_t net_bits_writer_finish(net_bits_writer_t *writer) {
    if (!writer->ok || !writer_sane(writer)) {
        writer->ok = false;
        return 0U;
    }
    const unsigned used = (unsigned)(writer->bits & 7U);
    if (used != 0U) {
        uint8_t *byte = &writer->data[writer->bits >> 3];
        *byte = (uint8_t)((unsigned)*byte & ((1U << used) - 1U));
    }
    return (writer->bits >> 3) + (used != 0U ? 1U : 0U);
}

void net_bits_reader_init(net_bits_reader_t *reader, const uint8_t *data, size_t size) {
    reader->data = data;
    reader->size = size;
    reader->bits = 0U;
    reader->ok = true;
}

size_t net_bits_reader_left(const net_bits_reader_t *reader) {
    const size_t size = capacity_bits(reader->size);
    return reader->ok && reader_sane(reader) ? size - reader->bits : 0U;
}

uint32_t net_bits_read(net_bits_reader_t *reader, unsigned bits) {
    if (!reader->ok) {
        return 0U;
    }
    if (bits == 0U || bits > 32U || net_bits_reader_left(reader) < bits) {
        reader->ok = false;
        return 0U;
    }
    uint32_t value = 0U;
    unsigned got = 0U;
    size_t pos = reader->bits;
    while (got < bits) {
        const unsigned shift = (unsigned)(pos & 7U);
        const unsigned take = 8U - shift < bits - got ? 8U - shift : bits - got;
        const uint32_t part = ((uint32_t)reader->data[pos >> 3] >> shift) & ((1U << take) - 1U);
        value |= part << got;
        pos += take;
        got += take;
    }
    reader->bits = pos;
    return value;
}

bool net_bits_reader_done(const net_bits_reader_t *reader) {
    if (!reader->ok || !reader_sane(reader)) {
        return false;
    }
    const size_t left = net_bits_reader_left(reader);
    if (left >= 8U) {
        return false;
    }
    if (left == 0U) {
        return true;
    }
    return ((unsigned)reader->data[reader->bits >> 3] >> (reader->bits & 7U)) == 0U;
}

unsigned net_bits_width(uint32_t count) { return count <= 1U ? 0U : bits_for(count - 1U); }

// #endregion

// #region whole numbers

void net_bits_write_int(net_bits_writer_t *writer, int32_t value, int32_t min, int32_t max) {
    if (!writer->ok) {
        return;
    }
    /* Also refuses min above max: no value is then both at least min and at most max. */
    if (value < min || value > max) {
        writer->ok = false;
        return;
    }
    const unsigned width = bits_for((uint32_t)((int64_t)max - (int64_t)min));
    if (width > 0U) {
        net_bits_write(writer, (uint32_t)((int64_t)value - (int64_t)min), width);
    }
}

int32_t net_bits_read_int(net_bits_reader_t *reader, int32_t min, int32_t max) {
    if (!reader->ok) {
        return min;
    }
    if (min > max) {
        reader->ok = false;
        return min;
    }
    const uint32_t span = (uint32_t)((int64_t)max - (int64_t)min);
    const unsigned width = bits_for(span);
    if (width == 0U) {
        return min;
    }
    const uint32_t code = net_bits_read(reader, width);
    if (!reader->ok || code > span) {
        reader->ok = false;
        return min;
    }
    return (int32_t)((int64_t)min + (int64_t)code);
}

// #endregion

// #region ranged reals

typedef struct grid {
    double min;
    double step;
    uint32_t last; /* code of the first point at or past max */
} grid_t;

/* Spacing of floats at magnitude `x`, taken from the top of its binade. */
static double float_spacing(double x) {
    if (x < (double)FLT_MIN) {
        return ldexp(1.0, -149);
    }
    int exponent = 0;
    (void)frexp(x, &exponent);
    return ldexp(1.0, exponent - 24);
}

/* A step of at least two float spacings keeps every decoded point within a quarter step of
   its exact value, so it re-encodes to its own code; it also bounds `last` by 2^24, since
   the span is at most twice the reach. */
static bool grid_of(net_bits_range_t range, grid_t *grid) {
    const double min = (double)range.min;
    const double max = (double)range.max;
    const double step = (double)range.step;
    if (!isfinite(min) || !isfinite(max) || !isfinite(step) || !(max > min) || !(step > 0.0)) {
        return false;
    }
    const double last = ceil((max - min) / step);
    const double reach = fmax(fabs(min), fabs(min + last * step));
    if (!(reach <= (double)FLT_MAX) || step < 2.0 * float_spacing(reach)) {
        return false;
    }
    grid->min = min;
    grid->step = step;
    grid->last = (uint32_t)last;
    return true;
}

static uint32_t grid_code(const grid_t *grid, float value) {
    const double steps = ((double)value - grid->min) / grid->step;
    if (!(steps > 0.0)) {
        return 0U;
    }
    if (steps >= (double)grid->last) {
        return grid->last;
    }
    const double code = floor(steps + 0.5);
    return code >= (double)grid->last ? grid->last : (uint32_t)code;
}

static float grid_value(const grid_t *grid, uint32_t code) { return (float)(grid->min + (double)code * grid->step); }

unsigned net_bits_range_width(net_bits_range_t range) {
    grid_t grid;
    return grid_of(range, &grid) ? bits_for(grid.last) : 0U;
}

float net_bits_quantize(net_bits_range_t range, float value) {
    grid_t grid;
    return grid_of(range, &grid) ? grid_value(&grid, grid_code(&grid, value)) : 0.0F;
}

void net_bits_write_ranged(net_bits_writer_t *writer, net_bits_range_t range, float value) {
    if (!writer->ok) {
        return;
    }
    grid_t grid;
    if (!grid_of(range, &grid)) {
        writer->ok = false;
        return;
    }
    net_bits_write(writer, grid_code(&grid, value), bits_for(grid.last));
}

float net_bits_read_ranged(net_bits_reader_t *reader, net_bits_range_t range) {
    grid_t grid;
    const bool valid = grid_of(range, &grid);
    if (!reader->ok) {
        return valid ? range.min : 0.0F;
    }
    if (!valid) {
        reader->ok = false;
        return 0.0F;
    }
    const uint32_t code = net_bits_read(reader, bits_for(grid.last));
    if (!reader->ok || code > grid.last) {
        reader->ok = false;
        return range.min;
    }
    return grid_value(&grid, code);
}

// #endregion

// #region quaternion

static void quat_identity(float out[4]) {
    out[0] = 0.0F;
    out[1] = 0.0F;
    out[2] = 0.0F;
    out[3] = 1.0F;
}

/* Codes 0 .. 2 * mid; code mid is zero exactly, so an axis-aligned rotation reads back exact. */
static uint32_t quat_mid(unsigned bits) { return (1U << (bits - 1U)) - 1U; }

static bool quat_bits_valid(unsigned bits) { return bits >= NET_BITS_QUAT_BITS_MIN && bits <= NET_BITS_QUAT_BITS_MAX; }

void net_bits_write_quat(net_bits_writer_t *writer, const float quat[4], unsigned bits) {
    if (!writer->ok) {
        return;
    }
    if (!quat_bits_valid(bits) || net_bits_writer_room(writer) < 2U + 3U * bits) {
        writer->ok = false;
        return;
    }
    double q[4] = {0.0, 0.0, 0.0, 1.0};
    double scale = 0.0;
    bool finite = true;
    for (int i = 0; i < 4; i++) {
        finite = finite && isfinite(quat[i]);
        scale = fmax(scale, fabs((double)quat[i]));
    }
    if (finite && scale > 0.0) {
        /* Scaled first so a huge but finite length cannot overflow the sum of squares. */
        double norm = 0.0;
        for (int i = 0; i < 4; i++) {
            q[i] = (double)quat[i] / scale;
            norm += q[i] * q[i];
        }
        norm = sqrt(norm);
        for (int i = 0; i < 4; i++) {
            q[i] /= norm;
        }
    }
    unsigned drop = 0U;
    for (unsigned i = 1U; i < 4U; i++) {
        if (fabs(q[i]) > fabs(q[drop])) {
            drop = i;
        }
    }
    const double sign = q[drop] < 0.0 ? -1.0 : 1.0;
    const uint32_t mid = quat_mid(bits);
    const double unit = QUAT_REACH / (double)mid;
    net_bits_write(writer, drop, 2U);
    for (unsigned i = 0U; i < 4U; i++) {
        if (i == drop) {
            continue;
        }
        const double steps = floor(sign * q[i] / unit + 0.5);
        uint32_t code = 0U;
        if (steps >= (double)mid) {
            code = 2U * mid;
        } else if (steps > -(double)mid) {
            code = (uint32_t)(steps + (double)mid);
        }
        net_bits_write(writer, code, bits);
    }
}

void net_bits_read_quat(net_bits_reader_t *reader, float out[4], unsigned bits) {
    quat_identity(out);
    if (!reader->ok) {
        return;
    }
    if (!quat_bits_valid(bits)) {
        reader->ok = false;
        return;
    }
    const uint32_t mid = quat_mid(bits);
    const double unit = QUAT_REACH / (double)mid;
    const unsigned drop = net_bits_read(reader, 2U);
    double q[4] = {0.0, 0.0, 0.0, 0.0};
    double sum = 0.0;
    for (unsigned i = 0U; i < 4U; i++) {
        if (i == drop) {
            continue;
        }
        const uint32_t code = net_bits_read(reader, bits);
        if (code > 2U * mid) {
            reader->ok = false;
        }
        q[i] = ((double)code - (double)mid) * unit;
        sum += q[i] * q[i];
    }
    if (!reader->ok) {
        return;
    }
    /* Any bit pattern must give a unit rotation: three components at the corner of the grid
       sum past one, so the dropped term clamps at zero and the four are scaled back. */
    const double rest = 1.0 - sum;
    q[drop] = rest > 0.0 ? sqrt(rest) : 0.0;
    const double norm = sqrt(sum + q[drop] * q[drop]);
    for (int i = 0; i < 4; i++) {
        out[i] = (float)(q[i] / norm);
    }
}

/* Derivation in README.md, "Rotation error bound": the worst case is the corner
   (1/2, 1/2, 1/2, 1/2) with every component error radial; the bound is tight to first order. */
float net_bits_quat_error(unsigned bits) {
    if (!quat_bits_valid(bits)) {
        return 3.14159274F; /* pi rounded up: no rotation is further away */
    }
    const double unit = QUAT_REACH / (double)quat_mid(bits);
    const double radial = sqrt(3.0) * 0.5 * unit;
    const double corner = sqrt(0.75);
    const double reach = corner + radial;
    const double dropped = radial * (2.0 * corner + radial) / (0.5 + sqrt(fmax(0.0, 1.0 - reach * reach)));
    const double chord = sqrt(radial * radial + dropped * dropped) + ldexp(1.0, -22);
    const double angle = 4.0 * asin(fmin(1.0, 0.5 * chord));
    float bound = (float)angle;
    if ((double)bound < angle) {
        bound = nextafterf(bound, INFINITY);
    }
    return bound;
}

// #endregion
