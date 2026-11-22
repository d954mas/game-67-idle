#ifndef GARMENT_CLOTH_H
#define GARMENT_CLOTH_H

#include <stdbool.h>
#include <stdint.h>

enum { CLOTH_VERTICES = 512, CLOTH_EDGES = 1536, CLOTH_CAPSULES = 64, CLOTH_MAX_STRAIN_PASSES = 32 };

enum {
    CLOTH_EDGE_WARP = 0,
    CLOTH_EDGE_WEFT = 1,
    CLOTH_EDGE_DIAG = 2
};

enum {
    CLOTH_COLLIDER_INSIDE = 0,
    CLOTH_COLLIDER_OUTSIDE = 1
};

typedef struct { uint16_t a, b, c; } ClothFace;
/* c,d are opposite vertices of an interior hinge; d < 0 marks a boundary. */
typedef struct {
    int a, b, c, d;
    float length, angle;
    uint8_t kind; /* CLOTH_EDGE_WARP, CLOTH_EDGE_WEFT, or CLOTH_EDGE_DIAG. */
} ClothEdge;
typedef struct { int pin; float max_length; } ClothTether;
typedef struct {
    int vertices, faces, edges;
    const float (*rest)[3];
    const float *inv_area; /* Zero at a sew point, otherwise reciprocal lumped vertex area (m^-2). */
    const ClothFace *face;
    const ClothEdge *edge;
    const ClothTether *tether; /* Optional per-vertex path-length limit; pin = -1 on unattached islands. */
    const float *anim_drive;   /* Optional per-vertex 0..1 AnimDrive weight mask. */
} ClothMesh;
typedef struct {
    float a[3], b[3];
    float radius;               /* Radius at endpoint a (m). */
    float radius_b;             /* Radius at endpoint b (m); 0 uses uniform radius. */
    float prev_a[3], prev_b[3]; /* Substep-start endpoints when has_prev != 0. */
    uint32_t mask;              /* Optional layer bitmask; 0 collides with every garment. */
    uint8_t flags;              /* CLOTH_COLLIDER_INSIDE or CLOTH_COLLIDER_OUTSIDE. */
    uint8_t has_prev;
} ClothCapsule;
typedef struct {
    float density, stretch, bend, drag, thickness;
    int iterations;
    float max_edge_strain;    /* Fractional extension limit for warp edges during strain limiting. */
    int strain_passes;        /* Zero disables the optional one-sided projection. */
    float gravity_scale;      /* 0 defaults to 1.0 (Earth gravity -9.81 m/s^2). */
    float aero_drag;          /* Normal-to-surface aerodynamic drag (1/s). */
    float anim_drive;         /* XPBD animation-drive stiffness (s^-2); 0 disables. */
    float weft_stretch_scale; /* 0 defaults to 1.0; >1 softens weft edges relative to stretch. */
    float diag_stretch_scale; /* 0 defaults to 1.0; >1 softens diagonal shear edges. */
    float weft_bend_scale;    /* 0 defaults to 1.0; >1 softens weft/diag hinges. */
    float weft_max_strain;    /* 0 defaults to max_edge_strain; separate limit for weft/diag edges. */
    float contact_friction;   /* 0..1 tangential friction relative to moving collider surface. */
    float max_speed_mps;      /* 0 disables post-step velocity clamp. */
    float compression_limit;  /* 0..0.5 free edge shortening fraction before compression stiffness engages. */
    uint32_t collider_mask;   /* 0 collides with all capsules; otherwise requires mask match. */
    float reset_carry_range_m; /* Optional distance over which inner colliders support inherited seam displacement at reset. */
} ClothMaterial;
/* Reference-frame motion over the current substep in simulation (actor-root) space. */
typedef struct {
    float linear_accel[3];      /* Root linear acceleration (m/s^2) in simulation space. */
    float angular_vel_y;        /* Root yaw rate around +Y (rad/s). */
    float angular_accel_y;      /* Root yaw acceleration around +Y (rad/s^2). */
    float linear_inertia;       /* 0..1 share of root linear inertia applied to cloth. */
    float angular_inertia;      /* 0..1 share of root yaw inertia applied to cloth. */
    float max_fictitious_accel; /* 0 defaults to 30.0 m/s^2 clamp against spikes. */
    float floor_y;              /* Optional horizontal floor plane height (m) when floor_enabled. */
    bool floor_enabled;         /* When true, prevents free cloth vertices from dropping below floor_y + thickness. */
    bool suppress_anim_drive;   /* When true, skips anim_drive for this substep (e.g. seated drape). */
    bool strict_guide;          /* Give the final pose envelope priority over contacts and tethers. */
} ClothFrame;
typedef struct {
    float normal[3], travel, backstop;
    float backstop_radius; /* 0 uses planar backstop; >0 uses spherical backstop behind anchor. */
} ClothGuide;
typedef struct {
    const ClothMesh *mesh; /* Immutable cooked mesh accepted by the last reset. */
    bool frame_valid;
    float x[CLOTH_VERTICES][3];
    float velocity[CLOTH_VERTICES][3];
    float stretch_lambda[CLOTH_EDGES], bend_lambda[CLOTH_EDGES];
    float anim_lambda[CLOTH_VERTICES];
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
/* Reset cloth onto anchors and resolve initial body penetration outward from pinned seams. */
bool cloth_reset_over_body(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m,
                           const float anchors[][3], const ClothGuide *guide,
                           const ClothCapsule *body, int count);
void cloth_step(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                const float anchors[][3], const ClothGuide *guide, const ClothCapsule *body, int count);
void cloth_step_framed(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                       const ClothFrame *frame, const float anchors[][3], const ClothGuide *guide,
                       const ClothCapsule *body, int count);
/* The final pose envelope has priority over body contact and tethers. NULL guide follows cloth_step. */
void cloth_step_shaped(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                       const float anchors[][3], const ClothGuide *guide, const ClothCapsule *body, int count);
/* Separate output storage, offsets in meters along the winding normal. A geometric failure copies the input pose. */
bool cloth_offset_surface(const ClothMesh *mesh, const float positions[][3], const float offsets_m[], float output[][3]);
ClothMetrics cloth_measure(const ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                           const float anchors[][3], const ClothCapsule *body, int count);

#endif
