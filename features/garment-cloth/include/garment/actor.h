#ifndef GARMENT_ACTOR_H
#define GARMENT_ACTOR_H

#include "garment/cloth.h"
#include "garment/surface_transfer.h"
#include "garment/debug.h"
#include "skeletal/nt_skeletal.h"

typedef struct {
    const char *id;
    bool ready, finite;
    int pieces, vertices, render_vertices, missing_bindings, surface_faces;
    unsigned steps;
    float max_displacement_m, max_strain;
    /* With GarmentInput.measure, after the last substep: strain, the cloth beyond its max distance and inside its
       backstop, and the reference vertices a shape holds deeper than their max distance (cloth_measure). */
    float max_distance_excess_m, backstop_depth_m, held_depth_m;
    int held_vertices;
} GarmentStats;

/* GARMENT_JOINTS_MAX bounds the used joints plus their ancestors, which the actor interpolates. */
enum { GARMENT_PIECES_MAX = 8, GARMENT_BONES_MAX = 160, GARMENT_JOINTS_MAX = 128 };

typedef struct {
    const char *name;
    float inverse_bind[12];
} GarmentBone;
typedef struct {
    uint16_t bone[4];
    float weight[4];
} GarmentSkin;
typedef struct {
    uint16_t bone;
    float center[3]; /* In the bone's space. */
    float radius;
} GarmentSphere;
typedef struct {
    uint16_t a, b;
} GarmentCone;
typedef struct GarmentAsset {
    const char *id;
    const ClothMesh *mesh;
    const GarmentSkin *skin;      /* Outfit bones. */
    const float (*direction)[3];  /* Body direction at rest, skinned like a normal. */
    ClothMaterial material;
    int render_vertices;
    float linear_inertia, angular_inertia;
} GarmentAsset;
typedef struct {
    const char *id;
    const GarmentBone *bones; /* Every joint a piece or a shape uses. */
    int bone_count;
    const GarmentSphere *spheres;
    int sphere_count;
    const GarmentCone *cones;
    int cone_count;
    const GarmentAsset *pieces;
    int piece_count;
} GarmentSet;

typedef struct {
    const nt_skeletal_mat34_t *pose; /* Model-space joint matrices of the frame. */
    const float *world;              /* Actor root, column-major 4x4; its axes are normalized. */
    float dt;
    /* Start from the reference with no history: the first frame, a staged or seated placement, a cut or a teleport. */
    bool reset;
    bool render;
    const ClothConvex *convexes; /* World space: floor, seats, props. */
    int convex_count;
    /* r' = max(0, r * scale + bias) where the cooked r > 0, else 0: 1 and 0 simulate as cooked; scale 0 and bias 0 is
       kinematic and skips the solve. */
    float max_distance_scale, max_distance_bias;
    /* Fill the stats' strain, max-distance, backstop and held measures (lab, probes, tests); costs a pass per frame. */
    bool measure;
} GarmentInput;

typedef struct {
    const GarmentSet *set;
    struct GarmentPiece *pieces;
    ClothVertexBasis *vertex_basis;
    nt_skeletal_mat34_t *palette; /* Submitted GPU palette records (one per proxy vertex). */
    int palette_count;
    uint64_t rig;
    int joint[GARMENT_BONES_MAX];       /* Skeleton joint of each outfit bone. */
    uint16_t chain[GARMENT_JOINTS_MAX]; /* Used joints and their ancestors, parents first. */
    int16_t chain_parent[GARMENT_JOINTS_MAX]; /* Chain position of each joint's parent; -1 at a root. */
    int chain_count;
    nt_skeletal_trs_t previous[GARMENT_JOINTS_MAX]; /* Parent-local transform of each chain joint at the last sample. */
    float root_q[4], root_t[3];         /* Root at the last sample. */
    float sim_root_q[4], sim_root_t[3]; /* Root at the last substep. */
    ClothSphere spheres[CLOTH_SHAPES];  /* Actor space at the last substep. */
    float remainder, origin[3], prev_vel[3];
    float yaw, prev_yaw_vel;
    bool velocity_known; /* prev_vel and prev_yaw_vel come from a timed frame since the last reset */
    bool primed;
    GarmentStats stats;
} GarmentActor;

/* Allow two 30 Hz frame intervals of catch-up before dropping time on a long stall. */
enum { GARMENT_SIM_HZ = 180, GARMENT_MAX_SUBSTEPS = GARMENT_SIM_HZ / 15 };
void garment_actor_clear(GarmentActor *actor);
bool garment_actor_prepare(GarmentActor *actor, const GarmentSet *set, const nt_skeletal_skeleton_t *skel);
/* An actor without a previous update resets whatever the input says. */
bool garment_actor_update(GarmentActor *actor, const GarmentSet *set, const nt_skeletal_skeleton_t *skel,
                          const GarmentInput *in);

/* A negative weight_joint shows the max-distance map; other values are skeleton joint indices. */
bool garment_actor_debug_lines(const GarmentActor *actor, const nt_skeletal_skeleton_t *skel,
                              const nt_skeletal_mat34_t *pose, unsigned layers, int weight_joint,
                              ClothDebugLine emit, void *user);

#endif
