#include "garment/actor.h"
#include "garment/contact.h"
#include "core/nt_assert.h"
#include "hash/nt_hash.h"
#include "math/nt_math.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct GarmentPiece {
    ClothState cloth;
    ClothReference frame[CLOTH_VERTICES]; /* Reference at the frame pose. */
    float step[CLOTH_VERTICES][3];        /* Reference at the last substep. */
    float r[CLOTH_VERTICES];              /* Runtime max distance. */
} GarmentPiece;

/* Concurrent actor updates use separate scratch; simulation never calls back into user code. */
static _Thread_local struct {
    nt_skeletal_trs_t current[GARMENT_JOINTS_MAX];
    nt_skeletal_mat34_t model[GARMENT_JOINTS_MAX]; /* Chain joints at the evaluated time. */
    nt_skeletal_mat34_t skin[GARMENT_BONES_MAX];
    ClothReference ref[CLOTH_VERTICES];
    ClothSphere spheres[CLOTH_SHAPES];
    ClothCone cones[CLOTH_SHAPES];
    ClothConvex convexes[CLOTH_CONVEXES];
    float move[3][4];
    float dressed[CLOTH_VERTICES][3];
    ClothVertexBasis basis[CLOTH_VERTICES];
    float mats[CLOTH_VERTICES][12];
    /* Held across emit callbacks, so an update called from emit must not share them. */
    float weights[CLOTH_VERTICES], display[CLOTH_VERTICES][3];
} scratch;

void garment_actor_clear(GarmentActor *actor) {
    if (!actor) return;
    free(actor->pieces);
    free(actor->palette);
    free(actor->vertex_basis);
    memset(actor, 0, sizeof *actor);
}

/* Position of each outfit bone's joint in the chain, or -1 for a joint the skeleton lacks. */
static int chain_slot(const GarmentActor *a, int joint) {
    for (int i = 0; i < a->chain_count; ++i)
        if (a->chain[i] == joint) return i;
    return -1;
}

/* The set comes from garment_asset_view, which asserted its structure; only the skeleton is checked here. */
static bool bind_actor(GarmentActor *a, const GarmentSet *set, const nt_skeletal_skeleton_t *skel) {
    garment_actor_clear(a);
    a->set = set;
    a->rig = skel->rig_compat_id.value;
    a->stats.id = set->id;
    a->stats.finite = true;
    NT_ASSERT(set->pieces && set->piece_count > 0 && set->piece_count <= GARMENT_PIECES_MAX);
    if (!skel->joint_id || !skel->parent) return false;
    a->pieces = calloc((size_t)set->piece_count, sizeof *a->pieces);
    if (!a->pieces) {
        a->set = NULL;
        return false;
    }
    uint8_t *used = calloc(skel->joint_count, 1);
    if (!used) return false;
    for (int i = 0; i < set->bone_count; ++i) {
        uint32_t id = nt_hash32_str(set->bones[i].name).value;
        int j = 0;
        while (j < skel->joint_count && skel->joint_id[j] != id) ++j;
        if (j == skel->joint_count) {
            ++a->stats.missing_bindings;
            a->joint[i] = -1;
            continue;
        }
        a->joint[i] = j;
        for (int k = j; k != NT_SKELETAL_NO_PARENT; k = skel->parent[k]) used[k] = true;
    }
    /* The skeleton is in preorder, so ascending indices put every parent before its children. */
    bool fits = true;
    for (int j = 0; j < skel->joint_count && fits; ++j) {
        if (!used[j]) continue;
        fits = a->chain_count < GARMENT_JOINTS_MAX;
        if (fits) a->chain[a->chain_count++] = (uint16_t)j;
    }
    free(used);
    if (!fits) return false;
    for (int i = 0; i < a->chain_count; ++i) {
        uint16_t parent = skel->parent[a->chain[i]];
        a->chain_parent[i] = (int16_t)(parent == NT_SKELETAL_NO_PARENT ? -1 : chain_slot(a, parent));
    }
    for (int i = 0; i < set->bone_count; ++i)
        if (a->joint[i] >= 0) a->joint[i] = chain_slot(a, a->joint[i]);
    for (int p = 0; p < set->piece_count; ++p) {
        a->stats.vertices += set->pieces[p].mesh->vertices;
        a->stats.render_vertices += set->pieces[p].render_vertices;
    }
    a->stats.pieces = set->piece_count;
    a->stats.ready = a->stats.missing_bindings == 0;
    return a->stats.ready;
}

bool garment_actor_prepare(GarmentActor *a, const GarmentSet *set, const nt_skeletal_skeleton_t *skel) {
    if (!a || !set || !skel) return false;
    if (a->set != set || a->rig != skel->rig_compat_id.value) return bind_actor(a, set, skel);
    return a->stats.ready;
}

static void point(const nt_skeletal_mat34_t *m, const float p[3], float out[3]) {
    for (int k = 0; k < 3; ++k) out[k] = m->r[k][0] * p[0] + m->r[k][1] * p[1] + m->r[k][2] * p[2] + m->r[k][3];
}

static bool normalize(float p[3]) {
    float length = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    if (!isfinite(length) || length < 1e-8F) return false;
    for (int k = 0; k < 3; ++k) p[k] /= length;
    return true;
}

static float wrap_angle(float a) {
    const float pi = 3.14159265F;
    while (a > pi) a -= 2.0F * pi;
    while (a < -pi) a += 2.0F * pi;
    return a;
}

static bool invert(const nt_skeletal_mat34_t *m, nt_skeletal_mat34_t *out) {
    mat3 a, inv;
    for (int c = 0; c < 3; ++c)
        for (int r = 0; r < 3; ++r) a[c][r] = m->r[r][c];
    float det = glm_mat3_det(a);
    if (!isfinite(det) || fabsf(det) < 1e-12F) return false;
    glm_mat3_inv(a, inv);
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) out->r[r][c] = inv[c][r];
        out->r[r][3] = -(inv[0][r] * m->r[0][3] + inv[1][r] * m->r[1][3] + inv[2][r] * m->r[2][3]);
    }
    return true;
}

/* Rotation and per-axis scale of an affine transform without shear, which is what a TRS hierarchy of uniform
   scales produces. */
static bool decompose(const nt_skeletal_mat34_t *m, nt_skeletal_trs_t *out) {
    mat3 rotation;
    for (int c = 0; c < 3; ++c) {
        float s = sqrtf(m->r[0][c] * m->r[0][c] + m->r[1][c] * m->r[1][c] + m->r[2][c] * m->r[2][c]);
        if (!isfinite(s) || s < 1e-8F) return false;
        out->s[c] = s;
        for (int r = 0; r < 3; ++r) rotation[c][r] = m->r[r][c] / s;
    }
    if (glm_mat3_det(rotation) < 0.0F) {
        out->s[0] = -out->s[0];
        glm_vec3_negate(rotation[0]);
    }
    glm_mat3_quat(rotation, out->q);
    glm_quat_normalize(out->q);
    for (int k = 0; k < 3; ++k) out->t[k] = m->r[k][3];
    return true;
}

/* Parent-local transforms of the chain from model-space joints. */
static bool chain_locals(const GarmentActor *a, const nt_skeletal_skeleton_t *skel, const nt_skeletal_mat34_t *pose,
                         nt_skeletal_trs_t *out) {
    for (int i = 0; i < a->chain_count; ++i) {
        int j = a->chain[i];
        nt_skeletal_mat34_t local = pose[j];
        uint16_t parent = skel->parent[j];
        if (parent != NT_SKELETAL_NO_PARENT) {
            nt_skeletal_mat34_t inverse;
            if (!invert(&pose[parent], &inverse)) return false;
            nt_skeletal_mat34_mul(&inverse, &pose[j], &local);
        }
        if (!decompose(&local, &out[i])) return false;
    }
    return true;
}

/* q and -q are one rotation; the pair is put in one hemisphere first, since glm_quat_slerp's near-equal fallback
   lerps the inputs as given. */
static void slerp(const float a[4], const float b[4], float u, float out[4]) {
    versor from, to;
    glm_quat_copy((float *)a, from);
    glm_quat_copy((float *)b, to);
    if (glm_quat_dot(from, to) < 0.0F) glm_vec4_negate(to);
    glm_quat_slerp(from, to, u, out);
    glm_quat_normalize(out);
}

/* Slerp of the local rotations and lerp of the local translations and scales, then forward kinematics. u = 1 takes the
   sampled pose itself, so a substep on a sample is exact. */
static void interpolate_joints(const GarmentActor *a, const nt_skeletal_mat34_t *pose, float u) {
    if (u >= 1.0F) {
        for (int i = 0; i < a->chain_count; ++i) scratch.model[i] = pose[a->chain[i]];
        return;
    }
    for (int i = 0; i < a->chain_count; ++i) {
        const nt_skeletal_trs_t *p = &a->previous[i], *c = &scratch.current[i];
        nt_skeletal_trs_t trs;
        for (int k = 0; k < 3; ++k) {
            trs.t[k] = p->t[k] + u * (c->t[k] - p->t[k]);
            trs.s[k] = p->s[k] + u * (c->s[k] - p->s[k]);
        }
        slerp(p->q, c->q, u, trs.q);
        nt_skeletal_mat34_t local;
        nt_skeletal_mat34_from_trs(&trs, &local);
        int slot = a->chain_parent[i];
        if (slot < 0) scratch.model[i] = local;
        else nt_skeletal_mat34_mul(&scratch.model[slot], &local, &scratch.model[i]);
    }
}

static void skin_matrices(const GarmentActor *a) {
    const GarmentSet *set = a->set;
    for (int i = 0; i < set->bone_count; ++i) {
        nt_skeletal_mat34_t inverse;
        memcpy(&inverse, set->bones[i].inverse_bind, sizeof inverse);
        nt_skeletal_mat34_mul(&scratch.model[a->joint[i]], &inverse, &scratch.skin[i]);
    }
}

/* ref = sum_j w_j S_j rest and b = normalize(sum_j w_j R_j b_rest), at the evaluated joints. */
static bool evaluate_reference(const GarmentAsset *g, ClothReference *out) {
    for (int i = 0; i < g->mesh->vertices; ++i) {
        const GarmentSkin *v = &g->skin[i];
        float *x = out[i].x, *b = out[i].b;
        memset(x, 0, 3 * sizeof(float));
        memset(b, 0, 3 * sizeof(float));
        for (int j = 0; j < 4; ++j) {
            if (v->weight[j] == 0) continue;
            const nt_skeletal_mat34_t *m = &scratch.skin[v->bone[j]];
            const float *d = g->direction[i];
            float influenced[3];
            point(m, g->mesh->rest[i], influenced);
            for (int k = 0; k < 3; ++k) {
                x[k] += v->weight[j] * influenced[k];
                b[k] += v->weight[j] * (m->r[k][0] * d[0] + m->r[k][1] * d[1] + m->r[k][2] * d[2]);
            }
        }
        if (!normalize(b)) return false;
    }
    return true;
}

static void pose_spheres(const GarmentActor *a, ClothSphere *out) {
    const GarmentSet *set = a->set;
    for (int i = 0; i < set->sphere_count; ++i) {
        const GarmentSphere *s = &set->spheres[i];
        point(&scratch.model[a->joint[s->bone]], s->center, out[i].c);
        out[i].r = s->radius;
    }
}

/* Root rotation and translation of a column-major world matrix whose axes are normalized. */
static bool root_of(const float world[16], float q[4], float t[3]) {
    mat3 rotation;
    for (int c = 0; c < 3; ++c) {
        float axis[3] = {world[4 * c], world[4 * c + 1], world[4 * c + 2]};
        if (!normalize(axis) || !isfinite(world[12 + c])) return false;
        glm_vec3_copy(axis, rotation[c]);
        t[c] = world[12 + c];
    }
    glm_mat3_quat(rotation, q);
    glm_quat_normalize(q);
    return true;
}

/* World-space convexes in actor space at root (q, t), and the map from the last substep's actor space to this one for
   a world-fixed point. */
static bool local_convexes(GarmentActor *a, const GarmentInput *in, const float q[4], const float t[3]) {
    mat3 r, prev_r;
    glm_quat_mat3((float *)q, r);
    glm_quat_mat3(a->sim_root_q, prev_r);
    for (int c = 0; c < in->convex_count; ++c) {
        const ClothConvex *source = &in->convexes[c];
        ClothConvex *local = &scratch.convexes[c];
        if (source->plane_count < 1 || source->plane_count > CLOTH_CONVEX_PLANES) return false;
        local->plane_count = source->plane_count;
        for (int f = 0; f < source->plane_count; ++f) {
            const float *n = source->planes[f];
            for (int k = 0; k < 3; ++k) local->planes[f][k] = glm_vec3_dot(r[k], (float *)n);
            local->planes[f][3] = glm_vec3_dot((float *)n, (float *)t) + n[3];
        }
    }
    vec3 offset;
    glm_vec3_sub(a->sim_root_t, (float *)t, offset);
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) scratch.move[row][col] = glm_vec3_dot(r[row], prev_r[col]);
        scratch.move[row][3] = glm_vec3_dot(r[row], offset);
    }
    return true;
}

static void runtime_max_distance(const GarmentAsset *g, const GarmentInput *in, float *out) {
    for (int i = 0; i < g->mesh->vertices; ++i)
        out[i] = g->mesh->max_distance[i] > 0.0F
                     ? fmaxf(0.0F, g->mesh->max_distance[i] * in->max_distance_scale + in->max_distance_bias)
                     : 0.0F;
}

static void record_measure(GarmentStats *stats, const ClothMetrics *mt) {
    stats->max_distance_excess_m = fmaxf(stats->max_distance_excess_m, mt->max_distance_excess_m);
    stats->backstop_depth_m = fmaxf(stats->backstop_depth_m, mt->backstop_depth_m);
    stats->held_depth_m = fmaxf(stats->held_depth_m, mt->held_depth_m);
    stats->held_vertices += mt->held_vertices;
    stats->max_strain = fmaxf(stats->max_strain, mt->stretch);
    if (!mt->finite) stats->finite = false;
}

static bool inputs_valid(const GarmentInput *in) {
    return in && in->pose && in->world && isfinite(in->dt) && in->dt >= 0 && in->convex_count >= 0 &&
           in->convex_count <= CLOTH_CONVEXES && (!in->convex_count || in->convexes) &&
           isfinite(in->max_distance_scale) && in->max_distance_scale >= 0 && isfinite(in->max_distance_bias);
}

static bool simulate(GarmentActor *a, const GarmentSet *set, const nt_skeletal_skeleton_t *skel, const GarmentInput *in) {
    if (!set || !skel || !inputs_valid(in) || !garment_actor_prepare(a, set, skel)) return false;
    float root_q[4], root_t[3];
    if (!root_of(in->world, root_q, root_t) || !chain_locals(a, skel, in->pose, scratch.current)) return false;
    const float *world = in->world;
    float yaw = atan2f(world[8], world[10]);
    const bool reset = in->reset || !a->primed;
    if (reset) {
        memcpy(a->previous, scratch.current, (size_t)a->chain_count * sizeof a->previous[0]);
        memcpy(a->root_q, root_q, sizeof root_q);
        memcpy(a->root_t, root_t, sizeof root_t);
        memcpy(a->sim_root_q, root_q, sizeof root_q);
        memcpy(a->sim_root_t, root_t, sizeof root_t);
        a->remainder = 0.0F;
    }
    float dyaw = reset ? 0.0F : wrap_angle(yaw - a->yaw);
    const float step = 1.0F / (float)GARMENT_SIM_HZ;
    float clamped_dt = fminf(in->dt, (float)GARMENT_MAX_SUBSTEPS * step);
    float elapsed = reset ? 0.0F : a->remainder + clamped_dt;
    int steps = (int)floorf(elapsed / step + 1e-5F);
    if (steps > GARMENT_MAX_SUBSTEPS) steps = GARMENT_MAX_SUBSTEPS;
    const bool kinematic = in->max_distance_scale == 0.0F && in->max_distance_bias <= 0.0F;

    /* Gravity-aligned actor-root fictitious acceleration in actor space, from the frame's root motion. */
    /* An acceleration needs two velocities: none on the first timed frame after a reset, and a frame without time
       (a pause) keeps the history it had. */
    float local_accel[3] = {0, 0, 0};
    float yaw_vel = 0.0F, yaw_accel = 0.0F;
    if (reset) {
        a->velocity_known = false;
    } else if (in->dt > 1e-5F) {
        float inv_dt = 1.0F / in->dt;
        float world_vel[3];
        for (int k = 0; k < 3; ++k) world_vel[k] = (root_t[k] - a->origin[k]) * inv_dt;
        yaw_vel = dyaw * inv_dt;
        if (a->velocity_known) {
            float world_accel[3];
            for (int k = 0; k < 3; ++k) world_accel[k] = (world_vel[k] - a->prev_vel[k]) * inv_dt;
            mat3 r;
            glm_quat_mat3(root_q, r);
            for (int k = 0; k < 3; ++k) local_accel[k] = glm_vec3_dot(r[k], world_accel);
            yaw_accel = (yaw_vel - a->prev_yaw_vel) * inv_dt;
        }
        memcpy(a->prev_vel, world_vel, sizeof a->prev_vel);
        a->prev_yaw_vel = yaw_vel;
        a->velocity_known = true;
    }

    GarmentStats *stats = &a->stats;
    stats->max_strain = stats->max_displacement_m = 0;
    stats->max_distance_excess_m = stats->backstop_depth_m = stats->held_depth_m = 0;
    stats->held_vertices = 0;
    stats->finite = true;
    stats->surface_faces = 0;
    for (int i = 0; i < set->cone_count; ++i) scratch.cones[i] = (ClothCone){(uint8_t)set->cones[i].a, (uint8_t)set->cones[i].b};
    ClothShapes shapes = {.spheres = scratch.spheres, .sphere_count = set->sphere_count, .cones = scratch.cones,
                          .cone_count = set->cone_count, .convexes = scratch.convexes, .convex_count = in->convex_count};

    /* The frame pose: every piece's frame reference, and the whole state on a reset. */
    interpolate_joints(a, in->pose, 1.0F);
    skin_matrices(a);
    for (int p = 0; p < set->piece_count; ++p) {
        if (!evaluate_reference(&set->pieces[p], a->pieces[p].frame)) return false;
        runtime_max_distance(&set->pieces[p], in, a->pieces[p].r);
    }
    if (reset || kinematic) {
        pose_spheres(a, a->spheres);
        for (int i = 0; i < set->sphere_count; ++i) memcpy(a->spheres[i].prev, a->spheres[i].c, sizeof a->spheres[i].prev);
        memcpy(scratch.spheres, a->spheres, (size_t)set->sphere_count * sizeof scratch.spheres[0]);
        if (!local_convexes(a, in, root_q, root_t)) return false;
        for (int p = 0; p < set->piece_count; ++p) {
            const GarmentAsset *g = &set->pieces[p];
            GarmentPiece *s = &a->pieces[p];
            if (!cloth_reset(&s->cloth, g->mesh, &g->material, s->frame, s->r, &shapes)) return false;
            for (int i = 0; i < g->mesh->vertices; ++i) memcpy(s->step[i], s->frame[i].x, sizeof s->step[i]);
            if (in->measure) {
                ClothMetrics mt = cloth_measure(&s->cloth, g->mesh, &g->material, s->frame, s->r, &shapes);
                record_measure(stats, &mt);
            }
        }
        steps = reset ? 0 : steps;
        if (kinematic) {
            memcpy(a->sim_root_q, root_q, sizeof root_q);
            memcpy(a->sim_root_t, root_t, sizeof root_t);
        }
    }
    shapes.convex_move = (const float (*)[4])scratch.move;
    for (int k = 0; k < steps && !kinematic; ++k) {
        float u = clamped_dt > 1e-6F ? fminf(1.0F, ((float)(k + 1) * step - a->remainder) / clamped_dt) : 1.0F;
        float q[4], t[3];
        slerp(a->root_q, root_q, u, q);
        for (int i = 0; i < 3; ++i) t[i] = a->root_t[i] + u * (root_t[i] - a->root_t[i]);
        if (!local_convexes(a, in, q, t)) return false;
        interpolate_joints(a, in->pose, u);
        skin_matrices(a);
        pose_spheres(a, scratch.spheres);
        /* A frame can end between substeps; the next substep sweeps from the last solved centres. */
        for (int i = 0; i < set->sphere_count; ++i) memcpy(scratch.spheres[i].prev, a->spheres[i].c, sizeof scratch.spheres[i].prev);
        ClothFrame frame = {.linear_accel = {local_accel[0], local_accel[1], local_accel[2]}, .angular_vel_y = yaw_vel,
                            .angular_accel_y = yaw_accel};
        for (int p = 0; p < set->piece_count; ++p) {
            const GarmentAsset *g = &set->pieces[p];
            GarmentPiece *s = &a->pieces[p];
            if (!evaluate_reference(g, scratch.ref)) return false;
            frame.linear_inertia = g->linear_inertia;
            frame.angular_inertia = g->angular_inertia;
            cloth_step(&s->cloth, g->mesh, &g->material, step, &frame, scratch.ref, s->r, &shapes);
            if (!s->cloth.frame_valid) {
                stats->finite = false;
                return false;
            }
            for (int i = 0; i < g->mesh->vertices; ++i) memcpy(s->step[i], scratch.ref[i].x, sizeof s->step[i]);
            if (k + 1 == steps && in->measure) {
                ClothMetrics mt = cloth_measure(&s->cloth, g->mesh, &g->material, scratch.ref, s->r, &shapes);
                record_measure(stats, &mt);
            }
        }
        memcpy(a->spheres, scratch.spheres, (size_t)set->sphere_count * sizeof a->spheres[0]);
        memcpy(a->sim_root_q, q, sizeof q);
        memcpy(a->sim_root_t, t, sizeof t);
    }
    if (!stats->finite) return false;
    memcpy(a->previous, scratch.current, (size_t)a->chain_count * sizeof a->previous[0]);
    memcpy(a->root_q, root_q, sizeof root_q);
    memcpy(a->root_t, root_t, sizeof root_t);
    a->primed = true;
    stats->steps = (reset ? 0U : stats->steps) + (kinematic ? 0U : (unsigned)steps);
    a->remainder = reset ? 0.0F : fmaxf(0.0F, elapsed - (float)steps * step);
    a->yaw = yaw;
    memcpy(a->origin, root_t, sizeof a->origin);
    return true;
}

static bool prepare_vertex_frames(GarmentActor *a) {
    if (a->palette_count) return true;
    int total_verts = 0;
    for (int p = 0; p < a->set->piece_count; ++p) total_verts += a->set->pieces[p].mesh->vertices;
    if (total_verts <= 0) return false;
    ClothVertexBasis *basis = calloc((size_t)total_verts, sizeof *basis);
    nt_skeletal_mat34_t *frames = calloc((size_t)total_verts, sizeof *frames);
    if (!basis || !frames) {
        free(basis);
        free(frames);
        return false;
    }
    int offset = 0;
    for (int p = 0; p < a->set->piece_count; ++p) {
        const ClothMesh *m = a->set->pieces[p].mesh;
        if (!cloth_vertex_frames_prepare(m, scratch.basis)) {
            free(basis);
            free(frames);
            return false;
        }
        memcpy(basis + offset, scratch.basis, (size_t)m->vertices * sizeof *basis);
        offset += m->vertices;
    }
    a->vertex_basis = basis;
    a->palette = frames;
    a->palette_count = total_verts;
    return true;
}

bool garment_actor_update(GarmentActor *a, const GarmentSet *set, const nt_skeletal_skeleton_t *skel,
                          const GarmentInput *in) {
    if (!a) return false;
    if (!simulate(a, set, skel, in)) {
        a->primed = false;
        return false;
    }
    if (!in->render) return true;
    if (!prepare_vertex_frames(a)) return false;
    int offset = 0;
    for (int p = 0; p < set->piece_count; ++p) {
        const ClothMesh *m = set->pieces[p].mesh;
        const GarmentPiece *piece = &a->pieces[p];
        for (int v = 0; v < m->vertices; ++v) {
            float d2 = 0.0F;
            for (int k = 0; k < 3; ++k) {
                float d = piece->cloth.x[v][k] - piece->step[v][k];
                d2 += d * d;
                /* The frame can end between substeps: the offset from the last substep's reference rides on the
                   frame's reference, so kinematic vertices sit exactly on the frame pose. */
                scratch.dressed[v][k] = d + piece->frame[v].x[k];
            }
            a->stats.max_displacement_m = fmaxf(a->stats.max_displacement_m, sqrtf(d2));
        }
        if (!cloth_vertex_quats_eval(m, a->vertex_basis + offset, scratch.dressed, scratch.mats)) return false;
        for (int v = 0; v < m->vertices; ++v) memcpy(&a->palette[offset + v], scratch.mats[v], sizeof a->palette[0]);
        offset += m->vertices;
    }
    return true;
}

bool garment_actor_piece(const GarmentActor *a, int piece, const ClothState **cloth, const ClothReference **reference) {
    if (!a || !a->primed || !a->pieces || piece < 0 || piece >= a->set->piece_count) return false;
    if (cloth) *cloth = &a->pieces[piece].cloth;
    if (reference) *reference = a->pieces[piece].frame;
    return true;
}

bool garment_actor_debug_lines(const GarmentActor *actor, const nt_skeletal_skeleton_t *skel,
                              const nt_skeletal_mat34_t *pose, unsigned layers, int weight_joint,
                              ClothDebugLine emit, void *user) {
    if (!skel || !pose || !emit || weight_joint >= skel->joint_count) return false;
    if (layers & CLOTH_DEBUG_SKELETON) {
        if (!skel->parent) return false;
        for (int j = 0; j < skel->joint_count; ++j) {
            uint16_t parent = skel->parent[j];
            if (parent >= skel->joint_count) continue;
            float a[3], b[3];
            for (int k = 0; k < 3; ++k) {
                a[k] = pose[parent].r[k][3];
                b[k] = pose[j].r[k][3];
            }
            cloth_debug_bone(a, b, emit, user);
        }
    }
    const unsigned cloth_layers = CLOTH_DEBUG_WEIGHTS | CLOTH_DEBUG_TETHERS | CLOTH_DEBUG_SHAPES |
                                  CLOTH_DEBUG_REFERENCE | CLOTH_DEBUG_MAX_DISTANCE | CLOTH_DEBUG_BACKSTOP;
    if (!(layers & cloth_layers)) return true;
    if (!actor || !actor->primed || !actor->pieces || actor->rig != skel->rig_compat_id.value) return false;
    const GarmentSet *set = actor->set;
    if (layers & CLOTH_DEBUG_SHAPES) {
        bool in_cone[CLOTH_SHAPES] = {0};
        for (int i = 0; i < set->cone_count; ++i) {
            in_cone[set->cones[i].a] = in_cone[set->cones[i].b] = true;
            cloth_debug_cone(&actor->spheres[set->cones[i].a], &actor->spheres[set->cones[i].b], emit, user);
        }
        for (int i = 0; i < set->sphere_count; ++i)
            if (!in_cone[i]) cloth_debug_cone(&actor->spheres[i], &actor->spheres[i], emit, user);
    }
    int offset = 0;
    for (int p = 0; p < set->piece_count; ++p) {
        const GarmentAsset *asset = &set->pieces[p];
        const GarmentPiece *piece = &actor->pieces[p];
        const int n = asset->mesh->vertices;
        if (layers & (CLOTH_DEBUG_WEIGHTS | CLOTH_DEBUG_REFERENCE | CLOTH_DEBUG_MAX_DISTANCE | CLOTH_DEBUG_BACKSTOP)) {
            if (!actor->palette || offset + n > actor->palette_count) return false;
            float *weights = scratch.weights, (*display)[3] = scratch.display;
            float largest = 0.0F;
            for (int i = 0; i < n; ++i) largest = fmaxf(largest, piece->r[i]);
            for (int i = 0; i < n; ++i) {
                memcpy(display[i], actor->palette[offset + i].r[0], sizeof display[i]);
                /* Without a joint the layer shows the runtime max distance, relative to its largest. */
                weights[i] = weight_joint < 0 && largest > 0.0F ? piece->r[i] / largest : 0.0F;
                if (weight_joint < 0) continue;
                for (int j = 0; j < 4; ++j) {
                    int slot = actor->joint[asset->skin[i].bone[j]];
                    if (slot >= 0 && actor->chain[slot] == weight_joint) weights[i] += asset->skin[i].weight[j];
                }
            }
            if (layers & CLOTH_DEBUG_WEIGHTS)
                cloth_debug_mesh(asset->mesh, display, weights, CLOTH_DEBUG_WEIGHTS, emit, user);
            cloth_debug_reference(asset->mesh, display, piece->frame, piece->r, layers, emit, user);
        }
        offset += n;
        if (layers & CLOTH_DEBUG_TETHERS) cloth_debug_mesh(asset->mesh, piece->cloth.x, NULL, CLOTH_DEBUG_TETHERS, emit, user);
    }
    return true;
}
