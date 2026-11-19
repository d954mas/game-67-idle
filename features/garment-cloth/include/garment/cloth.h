#ifndef GARMENT_CLOTH_H
#define GARMENT_CLOTH_H

#include <stdbool.h>
#include <stdint.h>

enum { CLOTH_VERTICES = 512, CLOTH_EDGES = 1536, CLOTH_MAX_STRAIN_PASSES = 32 };

typedef struct { uint16_t a, b, c; } ClothFace;
/* c,d are opposite vertices of an interior hinge; d < 0 marks a boundary. */
typedef struct { int a, b, c, d; float length, angle; } ClothEdge;
typedef struct { int pin; float max_length; } ClothTether;
typedef struct {
    int vertices, faces, edges;
    const float (*rest)[3];
    const float *inv_area; /* Zero at a sew point, otherwise reciprocal lumped vertex area (m^-2). */
    const ClothFace *face;
    const ClothEdge *edge;
    const ClothTether *tether; /* Optional per-vertex path-length limit; pin = -1 on unattached islands. */
} ClothMesh;
typedef struct {
    float a[3], b[3], radius;
} ClothCapsule;
typedef struct {
    float density, stretch, bend, drag, thickness;
    int iterations;
    float max_edge_strain; /* Fractional extension above rest length during strain limiting. */
    int strain_passes; /* Zero disables the optional one-sided projection. */
} ClothMaterial;
typedef struct { float normal[3], travel, backstop; } ClothGuide;
typedef struct {
    const ClothMesh *mesh; /* Immutable cooked mesh accepted by the last reset. */
    bool frame_valid;
    float x[CLOTH_VERTICES][3], previous[CLOTH_VERTICES][3];
    float stretch_lambda[CLOTH_EDGES], bend_lambda[CLOTH_EDGES];
} ClothState;
typedef struct {
    float stretch, penetration, pin_error, speed;
    bool finite;
} ClothMetrics;

/* All points are in one Y-up meter space. Anchors and capsules must be sampled at substep end. */
bool cloth_mesh_valid(const ClothMesh *mesh);
bool cloth_frame_valid(const ClothMesh *mesh, const ClothMaterial *m, float dt,
                       const float anchors[][3], const ClothGuide *guide, const ClothCapsule *body, int count);
void cloth_reset(ClothState *s, const ClothMesh *mesh, const float pose[][3]);
void cloth_step(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                const float anchors[][3], const ClothGuide *guide, const ClothCapsule *body, int count);
/* The final pose envelope has priority over body contact and tethers. NULL guide follows cloth_step. */
void cloth_step_shaped(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                       const float anchors[][3], const ClothGuide *guide, const ClothCapsule *body, int count);
/* Separate output storage, offsets in meters along the winding normal. A geometric failure copies the input pose. */
bool cloth_offset_surface(const ClothMesh *mesh, const float positions[][3], const float offsets_m[], float output[][3]);
ClothMetrics cloth_measure(const ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                           const float anchors[][3], const ClothCapsule *body, int count);

#endif
