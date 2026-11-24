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
    const float *drive;        /* Optional squared angular frequency (s^-2) of each vertex's spring to its target. */
} ClothMesh;
typedef struct {
    float a[3], b[3];
    float radius;               /* Radius at endpoint a (m). */
    float radius_b;             /* Radius at endpoint b (m); 0 uses uniform radius. */
    float prev_a[3], prev_b[3]; /* Substep-start endpoints when has_prev != 0. */
    uint8_t flags;              /* CLOTH_COLLIDER_INSIDE or CLOTH_COLLIDER_OUTSIDE. */
    uint8_t has_prev;
} ClothCapsule;
typedef struct {
    float density, stretch, bend, drag, thickness;
    int iterations;
    float max_edge_strain;    /* Fractional extension limit for warp edges during strain limiting. */
    int strain_passes;        /* Zero disables the optional one-sided projection. */
    float aero_drag;          /* Normal-to-surface aerodynamic drag (1/s). */
    float weft_stretch_scale; /* 0 defaults to 1.0; >1 softens weft edges relative to stretch. */
    float diag_stretch_scale; /* 0 defaults to 1.0; >1 softens diagonal shear edges. */
    float weft_bend_scale;    /* 0 defaults to 1.0; >1 softens weft/diag hinges. */
    float weft_max_strain;    /* 0 defaults to max_edge_strain; separate limit for weft/diag edges. */
    float contact_friction;   /* 0..1 tangential friction relative to moving collider surface. */
    float max_speed_mps;      /* 0 disables post-step velocity clamp. */
    float compression_limit;  /* 0..0.5 free edge shortening fraction before compression stiffness engages. */
    float reset_carry_range_m; /* Optional distance over which inner colliders support inherited seam displacement at reset. */
} ClothMaterial;
/* Finite, one-sided face. The cross product half_u x half_v points out of the surface. */
#define CLOTH_SUPPORT_PLANES 48
typedef struct {
    float center[3], half_u[3], half_v[3];
    float depth; /* Distance from the top face to the underside, in meters. Zero is a sheet. */
    int plane_count;
    /* Optional closed convex hull inside the face bounds. Inside satisfies n dot (x - center) + d <= 0. */
    float planes[CLOTH_SUPPORT_PLANES][4];
} ClothSupport;

/* Reference-frame motion over the current substep in simulation (actor-root) space. Free vertices stay
   above the simulation-space ground plane y = 0 by the material thickness. */
typedef struct {
    float linear_accel[3];      /* Root linear acceleration (m/s^2) in simulation space. */
    float angular_vel_y;        /* Root yaw rate around +Y (rad/s). */
    float angular_accel_y;      /* Root yaw acceleration around +Y (rad/s^2). */
    float linear_inertia;       /* 0..1 share of root linear inertia applied to cloth. */
    float angular_inertia;      /* 0..1 share of root yaw inertia applied to cloth. */
    const ClothSupport *support; /* Optional scene surface in simulation space for this substep. */
    float anchor_shift_y;        /* Free-vertex translation inherited from seam motion before integration (m). */
} ClothFrame;
/* Skinned normal of a target (contact fallback), the radius a free vertex may stray from its target and how far it
   may sink below the target along that normal. */
typedef struct {
    float normal[3], travel, backstop;
} ClothGuide;
typedef struct {
    const ClothMesh *mesh; /* Immutable cooked mesh accepted by the last reset. */
    bool frame_valid;
    float x[CLOTH_VERTICES][3];
    float velocity[CLOTH_VERTICES][3];
    float stretch_lambda[CLOTH_EDGES], bend_lambda[CLOTH_EDGES];
    float anim_lambda[CLOTH_VERTICES];
    /* Each free vertex's target after the last push out of the body and the support; the next push exits a
       collider on the side this point was on, so a target that crosses a limb's axis does not flip sides. */
    float target[CLOTH_VERTICES][3];
} ClothState;
typedef struct {
    float stretch, penetration, pin_error, speed;
    bool finite;
} ClothMetrics;

/* All points are in one Y-up meter space. Anchors and capsules must be sampled at substep end. A free vertex is driven
   toward and bounded around its anchor, which the caller has pushed out of the body (cloth_push_targets); pins
   follow their anchors. */
bool cloth_frame_valid(const ClothMesh *mesh, const ClothMaterial *m, float dt,
                       const float anchors[][3], const ClothGuide *guide, const ClothCapsule *body, int count);
void cloth_reset(ClothState *s, const ClothMesh *mesh, const float pose[][3]);
/* Reset cloth onto anchors and resolve initial body penetration outward from pinned seams. NULL support is none. */
bool cloth_reset_supported(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m,
                           const float anchors[][3], const ClothGuide *guide,
                           const ClothCapsule *body, int count, const ClothSupport *support);
/* Push the free vertices' skinned anchors out of the body capsules and then the support, in place: two
   Gauss-Seidel passes with the contact margin, each exit on the side the last pushed target was on. Call it once
   per pose sample; the steps between two samples interpolate the pushed targets. Pins keep their anchors. An invalid
   support is refused as cloth_step refuses it: false, the frame is invalid and the anchors stay unchanged. */
bool cloth_push_targets(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float anchors[][3],
                        const ClothGuide *guide, const ClothCapsule *body, int count, const ClothSupport *support);
/* NULL frame: an inertial frame without fictitious forces, ground plane or scene support. */
void cloth_step(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                const ClothFrame *frame, const float anchors[][3], const ClothGuide *guide,
                const ClothCapsule *body, int count);
ClothMetrics cloth_measure(const ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                           const float anchors[][3], const ClothCapsule *body, int count);

#endif
