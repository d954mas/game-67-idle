#ifndef GARMENT_ACTOR_H
#define GARMENT_ACTOR_H

#include "garment/cloth.h"
#include "garment/render_binding.h"
#include "garment/debug.h"
#include "skeletal/nt_skeletal.h"

typedef struct {
    const char *id;
    bool ready, finite;
    int pieces, vertices, driven_joints, render_vertices, missing_bindings, surface_faces;
    unsigned steps;
    float max_displacement_m, max_strain, pin_error_m;
} GarmentStats;

typedef struct {
    const char *name;
    float inverse_bind[12];
} GarmentBone;
typedef struct {
    uint16_t bone[4];
    float weight[4], normal[3], travel_m, backstop_m, backstop_radius_m;
} GarmentVertex;
typedef struct {
    uint16_t bone_a, bone_b;
    float point_a[3], point_b[3], radius_m, radius_b_m;
    uint8_t flags;
} GarmentCollider;
typedef struct {
    uint16_t bone[6]; /* Pairs for the trunk (base to top) and each thigh (hip to knee). */
    float start_deg, full_deg, proximal_m, distal_m;
} GarmentLap;
typedef struct GarmentAsset {
    const char *id;
    const GarmentBone *bones;
    int bone_count;
    const ClothMesh *mesh;
    const GarmentVertex *vertices;
    ClothMaterial material;
    const GarmentCollider *colliders;
    int collider_count;
    int render_vertices;
    float linear_inertia, angular_inertia;
    const GarmentLap *lap;
    float downward_follow;
} GarmentAsset;
typedef struct {
    const char *id;
    const GarmentAsset *pieces;
    int piece_count;
} GarmentSet;

typedef struct {
    const GarmentSet *set;
    struct GarmentPiece *pieces;
    ClothVertexBasis *vertex_basis;
    nt_skeletal_mat34_t *palette; /* Submitted GPU palette records (one per proxy vertex). */
    int palette_count;
    uint64_t rig;
    float remainder, origin[3], prev_vel[3];
    float yaw, prev_yaw_vel;
    bool primed;
    GarmentStats stats;
} GarmentActor;

/* Allow two 30 Hz frame intervals of catch-up before dropping time on a long stall. */
enum { GARMENT_SIM_HZ = 180, GARMENT_MAX_SUBSTEPS = GARMENT_SIM_HZ / 15 };
void garment_actor_clear(GarmentActor *actor);
bool garment_actor_prepare(GarmentActor *actor, const GarmentSet *set, const nt_skeletal_skeleton_t *skel);
bool garment_actor_update(GarmentActor *actor, const GarmentSet *set, const nt_skeletal_skeleton_t *skel,
                          const nt_skeletal_mat34_t *pose, const float world[16], float dt, bool render);
/* The caller owns the world-space face for this call, including the initial or teleport reset. */
bool garment_actor_update_supported(GarmentActor *actor, const GarmentSet *set, const nt_skeletal_skeleton_t *skel,
                                    const nt_skeletal_mat34_t *pose, const float world[16], float dt, bool render,
                                    const ClothSupport *world_support);

/* A negative weight_joint selects the AnimDrive mask; other values are skeleton joint indices. */
bool garment_actor_debug_lines(const GarmentActor *actor, const nt_skeletal_skeleton_t *skel,
                              const nt_skeletal_mat34_t *pose, unsigned layers, int weight_joint,
                              ClothDebugLine emit, void *user);

#endif
