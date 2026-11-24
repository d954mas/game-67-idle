#ifndef GARMENT_DEBUG_H
#define GARMENT_DEBUG_H

#include "garment/cloth.h"

enum {
    CLOTH_DEBUG_WEIGHTS = 1U,
    CLOTH_DEBUG_TETHERS = 2U,
    CLOTH_DEBUG_SHAPES = 4U,
    CLOTH_DEBUG_SKELETON = 8U,
    CLOTH_DEBUG_REFERENCE = 16U,
    CLOTH_DEBUG_MAX_DISTANCE = 32U,
    CLOTH_DEBUG_BACKSTOP = 64U
};

/* Endpoints are in the caller's simulation space. Consume or copy them during the callback. */
typedef void (*ClothDebugLine)(void *user, unsigned layer, const float a[3], const float b[3], const float rgb[3]);

/* Supply a valid cooked mesh and its posed vertices. NULL weights shows each vertex's max distance relative to the
   largest. */
void cloth_debug_mesh(const ClothMesh *mesh, const float positions[][3], const float *weights,
                      unsigned layers, ClothDebugLine emit, void *user);
/* REFERENCE draws the reference wire, MAX_DISTANCE each vertex's offset from its reference coloured by its share of
   r (red beyond it), BACKSTOP the body direction from the reference to the backstop surface. */
/* max_distance and backstop are the runtime ones (NULL: the mesh's), as the solver used them. */
void cloth_debug_reference(const ClothMesh *mesh, const float positions[][3], const ClothReference *ref,
                           const float *max_distance, const ClothBackstop *backstop, unsigned layers, ClothDebugLine emit,
                           void *user);
void cloth_debug_cone(const ClothSphere *a, const ClothSphere *b, ClothDebugLine emit, void *user);
void cloth_debug_bone(const float parent[3], const float joint[3], ClothDebugLine emit, void *user);

#endif
