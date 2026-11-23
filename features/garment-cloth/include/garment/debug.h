#ifndef GARMENT_DEBUG_H
#define GARMENT_DEBUG_H

#include "garment/cloth.h"

enum {
    CLOTH_DEBUG_WEIGHTS = 1U,
    CLOTH_DEBUG_TETHERS = 2U,
    CLOTH_DEBUG_CAPSULES = 4U,
    CLOTH_DEBUG_SKELETON = 8U
};

/* Endpoints are in the caller's simulation space. Consume or copy them during the callback. */
typedef void (*ClothDebugLine)(void *user, unsigned layer, const float a[3], const float b[3], const float rgb[3]);

/* Supply a valid cooked mesh and its posed vertices. NULL weights shows each vertex's drive relative to the strongest. */
void cloth_debug_mesh(const ClothMesh *mesh, const float positions[][3], const float *weights,
                      unsigned layers, ClothDebugLine emit, void *user);
void cloth_debug_capsule(const ClothCapsule *capsule, ClothDebugLine emit, void *user);
void cloth_debug_bone(const float parent[3], const float joint[3], ClothDebugLine emit, void *user);

#endif
