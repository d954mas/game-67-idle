#ifndef NET_BITS_TEST_SUPPORT_H
#define NET_BITS_TEST_SUPPORT_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* splitmix64: a fixed seed per test, so a failure replays. */
static uint64_t rng_state;

static inline void rng_seed(uint64_t seed) { rng_state = seed; }

static inline uint64_t rng_next(void) {
    uint64_t z = (rng_state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static inline uint32_t rng_u32(void) { return (uint32_t)(rng_next() >> 32); }

/* [0, count), count > 0 */
static inline uint32_t rng_below(uint32_t count) { return (uint32_t)(((rng_next() >> 32) * count) >> 32); }

/* [0, 1) */
static inline double rng_unit(void) { return (double)(rng_next() >> 11) * 0x1p-53; }

/* Writes outside the given bytes land in the guards; reads past them are left to ASan, so
   the guards sit in the same allocation only on the writer side. */
enum { GUARD_BYTES = 16, GUARD_FILL = 0xA5 };

typedef struct guarded {
    uint8_t *block;
    uint8_t *data;
    size_t size;
} guarded_t;

static inline guarded_t guarded_alloc(size_t size, uint8_t fill) {
    guarded_t guarded = {malloc(size + 2U * GUARD_BYTES), NULL, size};
    memset(guarded.block, GUARD_FILL, size + 2U * GUARD_BYTES);
    guarded.data = guarded.block + GUARD_BYTES;
    memset(guarded.data, fill, size);
    return guarded;
}

static inline bool guarded_intact(const guarded_t *guarded) {
    for (size_t i = 0U; i < GUARD_BYTES; i++) {
        if (guarded->block[i] != GUARD_FILL || guarded->data[guarded->size + i] != GUARD_FILL) {
            return false;
        }
    }
    return true;
}

static inline void guarded_free(guarded_t *guarded) { free(guarded->block); }

/* Exactly `size` bytes on the heap, so ASan sees a read one byte past the end. */
static inline uint8_t *exact_copy(const uint8_t *source, size_t size) {
    if (size == 0U) {
        return NULL;
    }
    uint8_t *copy = malloc(size);
    memcpy(copy, source, size);
    return copy;
}

/* Angle of the rotation between unit `expected` and `got` (normalised here); q and -q are
   one rotation. The chord form stays accurate near zero, where acos does not. */
static inline double rotation_angle(const double expected[4], const float got[4]) {
    double norm = 0.0;
    for (int i = 0; i < 4; i++) {
        norm += (double)got[i] * (double)got[i];
    }
    norm = sqrt(norm);
    double same = 0.0;
    double flipped = 0.0;
    for (int i = 0; i < 4; i++) {
        const double g = (double)got[i] / norm;
        same += (g - expected[i]) * (g - expected[i]);
        flipped += (g + expected[i]) * (g + expected[i]);
    }
    const double chord = sqrt(fmin(same, flipped));
    return 4.0 * asin(fmin(1.0, 0.5 * chord));
}

static inline bool quat_is_unit(const float q[4]) {
    double norm = 0.0;
    for (int i = 0; i < 4; i++) {
        if (!isfinite(q[i])) {
            return false;
        }
        norm += (double)q[i] * (double)q[i];
    }
    return fabs(sqrt(norm) - 1.0) <= 1e-6;
}

#endif
