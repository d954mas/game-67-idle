#include "garment/actor.h"
#include "garment/contact.h"
#include "hash/nt_hash.h"
#include "math/nt_math.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { PIECES_MAX = 8, BONES_MAX = 160, CAPSULES_MAX = 32 };

typedef struct {
    float anchors[CLOTH_VERTICES][3];
    ClothGuide guide[CLOTH_VERTICES];
    ClothCapsule caps[CAPSULES_MAX];
    int cap_count;
} GarmentFrame;

typedef struct GarmentPiece {
    int joint[BONES_MAX];
    int lap_joint[6];
    ClothState cloth;
    GarmentFrame previous;
    float sim_anchors[CLOTH_VERTICES][3];
    ClothCapsule sim_caps[CAPSULES_MAX];
    int sim_cap_count;
    int continuous_cap_count; /* A pose-only frame can remove a generated collider between physical steps. */
} GarmentPiece;

/* Concurrent actor updates use separate scratch; simulation never calls back into user code. */
static _Thread_local struct {
    GarmentFrame current, step;
    nt_skeletal_mat34_t skin[BONES_MAX];
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

static bool bind_actor(GarmentActor *a, const GarmentSet *set, const nt_skeletal_skeleton_t *skel) {
    garment_actor_clear(a);
    a->set = set;
    a->rig = skel->rig_compat_id.value;
    a->stats.id = set->id;
    a->stats.finite = true;
    if (!set->pieces || !skel->joint_id || set->piece_count <= 0 || set->piece_count > PIECES_MAX)
        return false;
    a->pieces = calloc((size_t)set->piece_count, sizeof *a->pieces);
    if (!a->pieces) {
        a->set = NULL;
        return false;
    }
    for (int p = 0; p < set->piece_count; ++p) {
        const GarmentAsset *asset = &set->pieces[p];
        GarmentPiece *piece = &a->pieces[p];
        if (!asset->bones || !cloth_mesh_valid(asset->mesh) || asset->bone_count < 1 || asset->bone_count > BONES_MAX ||
            asset->collider_count < 0 || asset->collider_count > CAPSULES_MAX || !asset->vertices ||
            (asset->collider_count && !asset->colliders))
            return false;
        for (int i = 0; i < asset->bone_count; ++i) {
            if (!asset->bones[i].name) return false;
            uint32_t id = nt_hash32_str(asset->bones[i].name).value;
            int j = 0;
            while (j < skel->joint_count && skel->joint_id[j] != id)
                ++j;
            if (j == skel->joint_count)
                ++a->stats.missing_bindings;
            piece->joint[i] = j;
        }
        if (asset->lap) {
            if (asset->collider_count + 2 > CAPSULES_MAX) return false;
            for (int k = 0; k < 6; ++k) {
                if (asset->lap->bone[k] >= asset->bone_count) return false;
                piece->lap_joint[k] = piece->joint[asset->lap->bone[k]];
            }
        }
        for (int i = 0; i < asset->mesh->vertices; ++i)
            for (int j = 0; j < 4; ++j)
                if (asset->vertices[i].bone[j] >= asset->bone_count)
                    return false;
        for (int i = 0; i < asset->collider_count; ++i)
            if (asset->colliders[i].bone_a >= asset->bone_count || asset->colliders[i].bone_b >= asset->bone_count)
                return false;
        a->stats.vertices += asset->mesh->vertices;
        a->stats.render_vertices += asset->render_vertices;
    }
    a->stats.pieces = set->piece_count;
    a->stats.ready = a->stats.missing_bindings == 0;
    return a->stats.ready;
}

static void point(const nt_skeletal_mat34_t *m, const float p[3], float out[3]) {
    for (int k = 0; k < 3; ++k)
        out[k] = m->r[k][0] * p[0] + m->r[k][1] * p[1] + m->r[k][2] * p[2] + m->r[k][3];
}

static bool normalize(float p[3]) {
    float length = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    if (!isfinite(length) || length < 1e-8F)
        return false;
    for (int k = 0; k < 3; ++k)
        p[k] /= length;
    return true;
}

static float wrap_angle(float a) {
    const float pi = 3.14159265F;
    while (a > pi) a -= 2.0F * pi;
    while (a < -pi) a += 2.0F * pi;
    return a;
}

bool garment_actor_prepare(GarmentActor *a, const GarmentSet *set, const nt_skeletal_skeleton_t *skel) {
    if (!a || !set || !skel)
        return false;
    if (a->set != set || a->rig != skel->rig_compat_id.value)
        return bind_actor(a, set, skel);
    return a->stats.ready;
}

/* Sample anchors, guides, and capsules directly in actor-root local space (metres, Y-up). */
static bool sample_frame_local(const GarmentAsset *g, const GarmentPiece *s, const nt_skeletal_mat34_t *pose) {
    GarmentFrame *f = &scratch.current;
    for (int i = 0; i < g->bone_count; ++i) {
        nt_skeletal_mat34_t inverse;
        memcpy(&inverse, g->bones[i].inverse_bind, sizeof inverse);
        nt_skeletal_mat34_t posed;
        nt_skeletal_mat34_mul(&pose[s->joint[i]], &inverse, &posed);
        scratch.skin[i] = posed;
    }
    for (int i = 0; i < g->mesh->vertices; ++i) {
        const GarmentVertex *v = &g->vertices[i];
        float *pos = f->anchors[i], *n = f->guide[i].normal;
        memset(pos, 0, 3 * sizeof(float));
        memset(n, 0, 3 * sizeof(float));
        for (int j = 0; j < 4; ++j) {
            if (v->weight[j] == 0)
                continue;
            const nt_skeletal_mat34_t *m = &scratch.skin[v->bone[j]];
            float influenced[3];
            point(m, g->mesh->rest[i], influenced);
            for (int k = 0; k < 3; ++k) {
                pos[k] += v->weight[j] * influenced[k];
                n[k] += v->weight[j] *
                        (m->r[k][0] * v->normal[0] + m->r[k][1] * v->normal[1] + m->r[k][2] * v->normal[2]);
            }
        }
        if (!normalize(n))
            return false;
        f->guide[i].travel = v->travel_m;
        f->guide[i].backstop = v->backstop_m;
        f->guide[i].backstop_radius = v->backstop_radius_m;
    }
    for (int i = 0; i < g->collider_count; ++i) {
        const GarmentCollider *c = &g->colliders[i];
        memset(&f->caps[i], 0, sizeof f->caps[i]);
        point(&scratch.skin[c->bone_a], c->point_a, f->caps[i].a);
        point(&scratch.skin[c->bone_b], c->point_b, f->caps[i].b);
        f->caps[i].radius = c->radius_m;
        f->caps[i].radius_b = c->radius_b_m;
        f->caps[i].flags = c->flags;
    }
    f->cap_count = g->collider_count;
    if (g->lap) {
        float hips[3], spine[3], lh[3], lk[3], rh[3], rk[3], up[3];
        for (int k = 0; k < 3; ++k) {
            hips[k] = pose[s->lap_joint[0]].r[k][3];
            spine[k] = pose[s->lap_joint[1]].r[k][3];
            lh[k] = pose[s->lap_joint[2]].r[k][3];
            lk[k] = pose[s->lap_joint[3]].r[k][3];
            rh[k] = pose[s->lap_joint[4]].r[k][3];
            rk[k] = pose[s->lap_joint[5]].r[k][3];
            up[k] = spine[k] - hips[k];
        }
        f->cap_count += cloth_build_lap_bridge(lh, lk, rh, rk, up, g->lap->start_deg, g->lap->full_deg,
                                               g->lap->proximal_m, g->lap->distal_m,
                                               f->caps + f->cap_count);
    }
    return true;
}

static void blend3(const float a[3], const float b[3], float u, float out[3]) {
    for (int k = 0; k < 3; ++k)
        out[k] = a[k] + u * (b[k] - a[k]);
}

static void interpolate(const GarmentAsset *asset, GarmentPiece *piece, float u) {
    const GarmentFrame *prev = &piece->previous;
    const GarmentFrame *curr = &scratch.current;
    GarmentFrame *step = &scratch.step;
    for (int i = 0; i < asset->mesh->vertices; ++i) {
        blend3(prev->anchors[i], curr->anchors[i], u, step->anchors[i]);
        step->guide[i] = curr->guide[i];
        blend3(prev->guide[i].normal, curr->guide[i].normal, u, step->guide[i].normal);
        if (!normalize(step->guide[i].normal))
            memcpy(step->guide[i].normal, curr->guide[i].normal, 3 * sizeof(float));
    }
    step->cap_count = curr->cap_count;
    for (int i = 0; i < curr->cap_count; ++i) {
        step->caps[i] = curr->caps[i];
        if (i < prev->cap_count) {
            blend3(prev->caps[i].a, curr->caps[i].a, u, step->caps[i].a);
            blend3(prev->caps[i].b, curr->caps[i].b, u, step->caps[i].b);
        }
        /* A render pose can fall between physical steps. Sweep from the last solved endpoint. */
        if (i < piece->continuous_cap_count) {
            memcpy(step->caps[i].prev_a, piece->sim_caps[i].a, sizeof step->caps[i].prev_a);
            memcpy(step->caps[i].prev_b, piece->sim_caps[i].b, sizeof step->caps[i].prev_b);
            step->caps[i].has_prev = 1U;
        } else {
            step->caps[i].has_prev = 0U;
        }
    }
    piece->sim_cap_count = step->cap_count;
    piece->continuous_cap_count = step->cap_count;
    /* A step on a pose timestamp shares the exact input endpoint with the next interval. */
    memcpy(piece->sim_caps, u == 1.0F ? curr->caps : step->caps, (size_t)step->cap_count * sizeof *step->caps);
}

static void geometry_stats(GarmentStats *stats, const GarmentPiece *s, const ClothMesh *m) {
    for (int i = 0; i < m->edges; ++i) {
        const ClothEdge *e = &m->edge[i];
        float length2 = 0;
        for (int k = 0; k < 3; ++k) {
            float v = s->cloth.x[e->a][k] - s->cloth.x[e->b][k];
            length2 += v * v;
        }
        stats->max_strain = fmaxf(stats->max_strain, sqrtf(length2) / e->length - 1.0F);
    }
    for (int i = 0; i < m->vertices; ++i) {
        float error2 = 0;
        for (int k = 0; k < 3; ++k) {
            float value = s->cloth.x[i][k];
            if (!isfinite(value))
                stats->finite = false;
            float d = value - s->sim_anchors[i][k];
            error2 += d * d;
        }
        if (m->inv_area[i] == 0)
            stats->pin_error_m = fmaxf(stats->pin_error_m, sqrtf(error2));
    }
}

static bool local_support(const ClothSupport *source, const float world[16], ClothSupport *local) {
    mat3 basis, inverse;
    for (int i = 0; i < 3; ++i) glm_vec3_copy((float *)&world[4 * i], basis[i]);
    float det = glm_mat3_det(basis);
    if (!isfinite(det) || fabsf(det) < 1e-12F) return false;
    glm_mat3_inv(basis, inverse);
    vec3 center;
    glm_vec3_sub((float *)source->center, (float *)&world[12], center);
    glm_mat3_mulv(inverse, center, local->center);
    glm_mat3_mulv(inverse, (float *)source->half_u, local->half_u);
    glm_mat3_mulv(inverse, (float *)source->half_v, local->half_v);
    if (det < 0.0F) glm_vec3_negate(local->half_v);
    if (source->plane_count < 0 || source->plane_count > CLOTH_SUPPORT_PLANES) return false;
    local->plane_count = source->plane_count;
    for (int f = 0; f < source->plane_count; ++f) {
        for (int k = 0; k < 3; ++k)
            local->planes[f][k] = glm_vec3_dot(basis[k], (float *)source->planes[f]);
        local->planes[f][3] = source->planes[f][3];
    }
    local->depth = source->depth;
    if (source->depth > 0.0F) {
        vec3 world_area, local_area;
        glm_vec3_cross((float *)source->half_u, (float *)source->half_v, world_area);
        glm_vec3_cross(local->half_u, local->half_v, local_area);
        /* Volume scales by the determinant even when the local face is skewed. */
        float area = glm_vec3_norm(local_area);
        if (area <= 1e-12F) return false;
        local->depth *= glm_vec3_norm(world_area) / (fabsf(det) * area);
    }
    return true;
}

static bool simulate(GarmentActor *a, const GarmentSet *set, const nt_skeletal_skeleton_t *skel,
                     const nt_skeletal_mat34_t *pose, const float transform[16], float dt,
                     const ClothSupport *world_support) {
    if (!set || !skel || !pose || !transform || !isfinite(dt) || dt < 0)
        return false;
    ClothSupport support;
    if (world_support && !local_support(world_support, transform, &support)) return false;
    if (!garment_actor_prepare(a, set, skel))
        return false;
    nt_skeletal_mat34_t world;
    float move2 = 0;
    for (int k = 0; k < 3; ++k) {
        float axis[] = {transform[4 * k], transform[4 * k + 1], transform[4 * k + 2]};
        if (!normalize(axis) || !isfinite(transform[12 + k]))
            return false;
        for (int j = 0; j < 3; ++j)
            world.r[j][k] = axis[j];
        world.r[k][3] = transform[12 + k];
        float move = world.r[k][3] - a->origin[k];
        move2 += move * move;
    }
    float yaw = atan2f(world.r[0][2], world.r[2][2]);
    float dyaw = a->primed ? wrap_angle(yaw - a->yaw) : 0.0F;

    const float step = 1.0F / (float)GARMENT_SIM_HZ;
    bool reset = !a->primed || move2 > 4.0F || fabsf(dyaw) > 1.8F;
    float clamped_dt = fminf(dt, (float)GARMENT_MAX_SUBSTEPS * step);
    float elapsed = reset ? 0.0F : a->remainder + clamped_dt;
    int steps = (int)floorf(elapsed / step + 1e-5F);
    if (steps > GARMENT_MAX_SUBSTEPS)
        steps = GARMENT_MAX_SUBSTEPS;

    /* Compute gravity-aligned actor-root fictitious acceleration in local space. */
    float local_accel[3] = {0, 0, 0};
    float yaw_vel = 0.0F, yaw_accel = 0.0F;
    if (!reset && dt > 1e-5F) {
        float inv_dt = 1.0F / dt;
        float world_vel[3] = {
            (world.r[0][3] - a->origin[0]) * inv_dt,
            (world.r[1][3] - a->origin[1]) * inv_dt,
            (world.r[2][3] - a->origin[2]) * inv_dt
        };
        float world_accel[3] = {
            (world_vel[0] - a->prev_vel[0]) * inv_dt,
            (world_vel[1] - a->prev_vel[1]) * inv_dt,
            (world_vel[2] - a->prev_vel[2]) * inv_dt
        };
        for (int k = 0; k < 3; ++k)
            local_accel[k] = world.r[0][k] * world_accel[0] + world.r[1][k] * world_accel[1] + world.r[2][k] * world_accel[2];
        yaw_vel = dyaw * inv_dt;
        yaw_accel = (yaw_vel - a->prev_yaw_vel) * inv_dt;
        memcpy(a->prev_vel, world_vel, sizeof a->prev_vel);
        a->prev_yaw_vel = yaw_vel;
    } else {
        memset(a->prev_vel, 0, sizeof a->prev_vel);
        a->prev_yaw_vel = 0.0F;
    }

    a->stats.max_strain = a->stats.pin_error_m = a->stats.max_displacement_m = 0;
    a->stats.finite = true;
    a->stats.surface_faces = a->stats.driven_joints = 0;
    for (int p = 0; p < set->piece_count; ++p) {
        const GarmentAsset *g = &set->pieces[p];
        GarmentPiece *s = &a->pieces[p];
        if (!sample_frame_local(g, s, pose)) {
            a->stats.finite = false;
            break;
        }
        size_t bytes = (size_t)g->mesh->vertices * sizeof s->sim_anchors[0];
        if (s->continuous_cap_count > scratch.current.cap_count)
            s->continuous_cap_count = scratch.current.cap_count;
        if (reset) {
            cloth_reset_supported(&s->cloth, g->mesh, &g->material, scratch.current.anchors,
                                  scratch.current.guide, scratch.current.caps, scratch.current.cap_count,
                                  world_support ? &support : NULL);
            memcpy(s->sim_anchors, scratch.current.anchors, bytes);
            s->sim_cap_count = scratch.current.cap_count;
            s->continuous_cap_count = scratch.current.cap_count;
            memcpy(s->sim_caps, scratch.current.caps, (size_t)s->sim_cap_count * sizeof *s->sim_caps);
        }
        ClothFrame frame = {
            .linear_accel = {local_accel[0], local_accel[1], local_accel[2]},
            .angular_vel_y = yaw_vel,
            .angular_accel_y = yaw_accel,
            .linear_inertia = g->linear_inertia,
            .angular_inertia = g->angular_inertia,
            .max_fictitious_accel = 28.0F,
            .floor_y = 0.0F,
            .floor_enabled = true,
            .suppress_anim_drive = scratch.current.cap_count > g->collider_count,
            .support = world_support ? &support : NULL
        };
        for (int j = 0; j < steps; ++j) {
            float u = clamped_dt > 1e-6F ? fminf(1.0F, ((float)(j + 1) * step - a->remainder) / clamped_dt) : 1.0F;
            interpolate(g, s, u);
            /* Inherit downward skeletal hip drops so snappy crouches do not outrun gravity and invert skirts. */
            float prev_pin_y = 0.0F, curr_pin_y = 0.0F;
            int pin_count = 0;
            for (int v = 0; v < g->mesh->vertices; ++v) {
                if (g->mesh->inv_area[v] == 0.0F) {
                    prev_pin_y += s->sim_anchors[v][1];
                    curr_pin_y += scratch.step.anchors[v][1];
                    ++pin_count;
                }
            }
            float dy = pin_count ? (curr_pin_y - prev_pin_y) / (float)pin_count : 0.0F;
            frame.anchor_shift_y = g->downward_follow * fminf(dy, 0.0F);
            cloth_step_framed(&s->cloth, g->mesh, &g->material, step, &frame, scratch.step.anchors,
                              scratch.step.guide, scratch.step.caps, scratch.step.cap_count);
            memcpy(s->sim_anchors, scratch.step.anchors, bytes);
            if (!s->cloth.frame_valid)
                break;
        }
        if (!s->cloth.frame_valid) {
            a->stats.finite = false;
            break;
        }
        memcpy(s->previous.anchors, scratch.current.anchors, bytes);
        memcpy(s->previous.guide, scratch.current.guide, (size_t)g->mesh->vertices * sizeof(ClothGuide));
        s->previous.cap_count = scratch.current.cap_count;
        memcpy(s->previous.caps, scratch.current.caps, (size_t)scratch.current.cap_count * sizeof(ClothCapsule));
        geometry_stats(&a->stats, s, g->mesh);
    }
    a->primed = a->stats.finite;
    if (!a->primed)
        return false;
    a->stats.steps = (reset ? 0U : a->stats.steps) + (unsigned)steps;
    a->remainder = reset ? 0.0F : fmaxf(0.0F, elapsed - (float)steps * step);
    a->yaw = yaw;
    for (int k = 0; k < 3; ++k)
        a->origin[k] = world.r[k][3];
    return true;
}

static bool prepare_vertex_frames(GarmentActor *a) {
    if (a->palette_count) return true;
    int total_verts = 0;
    for (int p = 0; p < a->set->piece_count; ++p) total_verts += a->set->pieces[p].mesh->vertices;
    if (total_verts <= 0) return false;
    ClothVertexBasis *basis = calloc((size_t)total_verts, sizeof *basis);
    nt_skeletal_mat34_t *frames = calloc((size_t)total_verts, sizeof *frames);
    if (!basis || !frames) { free(basis); free(frames); return false; }
    int offset = 0;
    for (int p = 0; p < a->set->piece_count; ++p) {
        const ClothMesh *m = a->set->pieces[p].mesh;
        if (!cloth_vertex_frames_prepare(m, scratch.basis)) {
            free(basis); free(frames); return false;
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
                          const nt_skeletal_mat34_t *pose, const float transform[16], float dt, bool render) {
    return garment_actor_update_supported(a, set, skel, pose, transform, dt, render, NULL);
}

bool garment_actor_update_supported(GarmentActor *a, const GarmentSet *set, const nt_skeletal_skeleton_t *skel,
                                    const nt_skeletal_mat34_t *pose, const float transform[16], float dt, bool render,
                                    const ClothSupport *world_support) {
    if (!simulate(a, set, skel, pose, transform, dt, world_support)) return false;
    if (!render) return true;
    if (!prepare_vertex_frames(a)) return false;
    int offset = 0;
    for (int p = 0; p < set->piece_count; ++p) {
        const ClothMesh *m = set->pieces[p].mesh;
        const GarmentPiece *piece = &a->pieces[p];
        for (int v = 0; v < m->vertices; ++v) {
            float d2 = 0.0F;
            for (int k = 0; k < 3; ++k) {
                float d = piece->cloth.x[v][k] - piece->sim_anchors[v][k];
                d2 += d * d;
                /* Compensate for fractional substep remainder so pins stay locked to current frame. */
                scratch.dressed[v][k] = d + piece->previous.anchors[v][k];
            }
            a->stats.max_displacement_m = fmaxf(a->stats.max_displacement_m, sqrtf(d2));
        }
        if (!cloth_vertex_quats_eval(m, a->vertex_basis + offset, scratch.dressed, scratch.mats))
            return false;
        for (int v = 0; v < m->vertices; ++v)
            memcpy(&a->palette[offset + v], scratch.mats[v], sizeof a->palette[0]);
        offset += m->vertices;
    }
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
    if (!(layers & (CLOTH_DEBUG_WEIGHTS | CLOTH_DEBUG_TETHERS | CLOTH_DEBUG_CAPSULES))) return true;
    if (!actor || !actor->primed || !actor->pieces || actor->rig != skel->rig_compat_id.value) return false;
    int offset = 0;
    for (int p = 0; p < actor->set->piece_count; ++p) {
        const GarmentAsset *asset = &actor->set->pieces[p];
        const GarmentPiece *piece = &actor->pieces[p];
        if (layers & CLOTH_DEBUG_WEIGHTS) {
            if (!actor->palette || offset + asset->mesh->vertices > actor->palette_count) return false;
            float *weights = scratch.weights, (*display)[3] = scratch.display;
            for (int i = 0; i < asset->mesh->vertices; ++i) {
                memcpy(display[i], actor->palette[offset + i].r[0], sizeof display[i]);
                if (weight_joint < 0) continue;
                const GarmentVertex *vertex = &asset->vertices[i];
                weights[i] = 0.0F;
                for (int j = 0; j < 4; ++j)
                    if (piece->joint[vertex->bone[j]] == weight_joint) weights[i] += vertex->weight[j];
            }
            cloth_debug_mesh(asset->mesh, display, weight_joint < 0 ? NULL : weights, CLOTH_DEBUG_WEIGHTS, emit, user);
        }
        offset += asset->mesh->vertices;
        if (layers & CLOTH_DEBUG_TETHERS)
            cloth_debug_mesh(asset->mesh, piece->cloth.x, NULL, CLOTH_DEBUG_TETHERS, emit, user);
        if (layers & CLOTH_DEBUG_CAPSULES) {
            for (int i = 0; i < piece->sim_cap_count; ++i) {
                const ClothCapsule *cap = &piece->sim_caps[i];
                if (asset->material.collider_mask && cap->mask && !(asset->material.collider_mask & cap->mask)) continue;
                cloth_debug_capsule(cap, emit, user);
            }
        }
    }
    return true;
}
