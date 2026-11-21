#ifndef BODY_SYNC_SYNC_MASKS_H
#define BODY_SYNC_SYNC_MASKS_H

/* Both shells: a LIGHT copy's shapes stop colliding with this endpoint's own characters
   (character_category). The seam takes those bits off each shape's mask once, at the copy's
   start, and later puts back exactly the bits it took, by shape id; whatever else the caller
   did to a copy's filters meanwhile stays. */

#include <stdbool.h>
#include <stdint.h>

#include "features/body_sync/body_sync.h"

typedef struct sync_masks {
    bool saved;
    uint8_t count;
    b3ShapeId shapes[BODY_SYNC_SHAPES_MAX];
    uint64_t taken[BODY_SYNC_SHAPES_MAX]; /* the category bits that shape had and lost */
} sync_masks_t;

static inline int sync_masks_shapes(b3BodyId body) { return b3Body_GetShapeCount(body); }

/* Takes `category` off every shape's mask. Nothing when already taken or no category. */
static inline void sync_masks_clear(sync_masks_t *m, b3BodyId body, uint64_t category) {
    if (m->saved || !category) {
        return;
    }
    int n = b3Body_GetShapes(body, m->shapes, BODY_SYNC_SHAPES_MAX);
    m->count = (uint8_t)(n < 0 ? 0 : n);
    for (int i = 0; i < m->count; i++) {
        b3Filter f = b3Shape_GetFilter(m->shapes[i]);
        m->taken[i] = f.maskBits & category;
        if (m->taken[i]) {
            f.maskBits &= ~category;
            b3Shape_SetFilter(m->shapes[i], f, true);
        }
    }
    m->saved = true;
}

/* Puts back the bits taken, on the shapes still on the body; a shape added since is left as
   the caller made it. */
static inline void sync_masks_restore(sync_masks_t *m, b3BodyId body) {
    if (!m->saved) {
        return;
    }
    for (int i = 0; i < m->count; i++) {
        if (!m->taken[i] || !b3Shape_IsValid(m->shapes[i])) {
            continue;
        }
        b3BodyId owner = b3Shape_GetBody(m->shapes[i]);
        b3Filter f = b3Shape_GetFilter(m->shapes[i]);
        if (B3_ID_EQUALS(owner, body) && (f.maskBits & m->taken[i]) != m->taken[i]) {
            f.maskBits |= m->taken[i];
            b3Shape_SetFilter(m->shapes[i], f, true);
        }
    }
    m->saved = false;
}

#endif
