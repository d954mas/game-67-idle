#ifndef GARMENT_CLOTH_H
#define GARMENT_CLOTH_H

#include <stdbool.h>
#include <stdint.h>

enum {
    CLOTH_VERTICES = 512,
    CLOTH_EDGES = 1536,
    CLOTH_SHAPES = 64, /* spheres, and separately cones */
    CLOTH_CONVEXES = 8,
    CLOTH_CONVEX_PLANES = 48
};

enum {
    CLOTH_EDGE_WARP = 0,
    CLOTH_EDGE_WEFT = 1,
    CLOTH_EDGE_DIAG = 2
};

typedef struct { uint16_t a, b, c; } ClothFace;
/* c,d are opposite vertices of an interior hinge; d < 0 marks a boundary. */
typedef struct {
    int a, b, c, d;
    float length, angle;
    uint8_t kind; /* CLOTH_EDGE_WARP, CLOTH_EDGE_WEFT, or CLOTH_EDGE_DIAG. */
} ClothEdge;
/* pin is a vertex with max distance 0, or -1 on an island without one. */
typedef struct { int pin; float max_length; } ClothTether;
/* A sphere of radius R whose centre stands d + R behind the reference along the body direction (m). R = 0 is off;
   R = +INFINITY is the half-space behind the plane at d, the form a flat or concave body takes. */
typedef struct { float d, R; } ClothBackstop;
typedef struct {
    int vertices, faces, edges;
    const float (*rest)[3];
    const float *inv_area; /* Reciprocal lumped vertex area (m^-2) of every vertex, kinematic ones included. */
    const ClothFace *face;
    const ClothEdge *edge;
    const ClothTether *tether;      /* Optional. */
    const float *max_distance;      /* r (m): how far a vertex may leave its reference; 0 is kinematic. */
    const ClothBackstop *backstop;
} ClothMesh;
typedef struct {
    float density, stretch, bend, drag, thickness;
    int iterations;
    float max_edge_strain;    /* Fractional extension limit for warp edges during strain limiting. */
    int strain_passes;        /* Zero disables the one-sided projection. */
    float aero_drag;          /* Normal-to-surface aerodynamic drag (1/s). */
    float weft_stretch_scale; /* 0 defaults to 1.0; >1 softens weft edges relative to stretch. */
    float diag_stretch_scale; /* 0 defaults to 1.0; >1 softens diagonal shear edges. */
    float weft_bend_scale;    /* 0 defaults to 1.0; >1 softens weft/diag hinges. */
    float weft_max_strain;    /* 0 defaults to max_edge_strain; separate limit for weft/diag edges. */
    float friction;           /* Coulomb coefficient against shapes and convexes, static equal to kinetic. */
    float compression_limit;  /* 0..0.5 fraction an edge shortens freely before it is held (half for WEFT). */
} ClothMaterial;

/* Where each vertex's skinned pose is this substep, and the unit direction from the cloth toward the body. */
typedef struct { float x[3], b[3]; } ClothReference;
/* A collision sphere at its current and previous substep centres (simulation space, m). */
typedef struct { float c[3], prev[3], r; } ClothSphere;
/* The round cone that joins two spheres. A sphere in no cone collides on its own. */
typedef struct { uint8_t a, b; } ClothCone;
/* Inside satisfies n . x + d <= 0 for every plane; n need not be unit. One plane is a half space. */
typedef struct {
    int plane_count;
    float planes[CLOTH_CONVEX_PLANES][4];
} ClothConvex;
typedef struct {
    const ClothSphere *spheres;
    int sphere_count;
    const ClothCone *cones;
    int cone_count;
    const ClothConvex *convexes;
    int convex_count;
    /* Optional rigid map taking a convex-fixed point from its previous-substep position to its current one, rows
       [R | t]; NULL is a convex at rest in simulation space. */
    const float (*convex_move)[4];
} ClothShapes;

/* Reference-frame motion over the current substep in simulation (actor-root) space. */
typedef struct {
    float linear_accel[3]; /* Root linear acceleration (m/s^2) in simulation space. */
    float angular_vel_y;   /* Root yaw rate around +Y (rad/s). */
    float angular_accel_y; /* Root yaw acceleration around +Y (rad/s^2). */
    float linear_inertia;  /* 0..1 share of root linear inertia applied to cloth. */
    float angular_inertia; /* 0..1 share of root yaw inertia applied to cloth. */
} ClothFrame;
typedef struct {
    const ClothMesh *mesh; /* Immutable cooked mesh accepted by the last reset. */
    bool frame_valid;
    float x[CLOTH_VERTICES][3];
    float velocity[CLOTH_VERTICES][3];
    float stretch_lambda[CLOTH_EDGES], bend_lambda[CLOTH_EDGES];
} ClothState;
typedef struct {
    float stretch, speed;
    float penetration; /* Deepest free vertex inside a shape or a convex, margin included. */
    float max_distance_excess_m; /* Largest distance of a vertex beyond its max-distance sphere. */
    float backstop_depth_m;      /* Deepest vertex inside its backstop sphere. */
    /* Reference vertices that a shape holds deeper than their max distance, and the deepest excess (m). */
    int held_vertices;
    float held_depth_m;
    bool finite;
} ClothMetrics;

/* All points are in one Y-up meter space and sampled at the substep end. max_distance is the runtime r of every
   vertex (NULL uses the mesh's); a vertex with r = 0 is kinematic this substep and sits on its reference. */
bool cloth_frame_valid(const ClothMesh *mesh, const ClothMaterial *m, float dt, const ClothReference *ref,
                       const float *max_distance, const ClothShapes *shapes);
/* x = reference and v = 0. A free vertex inside a shape leaves along -b, the side of the body its direction names,
   since there is no previous position to judge the side by; inside a convex it leaves through the nearest face. */
bool cloth_reset(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, const ClothReference *ref,
                 const float *max_distance, const ClothShapes *shapes);
/* NULL frame: an inertial frame without fictitious forces; NULL shapes: nothing to collide with. */
void cloth_step(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt, const ClothFrame *frame,
                const ClothReference *ref, const float *max_distance, const ClothShapes *shapes);
ClothMetrics cloth_measure(const ClothState *s, const ClothMesh *mesh, const ClothMaterial *m,
                           const ClothReference *ref, const float *max_distance, const ClothShapes *shapes);

#endif
