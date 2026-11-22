#include "garment/cloth.h"
#include "garment/contact.h"
#include "math/nt_math.h"

#include <float.h>
#include <math.h>
#include <string.h>

static bool finite_point(const float p[3]) {
    return isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2]);
}

static bool capsule_active(const ClothMaterial *m, const ClothCapsule *c) {
    return m->collider_mask == 0U || c->mask == 0U || (m->collider_mask & c->mask) != 0U;
}

bool cloth_mesh_valid(const ClothMesh *mesh) {
    if (!mesh || mesh->vertices < 3 || mesh->vertices > CLOTH_VERTICES ||
        mesh->edges < 3 || mesh->edges > CLOTH_EDGES || mesh->faces < 1 || mesh->faces > 2 * mesh->edges / 3 ||
        !mesh->rest || !mesh->inv_area || !mesh->face || !mesh->edge) return false;
    for (int i = 0; i < mesh->vertices; ++i) {
        if (!finite_point(mesh->rest[i]) || !isfinite(mesh->inv_area[i]) || mesh->inv_area[i] < 0) return false;
        if (mesh->tether) {
            ClothTether t = mesh->tether[i];
            if (t.pin < -1 || t.pin >= mesh->vertices || !isfinite(t.max_length) || t.max_length < 0 ||
                (t.pin >= 0 && mesh->inv_area[t.pin] != 0.0F)) return false;
        }
        if (mesh->anim_drive) {
            float w = mesh->anim_drive[i];
            if (!isfinite(w) || w < 0.0F || w > 1.0F) return false;
        }
    }
    for (int i = 0; i < mesh->faces; ++i) {
        ClothFace f = mesh->face[i];
        if (f.a >= mesh->vertices || f.b >= mesh->vertices || f.c >= mesh->vertices ||
            f.a == f.b || f.b == f.c || f.c == f.a) return false;
    }
    for (int i = 0; i < mesh->edges; ++i) {
        const ClothEdge *e = mesh->edge + i;
        if (e->a < 0 || e->a >= mesh->vertices || e->b < 0 || e->b >= mesh->vertices ||
            e->c < 0 || e->c >= mesh->vertices || e->d < -1 || e->d >= mesh->vertices ||
            e->a == e->b || e->a == e->c || e->a == e->d || e->b == e->c || e->b == e->d || e->c == e->d ||
            !isfinite(e->length) || e->length <= 0 || !isfinite(e->angle) || fabsf(e->angle) > NT_PI ||
            e->kind > CLOTH_EDGE_DIAG) return false;
    }
    return true;
}

static bool frame_motion_valid(const ClothFrame *frame) {
    if (!frame) return true;
    return finite_point(frame->linear_accel) && isfinite(frame->angular_vel_y) && isfinite(frame->angular_accel_y) &&
           isfinite(frame->linear_inertia) && frame->linear_inertia >= 0.0F && frame->linear_inertia <= 1.0F &&
           isfinite(frame->angular_inertia) && frame->angular_inertia >= 0.0F && frame->angular_inertia <= 1.0F &&
           isfinite(frame->max_fictitious_accel) && frame->max_fictitious_accel >= 0.0F;
}

bool cloth_frame_valid(const ClothMesh *mesh, const ClothMaterial *m, float dt,
                       const float anchors[][3], const ClothGuide *guide, const ClothCapsule *body, int count) {
    if (!mesh || mesh->vertices < 3 || mesh->vertices > CLOTH_VERTICES || !m || !anchors || count < 0 || (count && !body) ||
        !isfinite(dt) || dt <= 0 || !isfinite(m->density) || m->density <= 0 || !isfinite(m->stretch) || m->stretch < 0 ||
        !isfinite(m->bend) || m->bend < 0 || !isfinite(m->drag) || m->drag < 0 ||
        !isfinite(m->thickness) || m->thickness < 0 || m->iterations < 1 ||
        !isfinite(m->max_edge_strain) || m->max_edge_strain < 0 ||
        m->strain_passes < 0 || m->strain_passes > CLOTH_MAX_STRAIN_PASSES ||
        !isfinite(m->gravity_scale) || m->gravity_scale < 0 ||
        !isfinite(m->aero_drag) || m->aero_drag < 0 ||
        !isfinite(m->anim_drive) || m->anim_drive < 0 ||
        !isfinite(m->weft_stretch_scale) || m->weft_stretch_scale < 0 ||
        !isfinite(m->diag_stretch_scale) || m->diag_stretch_scale < 0 ||
        !isfinite(m->weft_bend_scale) || m->weft_bend_scale < 0 ||
        !isfinite(m->weft_max_strain) || m->weft_max_strain < 0 ||
        !isfinite(m->contact_friction) || m->contact_friction < 0 || m->contact_friction > 1.0F ||
        !isfinite(m->max_speed_mps) || m->max_speed_mps < 0 ||
        !isfinite(m->compression_limit) || m->compression_limit < 0 || m->compression_limit > 0.5F) return false;
    float scale = m->density / (dt * dt);
    if (!isfinite(scale) || !isfinite(m->stretch * scale) || !isfinite(m->bend * scale)) return false;
    for (int i = 0; i < mesh->vertices; ++i) {
        if (!finite_point(anchors[i])) return false;
        if (guide) {
            const ClothGuide *g = guide + i;
            float length2 = glm_vec3_norm2((float *)g->normal);
            if (!isfinite(length2) || fabsf(length2 - 1.0F) > 0.002F ||
                !isfinite(g->travel) || g->travel < 0 || !isfinite(g->backstop) || g->backstop < 0 ||
                !isfinite(g->backstop_radius) || g->backstop_radius < 0) return false;
        }
    }
    for (int i = 0; i < count; ++i) {
        float rb = cloth_capsule_radius_b(body + i);
        if (!finite_point(body[i].a) || !finite_point(body[i].b) ||
            !isfinite(body[i].radius) || body[i].radius < 0 ||
            !isfinite(body[i].radius_b) || body[i].radius_b < 0 ||
            !isfinite(body[i].radius + m->thickness) || !isfinite(rb + m->thickness)) return false;
        if (body[i].has_prev && (!finite_point(body[i].prev_a) || !finite_point(body[i].prev_b))) return false;
    }
    return true;
}

void cloth_reset(ClothState *s, const ClothMesh *mesh, const float pose[][3]) {
    if (!s) return;
    s->mesh = NULL;
    s->frame_valid = false;
    if (!cloth_mesh_valid(mesh) || !pose) return;
    for (int i = 0; i < mesh->vertices; ++i) {
        if (!finite_point(pose[i])) return;
    }
    size_t bytes = (size_t)mesh->vertices * sizeof s->x[0];
    memcpy(s->x, pose, bytes);
    memset(s->velocity, 0, bytes);
    s->mesh = mesh;
    s->frame_valid = true;
}

static bool depenetrate_reset_vertex(float p[3], const float *guide_n, bool has_parent, const float parent_avg[3],
                                     const ClothCapsule *c, float thickness, float out_n[3]) {
    vec3 ab;
    glm_vec3_sub((float *)c->b, (float *)c->a, ab);
    float l2 = glm_vec3_norm2(ab);
    float ra = c->radius, rb = cloth_capsule_radius_b(c);

    /* Sample point p and, when a resolved parent exists, the segment parent_avg -> p so a vertex
       that spawned completely past the far side of a limb is still detected as piercing it. */
    float best_pen = 0.0F;
    float best_t = 0.0F;
    vec3 best_cl = {0.0F, 0.0F, 0.0F};
    int samples = has_parent ? 4 : 1;
    for (int s = 0; s < samples; ++s) {
        float u = (s == 0) ? 1.0F : ((float)s / (float)samples);
        vec3 q;
        for (int k = 0; k < 3; ++k) q[k] = has_parent ? (parent_avg[k] + u * (p[k] - parent_avg[k])) : p[k];
        vec3 ap, cl, dq;
        glm_vec3_sub(q, (float *)c->a, ap);
        float t = l2 > 1e-12F ? fmaxf(0.0F, fminf(1.0F, glm_vec3_dot(ap, ab) / l2)) : 0.0F;
        glm_vec3_copy((float *)c->a, cl);
        glm_vec3_muladds(ab, t, cl);
        float r_eff = ra + t * (rb - ra) + thickness;
        glm_vec3_sub(q, cl, dq);
        float pen = r_eff - glm_vec3_norm(dq);
        if (pen > best_pen) {
            best_pen = pen;
            best_t = t;
            glm_vec3_copy(cl, best_cl);
        }
    }
    if (best_pen <= 0.0F) return false;

    float r_eff = ra + best_t * (rb - ra) + thickness;
    vec3 d;
    glm_vec3_sub(p, best_cl, d);
    float dist = glm_vec3_norm(d);

    vec3 pref = {0.0F, 0.0F, 0.0F};
    if (guide_n) glm_vec3_copy((float *)guide_n, pref);
    if (has_parent) {
        vec3 up;
        glm_vec3_sub((float *)parent_avg, best_cl, up);
        float ulen = glm_vec3_norm(up);
        if (ulen > 1e-5F) glm_vec3_muladds(up, 0.85F / ulen, pref);
    }
    if (l2 > 1e-12F && best_t > 0.0F && best_t < 1.0F) {
        vec3 axis, perp;
        glm_vec3_scale(ab, 1.0F / sqrtf(l2), axis);
        float along = glm_vec3_dot(pref, axis);
        glm_vec3_copy(pref, perp);
        glm_vec3_muladds(axis, -along, perp);
        if (glm_vec3_norm2(perp) > 0.01F) glm_vec3_copy(perp, pref);
    }
    float plen = glm_vec3_norm(pref);
    bool has_pref = plen > 1e-5F;
    if (has_pref) glm_vec3_scale(pref, 1.0F / plen, pref);

    if (dist < 1e-5F || dist >= r_eff || (has_pref && glm_vec3_dot(d, pref) < 0.0F)) {
        if (has_pref) {
            glm_vec3_copy(best_cl, p);
            glm_vec3_muladds(pref, r_eff, p);
            glm_vec3_copy(pref, out_n);
            return true;
        }
    }
    vec3 normal;
    float depth = cloth_point_capsule_contact(p, c, thickness, normal, NULL);
    if (depth > 0.0F) {
        glm_vec3_muladds(normal, depth, p);
        glm_vec3_copy(normal, out_n);
        return true;
    }
    return false;
}

static void stretch(ClothState *s, const ClothEdge *e, const float *w, float alpha,
                    float compress_limit, float *lambda) {
    vec3 dx;
    glm_vec3_sub(s->x[e->b], s->x[e->a], dx);
    float length = glm_vec3_norm(dx), weight = w[e->a] + w[e->b];
    if (length < 1e-8F || weight <= 0) return;
    float target = e->length;
    if (compress_limit > 0.0F && length < e->length) {
        /* Preserve circumferential WEFT hoops against radial collapse while allowing WARP/DIAG
           creases to gather softly without coarse-mesh Euler buckling. */
        bool is_weft = e->kind == CLOTH_EDGE_WEFT;
        float eff_limit = is_weft ? 0.5F * compress_limit : compress_limit;
        float min_len = e->length * (1.0F - eff_limit);
        if (length >= min_len) {
            alpha += (is_weft ? 3.0F : 12.0F) * weight;
        } else {
            target = min_len;
        }
    }
    float dl = (target - length - alpha * *lambda) / (weight + alpha);
    *lambda += dl;
    glm_vec3_muladds(dx, -dl * w[e->a] / length, s->x[e->a]);
    glm_vec3_muladds(dx, dl * w[e->b] / length, s->x[e->b]);
}

static void bend(ClothState *s, const ClothEdge *e, const float *w, float alpha, float *lambda) {
    int ids[] = {e->a, e->b, e->c, e->d};
    vec3 edge, ac, ad, n1, n2, cross, grad[4];
    glm_vec3_sub(s->x[e->b], s->x[e->a], edge);
    glm_vec3_sub(s->x[e->c], s->x[e->a], ac);
    glm_vec3_sub(s->x[e->d], s->x[e->a], ad);
    glm_vec3_cross(edge, ac, n1);
    glm_vec3_cross(ad, edge, n2);
    float edge2 = glm_vec3_norm2(edge), a2 = glm_vec3_norm2(n1), b2 = glm_vec3_norm2(n2);
    if (edge2 < 1e-12F || a2 < 1e-15F || b2 < 1e-15F) return;
    float length = sqrtf(edge2);
    float norm1 = sqrtf(a2), norm2 = sqrtf(b2);
    /* Guard sliver triangles whose small altitude h = |n| / L explodes dihedral gradients. */
    float min_norm = 0.05F * edge2;
    if (norm1 < min_norm || norm2 < min_norm) return;
    float reg_norm = 0.15F * edge2;
    float a2_reg = fmaxf(a2, reg_norm * reg_norm);
    float b2_reg = fmaxf(b2, reg_norm * reg_norm);
    glm_vec3_cross(n1, n2, cross);
    float angle = atan2f(glm_vec3_dot(edge, cross) / length, glm_vec3_dot(n1, n2));
    float error = angle - e->angle;
    if (error > NT_PI) error -= 2.0F * NT_PI;
    if (error < -NT_PI) error += 2.0F * NT_PI;
    glm_vec3_scale(n1, -length / a2_reg, grad[2]);
    glm_vec3_scale(n2, -length / b2_reg, grad[3]);
    float u = glm_vec3_dot(ac, edge) / edge2, v = glm_vec3_dot(ad, edge) / edge2;
    glm_vec3_scale(grad[2], -u, grad[1]);
    glm_vec3_muladds(grad[3], -v, grad[1]);
    glm_vec3_scale(grad[2], u - 1.0F, grad[0]);
    glm_vec3_muladds(grad[3], v - 1.0F, grad[0]);
    float weight = 0;
    for (int i = 0; i < 4; ++i) weight += w[ids[i]] * glm_vec3_norm2(grad[i]);
    if (weight <= 0) return;
    /* Grinspun Discrete Shells geometric compliance factor: (A1 + A2) / (3 * L^2). */
    float geom = fmaxf(0.15F, fminf(2.0F, (norm1 + norm2) / (6.0F * edge2)));
    float alpha_e = alpha * geom;
    float dl = (-error - alpha_e * *lambda) / (weight + alpha_e);
    *lambda += dl;
    for (int i = 0; i < 4; ++i) glm_vec3_muladds(grad[i], dl * w[ids[i]], s->x[ids[i]]);
}

typedef struct {
    float normal[CLOTH_VERTICES][3];
    float vel[CLOTH_VERTICES][3];
    float depth[CLOTH_VERTICES];
    int count[CLOTH_VERTICES];
} ContactAccum;

static void record_contact(ContactAccum *acc, int v, const float n[3], const ClothCapsule *c,
                           float t, float inv_dt, float shift) {
    for (int k = 0; k < 3; ++k) acc->normal[v][k] += n[k];
    if (c->has_prev) {
        for (int k = 0; k < 3; ++k) {
            float va = (c->a[k] - c->prev_a[k]) * inv_dt;
            float vb = (c->b[k] - c->prev_b[k]) * inv_dt;
            acc->vel[v][k] += va + t * (vb - va);
        }
    }
    acc->depth[v] += shift;
    ++acc->count[v];
}

typedef struct {
    float min[3], max[3];
    float r_min;
    int idx;
} CapsuleBox;

static inline bool point_in_box(const float p[3], const float bmin[3], const float bmax[3]) {
    return p[0] >= bmin[0] && p[0] <= bmax[0] &&
           p[1] >= bmin[1] && p[1] <= bmax[1] &&
           p[2] >= bmin[2] && p[2] <= bmax[2];
}

static inline bool box_overlaps(const float amin[3], const float amax[3],
                                const float bmin[3], const float bmax[3]) {
    return amin[0] <= bmax[0] && amax[0] >= bmin[0] &&
           amin[1] <= bmax[1] && amax[1] >= bmin[1] &&
           amin[2] <= bmax[2] && amax[2] >= bmin[2];
}

typedef struct {
    CapsuleBox boxes[64];
    float all_min[3], all_max[3];
    int count;
} CapsuleBroadphase;

static void build_broadphase(const ClothMaterial *m, const ClothCapsule *body, int count, CapsuleBroadphase *bp) {
    bp->count = 0;
    for (int k = 0; k < 3; ++k) {
        bp->all_min[k] = FLT_MAX;
        bp->all_max[k] = -FLT_MAX;
    }
    float thickness = m->thickness;
    for (int j = 0; j < count && bp->count < 64; ++j) {
        if (!capsule_active(m, body + j)) continue;
        float ra = body[j].radius + thickness;
        float rb = cloth_capsule_radius_b(body + j) + thickness;
        float r_max = fmaxf(ra, rb);
        CapsuleBox *b = &bp->boxes[bp->count++];
        b->idx = j;
        b->r_min = fminf(ra, rb);
        for (int k = 0; k < 3; ++k) {
            b->min[k] = fminf(body[j].a[k], body[j].b[k]) - r_max;
            b->max[k] = fmaxf(body[j].a[k], body[j].b[k]) + r_max;
            if (b->min[k] < bp->all_min[k]) bp->all_min[k] = b->min[k];
            if (b->max[k] > bp->all_max[k]) bp->all_max[k] = b->max[k];
        }
    }
}

static void project_body(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m,
                         const float x_prev[][3], const ClothGuide *guide,
                         const ClothCapsule *body, const CapsuleBroadphase *bp,
                         const ClothFrame *frame, float inv_dt, bool full_contact, ContactAccum *acc) {
    float thickness = m->thickness;
    if (frame && frame->floor_enabled) {
        float y_min = frame->floor_y + thickness;
        const ClothCapsule floor_dummy = {0};
        const float floor_n[3] = {0.0F, 1.0F, 0.0F};
        for (int i = 0; i < mesh->vertices; ++i) {
            if (mesh->inv_area[i] == 0.0F) continue;
            if (s->x[i][1] < y_min) {
                float depth = y_min - s->x[i][1];
                s->x[i][1] = y_min;
                if (acc) record_contact(acc, i, floor_n, &floor_dummy, 0.0F, inv_dt, depth);
            }
        }
    }
    if (!bp || bp->count <= 0) return;

    const int active_count = bp->count;
    const CapsuleBox *boxes = bp->boxes;
    const float *all_min = bp->all_min, *all_max = bp->all_max;

    /* 1. Exact round-cone SDF on free vertices (AABB broadphase + temporal side disambiguation). */
    for (int i = 0; i < mesh->vertices; ++i) {
        if (mesh->inv_area[i] == 0.0F) continue;
        if (!point_in_box(s->x[i], all_min, all_max)) continue;
        const float *fallback = guide ? guide[i].normal : NULL;
        for (int bi = 0; bi < active_count; ++bi) {
            const CapsuleBox *bx = &boxes[bi];
            if (!point_in_box(s->x[i], bx->min, bx->max)) continue;
            const ClothCapsule *cap = body + bx->idx;
            vec3 normal;
            float t = 0.0F;
            float depth = cloth_point_capsule_contact_swept(s->x[i], x_prev[i], cap, thickness,
                                                            fallback, normal, &t);
            if (depth <= 0.0F) continue;
            glm_vec3_muladds(normal, depth, s->x[i]);
            if (acc) record_contact(acc, i, normal, cap, t, inv_dt, depth);
        }
    }

    /* 2. Exact round-cone SDF on edge midpoints (covers triangle edges without barycentric blowup). */
    for (int e = 0; e < mesh->edges; ++e) {
        const ClothEdge *edge = &mesh->edge[e];
        float wa = mesh->inv_area[edge->a], wb = mesh->inv_area[edge->b];
        if (wa + wb <= 0.0F) continue;
        vec3 mid;
        for (int k = 0; k < 3; ++k) mid[k] = 0.5F * (s->x[edge->a][k] + s->x[edge->b][k]);
        if (!point_in_box(mid, all_min, all_max)) continue;
        vec3 prev_mid, fb_avg;
        const float *fallback = NULL;
        bool lazy_init = false;
        for (int bi = 0; bi < active_count; ++bi) {
            const CapsuleBox *bx = &boxes[bi];
            if (!point_in_box(mid, bx->min, bx->max)) continue;
            if (!lazy_init) {
                for (int k = 0; k < 3; ++k) prev_mid[k] = 0.5F * (x_prev[edge->a][k] + x_prev[edge->b][k]);
                if (guide) {
                    for (int k = 0; k < 3; ++k) fb_avg[k] = guide[edge->a].normal[k] + guide[edge->b].normal[k];
                    fallback = fb_avg;
                }
                lazy_init = true;
            }
            const ClothCapsule *cap = body + bx->idx;
            vec3 normal;
            float t = 0.0F;
            float depth = cloth_point_capsule_contact_swept(mid, prev_mid, cap, thickness,
                                                            fallback, normal, &t);
            if (depth <= 0.0F) continue;
            /* Midpoint shift = 0.5*(da + db) = depth => scale = 2*depth/(wa+wb), capped near pins. */
            float max_w = fmaxf(wa, wb);
            float scale = fminf(2.0F * depth / (wa + wb), 0.5F * edge->length / max_w);
            if (wa > 0.0F) {
                glm_vec3_muladds(normal, scale * wa, s->x[edge->a]);
                if (acc) record_contact(acc, edge->a, normal, cap, t, inv_dt, scale * wa);
            }
            if (wb > 0.0F) {
                glm_vec3_muladds(normal, scale * wb, s->x[edge->b]);
                if (acc) record_contact(acc, edge->b, normal, cap, t, inv_dt, scale * wb);
            }
            for (int k = 0; k < 3; ++k) mid[k] = 0.5F * (s->x[edge->a][k] + s->x[edge->b][k]);
        }
    }
    if (!full_contact) return;

    /* 3. Face-interior fallback only when a triangle is large relative to a capsule radius. */
    for (int f = 0; f < mesh->faces; ++f) {
        const ClothFace face = mesh->face[f];
        const float *pa = s->x[face.a], *pb = s->x[face.b], *pc = s->x[face.c];
        float fmin[3], fmax[3];
        for (int k = 0; k < 3; ++k) {
            fmin[k] = fminf(pa[k], fminf(pb[k], pc[k]));
            fmax[k] = fmaxf(pa[k], fmaxf(pb[k], pc[k]));
        }
        if (!box_overlaps(fmin, fmax, all_min, all_max)) continue;
        bool edges_measured = false;
        float max_e = 0.0F, min_e = 0.0F;
        int ids[] = {face.a, face.b, face.c};
        for (int bi = 0; bi < active_count; ++bi) {
            const CapsuleBox *bx = &boxes[bi];
            if (!box_overlaps(fmin, fmax, bx->min, bx->max)) continue;
            if (!edges_measured) {
                float e0 = glm_vec3_distance((float *)mesh->rest[face.a], (float *)mesh->rest[face.b]);
                float e1 = glm_vec3_distance((float *)mesh->rest[face.b], (float *)mesh->rest[face.c]);
                float e2 = glm_vec3_distance((float *)mesh->rest[face.c], (float *)mesh->rest[face.a]);
                max_e = fmaxf(e0, fmaxf(e1, e2));
                min_e = fminf(e0, fminf(e1, e2));
                edges_measured = true;
            }
            if (max_e <= 1.5F * bx->r_min) continue;
            const ClothCapsule *cap = body + bx->idx;
            vec3 bary, normal;
            float depth = cloth_face_contact(pa, pb, pc, cap, thickness, bary, normal);
            if (depth <= 0.0F) continue;
            float weight = 0.0F, largest = 0.0F;
            for (int i = 0; i < 3; ++i) {
                float bw = bary[i] * mesh->inv_area[ids[i]];
                weight += bary[i] * bw;
                largest = fmaxf(largest, bw);
            }
            if (weight < 1e-8F || largest < 1e-8F) continue;
            /* Clamp against the shortest face edge so an irregular triangle cannot spike strain. */
            float impulse = fminf(depth / weight, 0.25F * min_e / largest);
            for (int i = 0; i < 3; ++i) {
                if (mesh->inv_area[ids[i]] <= 0.0F) continue;
                float shift = impulse * bary[i] * mesh->inv_area[ids[i]];
                glm_vec3_muladds(normal, shift, s->x[ids[i]]);
                if (acc) record_contact(acc, ids[i], normal, cap, 0.5F, inv_dt, shift);
            }
        }
    }
}

static void project_tethers(ClothState *s, const ClothMesh *mesh) {
    for (int i = 0; i < mesh->vertices; ++i) {
        ClothTether t = mesh->tether[i];
        if (t.pin < 0 || mesh->inv_area[i] == 0.0F) continue;
        vec3 offset;
        glm_vec3_sub(s->x[i], s->x[t.pin], offset);
        float distance2 = glm_vec3_norm2(offset);
        if (distance2 > t.max_length * t.max_length) {
            glm_vec3_scale(offset, t.max_length / sqrtf(distance2), offset);
            glm_vec3_add(s->x[t.pin], offset, s->x[i]);
        }
    }
}

static void project_guide(float x[3], const float anchor[3], const ClothGuide *g) {
    vec3 offset;
    glm_vec3_sub(x, (float *)anchor, offset);
    if (g->backstop_radius > 0.0F) {
        /* Spherical backstop behind anchor: ||x - (anchor - (R + backstop) * n)|| >= R. */
        float R = g->backstop_radius;
        float shift = R + g->backstop;
        vec3 rel;
        for (int k = 0; k < 3; ++k) rel[k] = offset[k] + shift * g->normal[k];
        float d2 = glm_vec3_norm2(rel);
        if (d2 < R * R) {
            if (d2 > 1e-12F) {
                float d = sqrtf(d2);
                for (int k = 0; k < 3; ++k) offset[k] = rel[k] * (R / d) - shift * g->normal[k];
            } else {
                for (int k = 0; k < 3; ++k) offset[k] = -g->backstop * g->normal[k];
            }
        }
    } else {
        float inward = -glm_vec3_dot(offset, (float *)g->normal) - g->backstop;
        if (inward > 0.0F) glm_vec3_muladds((float *)g->normal, inward, offset);
    }
    float length = glm_vec3_norm(offset);
    if (length > g->travel) {
        if (g->travel == 0.0F) glm_vec3_zero(offset);
        else glm_vec3_scale(offset, g->travel / length, offset);
    }
    glm_vec3_add((float *)anchor, offset, x);
}

static void project_tangent_if_contact(float corr[3], int v, const ContactAccum *acc) {
    if (!acc || acc->count[v] == 0) return;
    vec3 n;
    glm_vec3_copy((float *)acc->normal[v], n);
    float nlen = glm_vec3_norm(n);
    if (nlen <= 1e-7F) return;
    glm_vec3_scale(n, 1.0F / nlen, n);
    float into = glm_vec3_dot(corr, n);
    if (into < 0.0F) glm_vec3_muladds(n, -into, corr);
}

static void limit_strain(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, const ContactAccum *acc) {
    float warp_ratio = 1.0F + m->max_edge_strain;
    float weft_ratio = 1.0F + (m->weft_max_strain > 0.0F ? m->weft_max_strain : m->max_edge_strain);
    for (int pass = 0; pass < m->strain_passes; ++pass) {
        bool moved = false;
        for (int k = 0; k < mesh->edges; ++k) {
            const ClothEdge *e = &mesh->edge[pass % 2 == 0 ? k : mesh->edges - 1 - k];
            vec3 delta;
            glm_vec3_sub(s->x[e->b], s->x[e->a], delta);
            float ratio = (e->kind == CLOTH_EDGE_WARP) ? warp_ratio : weft_ratio;
            float length = glm_vec3_norm(delta), target = ratio * e->length;
            float wa = mesh->inv_area[e->a], wb = mesh->inv_area[e->b];
            if (length <= target || wa + wb == 0.0F) continue;
            moved = true;
            float scale = (length - target) / (length * (wa + wb));
            if (wa > 0.0F) {
                vec3 corr;
                glm_vec3_scale(delta, scale * wa, corr);
                project_tangent_if_contact(corr, e->a, acc);
                glm_vec3_add(s->x[e->a], corr, s->x[e->a]);
            }
            if (wb > 0.0F) {
                vec3 corr;
                glm_vec3_scale(delta, -scale * wb, corr);
                project_tangent_if_contact(corr, e->b, acc);
                glm_vec3_add(s->x[e->b], corr, s->x[e->b]);
            }
        }
        if (!moved) break;
    }
}

static void compute_vertex_normals(const ClothMesh *mesh, const float pos[][3], float out[][3]) {
    memset(out, 0, (size_t)mesh->vertices * sizeof out[0]);
    for (int f = 0; f < mesh->faces; ++f) {
        ClothFace face = mesh->face[f];
        vec3 ab, ac, n;
        glm_vec3_sub((float *)pos[face.b], (float *)pos[face.a], ab);
        glm_vec3_sub((float *)pos[face.c], (float *)pos[face.a], ac);
        glm_vec3_cross(ab, ac, n);
        glm_vec3_add(out[face.a], n, out[face.a]);
        glm_vec3_add(out[face.b], n, out[face.b]);
        glm_vec3_add(out[face.c], n, out[face.c]);
    }
    for (int i = 0; i < mesh->vertices; ++i) {
        float len = glm_vec3_norm(out[i]);
        if (len > 1e-8F) glm_vec3_scale(out[i], 1.0F / len, out[i]);
        else glm_vec3_zero(out[i]);
    }
}

void cloth_step_framed(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                       const ClothFrame *frame, const float anchors[][3], const ClothGuide *guide,
                       const ClothCapsule *body, int count) {
    if (!s) return;
    s->frame_valid = s->mesh == mesh && frame_motion_valid(frame) &&
                     cloth_frame_valid(mesh, m, dt, anchors, guide, body, count);
    if (!s->frame_valid) return;

    float x_prev[CLOTH_VERTICES][3];
    float v_normals[CLOTH_VERTICES][3];
    uint8_t anim_ok[CLOTH_VERTICES];
    ContactAccum acc;
    size_t v_count = (size_t)mesh->vertices;
    memset(acc.normal, 0, v_count * sizeof acc.normal[0]);
    memset(acc.vel, 0, v_count * sizeof acc.vel[0]);
    memset(acc.depth, 0, v_count * sizeof acc.depth[0]);
    memset(acc.count, 0, v_count * sizeof acc.count[0]);

    CapsuleBroadphase bp;
    build_broadphase(m, body, count, &bp);

    float inv_dt = 1.0F / dt;
    float g_scale = m->gravity_scale > 0.0F ? m->gravity_scale : 1.0F;
    float retain = expf(-m->drag * dt);
    float aero_damp = m->aero_drag > 0.0F ? (1.0F - expf(-m->aero_drag * dt)) : 0.0F;
    float max_fict = (frame && frame->max_fictitious_accel > 0.0F) ? frame->max_fictitious_accel : 30.0F;

    if (aero_damp > 0.0F) compute_vertex_normals(mesh, s->x, v_normals);

    for (int i = 0; i < mesh->vertices; ++i) {
        glm_vec3_copy(s->x[i], x_prev[i]);
        if (mesh->inv_area[i] == 0.0F) {
            glm_vec3_copy((float *)anchors[i], s->x[i]);
            for (int k = 0; k < 3; ++k) s->velocity[i][k] = (anchors[i][k] - x_prev[i][k]) * inv_dt;
            continue;
        }
        vec3 accel = {0.0F, -9.81F * g_scale, 0.0F};
        if (frame) {
            vec3 fict = {
                -frame->linear_inertia * frame->linear_accel[0],
                -frame->linear_inertia * frame->linear_accel[1],
                -frame->linear_inertia * frame->linear_accel[2]
            };
            float w2 = frame->angular_vel_y * frame->angular_vel_y;
            float ay = frame->angular_accel_y;
            /* Yaw around +Y in right-handed Y-up: centrifugal (+w^2 * r_xz) and Euler (-ay * (y x r)). */
            fict[0] += frame->angular_inertia * (w2 * s->x[i][0] - ay * s->x[i][2]);
            fict[2] += frame->angular_inertia * (w2 * s->x[i][2] + ay * s->x[i][0]);
            float flen = glm_vec3_norm(fict);
            if (flen > max_fict) glm_vec3_scale(fict, max_fict / flen, fict);
            glm_vec3_add(accel, fict, accel);
        }
        glm_vec3_muladds(accel, dt, s->velocity[i]);
        glm_vec3_scale(s->velocity[i], retain, s->velocity[i]);
        if (aero_damp > 0.0F) {
            float vn = glm_vec3_dot(s->velocity[i], v_normals[i]);
            glm_vec3_muladds(v_normals[i], -aero_damp * vn, s->velocity[i]);
        }
        glm_vec3_muladds(s->velocity[i], dt, s->x[i]);
    }

    bool use_anim = mesh->anim_drive != NULL && m->anim_drive > 0.0F && !(frame && frame->suppress_anim_drive);
    if (use_anim) {
        for (int i = 0; i < mesh->vertices; ++i) {
            anim_ok[i] = (mesh->inv_area[i] > 0.0F && mesh->anim_drive[i] > 0.0F) ? 1U : 0U;
            if (!anim_ok[i] || bp.count == 0 || !point_in_box(anchors[i], bp.all_min, bp.all_max)) continue;
            /* Do not drive cloth toward an anchor occluded inside a body collider. */
            for (int bi = 0; bi < bp.count; ++bi) {
                const CapsuleBox *bx = &bp.boxes[bi];
                const ClothCapsule *cap = body + bx->idx;
                if (cap->flags != CLOTH_COLLIDER_INSIDE || !point_in_box(anchors[i], bx->min, bx->max)) continue;
                vec3 tmp_n;
                if (cloth_point_capsule_contact(anchors[i], cap, m->thickness, tmp_n, NULL) > 0.0F) {
                    anim_ok[i] = 0U;
                    break;
                }
            }
        }
    }

    /* Weights are inverse area; multiplying compliance by density cancels the same factor in mass. */
    float scale = m->density / (dt * dt);
    float weft_s = m->weft_stretch_scale > 0.0F ? m->weft_stretch_scale : 1.0F;
    float diag_s = m->diag_stretch_scale > 0.0F ? m->diag_stretch_scale : 1.0F;
    float weft_b = m->weft_bend_scale > 0.0F ? m->weft_bend_scale : 1.0F;

    memset(s->stretch_lambda, 0, (size_t)mesh->edges * sizeof(float));
    memset(s->bend_lambda, 0, (size_t)mesh->edges * sizeof(float));
    memset(s->anim_lambda, 0, (size_t)mesh->vertices * sizeof(float));

    for (int pass = 0; pass < m->iterations; ++pass) {
        if (use_anim) {
            for (int i = 0; i < mesh->vertices; ++i) {
                if (!anim_ok[i]) continue;
                float w2 = mesh->anim_drive[i] * m->anim_drive;
                float alpha = 1.0F / (w2 * dt * dt);
                vec3 dx;
                glm_vec3_sub(s->x[i], (float *)anchors[i], dx);
                float dist = glm_vec3_norm(dx);
                if (dist < 1e-8F) continue;
                float dl = (-dist - alpha * s->anim_lambda[i]) / (1.0F + alpha);
                s->anim_lambda[i] += dl;
                glm_vec3_muladds(dx, dl / dist, s->x[i]);
            }
        }
        for (int k = 0; k < mesh->edges; ++k) {
            int i = (pass % 2 == 0) ? k : mesh->edges - 1 - k;
            const ClothEdge *e = mesh->edge + i;
            if (e->d >= 0) {
                float b_scale = (e->kind == CLOTH_EDGE_WARP) ? 1.0F : weft_b;
                bend(s, e, mesh->inv_area, m->bend * b_scale * scale, &s->bend_lambda[i]);
            }
        }
        for (int k = 0; k < mesh->edges; ++k) {
            int i = (pass % 2 == 0) ? k : mesh->edges - 1 - k;
            const ClothEdge *e = mesh->edge + i;
            float s_scale = e->kind == CLOTH_EDGE_WEFT ? weft_s : (e->kind == CLOTH_EDGE_DIAG ? diag_s : 1.0F);
            stretch(s, e, mesh->inv_area, m->stretch * s_scale * scale, m->compression_limit, &s->stretch_lambda[i]);
        }
        if (mesh->tether) project_tethers(s, mesh);
        if (guide) {
            for (int i = 0; i < mesh->vertices; ++i) {
                if (mesh->inv_area[i] != 0.0F) project_guide(s->x[i], anchors[i], &guide[i]);
            }
        }
        project_body(s, mesh, m, x_prev, guide, body, &bp, frame, inv_dt, pass == m->iterations - 1, &acc);
    }

    if (m->strain_passes) limit_strain(s, mesh, m, &acc);

    for (int i = 0; i < mesh->vertices; ++i) {
        if (mesh->inv_area[i] == 0.0F) continue;
        vec3 v_free, v;
        glm_vec3_copy(s->velocity[i], v_free);
        for (int k = 0; k < 3; ++k) v[k] = (s->x[i][k] - x_prev[i][k]) * inv_dt;
        if (acc.count[i] > 0) {
            vec3 cn, v_body;
            glm_vec3_copy(acc.normal[i], cn);
            float nlen = glm_vec3_norm(cn);
            float inv_n = 1.0F / (float)acc.count[i];
            for (int k = 0; k < 3; ++k) v_body[k] = acc.vel[i][k] * inv_n;
            if (nlen > 1e-7F) {
                glm_vec3_scale(cn, 1.0F / nlen, cn);
                vec3 v_rel, v_tan, v_rel_free, v_free_tan;
                glm_vec3_sub(v, v_body, v_rel);
                float vn_rel = glm_vec3_dot(v_rel, cn);
                glm_vec3_copy(v_rel, v_tan);
                glm_vec3_muladds(cn, -vn_rel, v_tan);
                /* Prevent normal collision projections from injecting spurious tangential energy. */
                glm_vec3_sub(v_free, v_body, v_rel_free);
                glm_vec3_copy(v_rel_free, v_free_tan);
                glm_vec3_muladds(cn, -glm_vec3_dot(v_rel_free, cn), v_free_tan);
                float v_tan_len = glm_vec3_norm(v_tan);
                float max_vt = glm_vec3_norm(v_free_tan) + 60.0F * dt;
                if (v_tan_len > max_vt && v_tan_len > 1e-6F) {
                    glm_vec3_scale(v_tan, max_vt / v_tan_len, v_tan);
                    v_tan_len = max_vt;
                }
                /* Substep-rate-invariant dynamic + Coulomb tangential friction. */
                if (v_tan_len > 1e-6F && m->contact_friction > 0.0F) {
                    float dyn_damp = expf(-m->contact_friction * 24.0F * dt);
                    float dv_fric = m->contact_friction * fminf(acc.depth[i] * inv_dt, 2.0F);
                    float slip = fmaxf(0.0F, dyn_damp - dv_fric / v_tan_len);
                    glm_vec3_scale(v_tan, slip, v_tan);
                }
                /* One-sided zero restitution: suppress penetration and projection bounce without suction. */
                float vn_body = glm_vec3_dot(v_body, cn);
                float vn_target = fmaxf(vn_body, glm_vec3_dot(v_free, cn));
                float vn_extra = vn_target - vn_body;
                for (int k = 0; k < 3; ++k) v[k] = v_body[k] + v_tan[k] + vn_extra * cn[k];
            }
        }
        if (m->max_speed_mps > 0.0F) {
            float spd = glm_vec3_norm(v);
            if (spd > m->max_speed_mps) glm_vec3_scale(v, m->max_speed_mps / spd, v);
        }
        glm_vec3_copy(v, s->velocity[i]);
    }
}

void cloth_step(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                const float anchors[][3], const ClothGuide *guide, const ClothCapsule *body, int count) {
    cloth_step_framed(s, mesh, m, dt, NULL, anchors, guide, body, count);
}

static double dot3(const double a[3], const double b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static bool buffers_overlap(const void *a, size_t a_bytes, const void *b, size_t b_bytes) {
    uintptr_t first = (uintptr_t)a, second = (uintptr_t)b;
    return first <= second ? second - first < a_bytes : first - second < b_bytes;
}

bool cloth_offset_surface(const ClothMesh *mesh, const float positions[][3], const float offsets_m[], float output[][3]) {
    if (!positions || !offsets_m || !output || !cloth_mesh_valid(mesh)) return false;
    size_t n = (size_t)mesh->vertices, bytes = n * sizeof output[0];
    if (buffers_overlap(output, bytes, positions, bytes) ||
        buffers_overlap(output, bytes, offsets_m, n * sizeof *offsets_m) ||
        buffers_overlap(output, bytes, mesh, sizeof *mesh) ||
        buffers_overlap(output, bytes, mesh->rest, bytes) ||
        buffers_overlap(output, bytes, mesh->inv_area, n * sizeof *mesh->inv_area) ||
        buffers_overlap(output, bytes, mesh->face, (size_t)mesh->faces * sizeof *mesh->face) ||
        buffers_overlap(output, bytes, mesh->edge, (size_t)mesh->edges * sizeof *mesh->edge) ||
        (mesh->tether && buffers_overlap(output, bytes, mesh->tether, n * sizeof *mesh->tether))) return false;
    bool active = false;
    for (int i = 0; i < mesh->vertices; ++i) {
        if (!finite_point(positions[i]) || !isfinite(offsets_m[i])) return false;
        active |= offsets_m[i] != 0.0F && mesh->inv_area[i] != 0.0F;
    }
    if (!active) {
        memcpy(output, positions, bytes);
        return true;
    }

    double area_sum[CLOTH_VERTICES] = {0};
    memset(output, 0, bytes);
    for (int f = 0; f < mesh->faces; ++f) {
        ClothFace face = mesh->face[f];
        double ab[3], ac[3];
        for (int k = 0; k < 3; ++k) {
            ab[k] = (double)positions[face.b][k] - (double)positions[face.a][k];
            ac[k] = (double)positions[face.c][k] - (double)positions[face.a][k];
        }
        double normal[] = {ab[1]*ac[2] - ab[2]*ac[1], ab[2]*ac[0] - ab[0]*ac[2], ab[0]*ac[1] - ab[1]*ac[0]};
        double area2 = dot3(normal, normal);
        if (area2 <= 1e-14 * dot3(ab, ab) * dot3(ac, ac)) continue;
        double area = sqrt(area2);
        int ids[] = {face.a, face.b, face.c};
        for (int j = 0; j < 3; ++j) {
            int i = ids[j];
            if (offsets_m[i] == 0.0F || mesh->inv_area[i] == 0.0F) continue;
            /* The area-weighted mean fits in float even when a posed triangle's area does not. */
            double total = area_sum[i] + area, inverse = 1.0 / total;
            for (int k = 0; k < 3; ++k) {
                output[i][k] = (float)(((double)output[i][k] * area_sum[i] + normal[k]) * inverse);
            }
            area_sum[i] = total;
        }
    }
    for (int i = 0; i < mesh->vertices; ++i) {
        if (offsets_m[i] == 0.0F || mesh->inv_area[i] == 0.0F) {
            memcpy(output[i], positions[i], sizeof output[i]);
            continue;
        }
        double normal[] = {(double)output[i][0], (double)output[i][1], (double)output[i][2]};
        double normal2 = dot3(normal, normal);
        if (normal2 <= 1e-12) {
            memcpy(output, positions, bytes);
            return false;
        }
        double scale = (double)offsets_m[i] / sqrt(normal2);
        for (int k = 0; k < 3; ++k) {
            double value = (double)positions[i][k] + scale * normal[k];
            if (fabs(value) > (double)FLT_MAX) {
                memcpy(output, positions, bytes);
                return false;
            }
            output[i][k] = (float)value;
        }
    }
    return true;
}

static void cap_shape_residual(float x[3], const float anchor[3], const ClothGuide *g) {
    double travel = (double)g->travel, backstop = (double)g->backstop, bs_r = (double)g->backstop_radius;
    if (travel == 0.0) {
        memcpy(x, anchor, 3 * sizeof *x);
        return;
    }
    double normal[] = {(double)g->normal[0], (double)g->normal[1], (double)g->normal[2]};
    double delta[3];
    double normal_length = sqrt(dot3(normal, normal));
    for (int k = 0; k < 3; ++k) {
        normal[k] /= normal_length;
        delta[k] = (double)x[k] - (double)anchor[k];
    }
    if (bs_r > 0.0) {
        double shift = bs_r + backstop;
        double rel[3] = {delta[0] + shift * normal[0], delta[1] + shift * normal[1], delta[2] + shift * normal[2]};
        double d2 = dot3(rel, rel);
        if (d2 >= bs_r * bs_r && dot3(delta, delta) <= travel * travel) return;
        if (d2 < bs_r * bs_r) {
            if (d2 > 1e-16) {
                double d = sqrt(d2);
                for (int k = 0; k < 3; ++k) delta[k] = rel[k] * (bs_r / d) - shift * normal[k];
            } else {
                for (int k = 0; k < 3; ++k) delta[k] = -backstop * normal[k];
            }
        }
    } else {
        double inward = -dot3(delta, normal) - backstop;
        if (inward <= 0.0 && dot3(delta, delta) <= travel * travel) return;
        if (inward > 0.0) {
            for (int k = 0; k < 3; ++k) delta[k] += inward * normal[k];
        }
    }
    double length = sqrt(dot3(delta, delta));
    if (length > travel) {
        for (int k = 0; k < 3; ++k) delta[k] *= travel / length;
    }

    /* Float world coordinates can round a boundary point outside either limit. Only then
       move toward a point strictly inside both bounds; the anchor is always representable. */
    for (double bias = 0.0; bias < 1.0; bias = bias == 0.0 ? 1e-6 : bias * 16.0) {
        double actual[3];
        for (int k = 0; k < 3; ++k) {
            double offset = delta[k] + bias * (0.5 * travel * normal[k] - delta[k]);
            x[k] = (float)((double)anchor[k] + offset);
            actual[k] = (double)x[k] - (double)anchor[k];
        }
        bool back_ok = true;
        if (bs_r > 0.0) {
            double shift = bs_r + backstop;
            double rel[3] = {actual[0] + shift * normal[0], actual[1] + shift * normal[1], actual[2] + shift * normal[2]};
            back_ok = dot3(rel, rel) >= bs_r * bs_r;
        } else {
            back_ok = dot3(actual, normal) >= -backstop;
        }
        if (dot3(actual, actual) <= travel * travel && back_ok) return;
    }
    memcpy(x, anchor, 3 * sizeof *x);
}

void cloth_reset_over_body(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m,
                           const float anchors[][3], const ClothGuide *guide,
                           const ClothCapsule *body, int count) {
    cloth_reset(s, mesh, anchors);
    if (!s || !s->frame_valid || !m || !body || count <= 0) return;
    if (!cloth_frame_valid(mesh, m, 1.0F / 60.0F, anchors, guide, body, count)) return;

    int n = mesh->vertices;
    int deg[CLOTH_VERTICES] = {0};
    for (int e = 0; e < mesh->edges; ++e) {
        ++deg[mesh->edge[e].a];
        ++deg[mesh->edge[e].b];
    }
    int off[CLOTH_VERTICES + 1];
    off[0] = 0;
    for (int i = 0; i < n; ++i) off[i + 1] = off[i] + deg[i];
    int nb[2 * CLOTH_EDGES];
    int cur[CLOTH_VERTICES];
    memcpy(cur, off, (size_t)n * sizeof cur[0]);
    for (int e = 0; e < mesh->edges; ++e) {
        int a = mesh->edge[e].a, b = mesh->edge[e].b;
        nb[cur[a]++] = b;
        nb[cur[b]++] = a;
    }

    int hop[CLOTH_VERTICES];
    int order[CLOTH_VERTICES];
    int head = 0, tail = 0;
    for (int i = 0; i < n; ++i) {
        if (mesh->inv_area[i] == 0.0F) {
            hop[i] = 0;
            order[tail++] = i;
        } else {
            hop[i] = -1;
        }
    }
    while (head < tail) {
        int u = order[head++];
        for (int k = off[u]; k < off[u + 1]; ++k) {
            int v = nb[k];
            if (hop[v] < 0) {
                hop[v] = hop[u] + 1;
                order[tail++] = v;
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        if (hop[i] < 0) {
            hop[i] = n;
            order[tail++] = i;
        }
    }

    float disp[CLOTH_VERTICES][3];
    memset(disp, 0, (size_t)n * sizeof disp[0]);

    /* Sweep outward from pinned seams so lower hem rows inherit upper-row limb lift and unroll along limbs. */
    for (int oi = 0; oi < tail; ++oi) {
        int v = order[oi];
        if (mesh->inv_area[v] == 0.0F) continue;
        vec3 avg_disp = {0.0F, 0.0F, 0.0F}, parent_avg = {0.0F, 0.0F, 0.0F};
        float parent_rest_len = 0.0F;
        int pcnt = 0;
        for (int k = off[v]; k < off[v + 1]; ++k) {
            int u = nb[k];
            if (hop[u] < hop[v]) {
                glm_vec3_add(avg_disp, disp[u], avg_disp);
                glm_vec3_add(parent_avg, s->x[u], parent_avg);
                parent_rest_len += glm_vec3_distance((float *)anchors[v], (float *)anchors[u]);
                ++pcnt;
            }
        }
        if (pcnt > 0) {
            float inv_p = 1.0F / (float)pcnt;
            glm_vec3_scale(avg_disp, inv_p, avg_disp);
            glm_vec3_scale(parent_avg, inv_p, parent_avg);
            parent_rest_len *= inv_p;
            glm_vec3_add(s->x[v], avg_disp, s->x[v]);
        }
        const float *gn = guide ? guide[v].normal : NULL;
        vec3 cn_sum = {0.0F, 0.0F, 0.0F};
        bool hit = false;
        for (int pass = 0; pass < 2; ++pass) {
            for (int c = 0; c < count; ++c) {
                if (!capsule_active(m, body + c) || body[c].flags != CLOTH_COLLIDER_INSIDE) continue;
                vec3 cn;
                if (depenetrate_reset_vertex(s->x[v], gn, pcnt > 0, parent_avg, body + c, m->thickness, cn)) {
                    glm_vec3_add(cn_sum, cn, cn_sum);
                    hit = true;
                }
            }
        }
        /* When a limb lifts cloth up toward its parent row, unroll the shortened edge along the
           guide-normal tangent over the limb surface instead of stacking vertices at the hip crease. */
        if (hit && gn && pcnt > 0) {
            float cn_len = glm_vec3_norm(cn_sum);
            float cur_len = glm_vec3_distance(s->x[v], parent_avg);
            if (cn_len > 1e-5F && cur_len < parent_rest_len) {
                vec3 cn, tang;
                glm_vec3_scale(cn_sum, 1.0F / cn_len, cn);
                float along = glm_vec3_dot((float *)gn, cn);
                glm_vec3_copy((float *)gn, tang);
                glm_vec3_muladds(cn, -along, tang);
                float tlen = glm_vec3_norm(tang);
                if (tlen > 0.25F) {
                    glm_vec3_scale(tang, 1.0F / tlen, tang);
                    float slide = sqrtf(fmaxf(0.0F, parent_rest_len * parent_rest_len - cur_len * cur_len));
                    glm_vec3_muladds(tang, slide, s->x[v]);
                    for (int c = 0; c < count; ++c) {
                        if (!capsule_active(m, body + c) || body[c].flags != CLOTH_COLLIDER_INSIDE) continue;
                        vec3 cn2;
                        depenetrate_reset_vertex(s->x[v], gn, true, parent_avg, body + c, m->thickness, cn2);
                    }
                }
            }
        }
        glm_vec3_sub(s->x[v], (float *)anchors[v], disp[v]);
    }

    CapsuleBroadphase bp;
    build_broadphase(m, body, count, &bp);
    float scale = m->density * 60.0F * 60.0F;
    float weft_s = m->weft_stretch_scale > 0.0F ? m->weft_stretch_scale : 1.0F;
    float diag_s = m->diag_stretch_scale > 0.0F ? m->diag_stretch_scale : 1.0F;
    for (int pass = 0; pass < 6; ++pass) {
        memset(s->stretch_lambda, 0, (size_t)mesh->edges * sizeof(float));
        for (int k = 0; k < mesh->edges; ++k) {
            int i = (pass % 2 == 0) ? k : mesh->edges - 1 - k;
            const ClothEdge *e = mesh->edge + i;
            float s_scale = e->kind == CLOTH_EDGE_WEFT ? weft_s : (e->kind == CLOTH_EDGE_DIAG ? diag_s : 1.0F);
            stretch(s, e, mesh->inv_area, m->stretch * s_scale * scale, m->compression_limit, &s->stretch_lambda[i]);
        }
        if (mesh->tether) project_tethers(s, mesh);
        for (int i = 0; i < n; ++i) {
            if (mesh->inv_area[i] == 0.0F) glm_vec3_copy((float *)anchors[i], s->x[i]);
        }
        project_body(s, mesh, m, s->x, guide, body, &bp, NULL, 1.0F, true, NULL);
    }
    memset(s->velocity, 0, (size_t)n * sizeof s->velocity[0]);
}

void cloth_step_shaped(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                       const float anchors[][3], const ClothGuide *guide, const ClothCapsule *body, int count) {
    float x_before[CLOTH_VERTICES][3];
    bool valid_before = s && mesh && s->mesh == mesh && mesh->vertices <= CLOTH_VERTICES;
    if (valid_before) memcpy(x_before, s->x, (size_t)mesh->vertices * sizeof x_before[0]);
    cloth_step(s, mesh, m, dt, anchors, guide, body, count);
    if (!s || !s->frame_valid || !guide) return;
    float inv_dt = 1.0F / dt;
    for (int i = 0; i < mesh->vertices; ++i) {
        if (!finite_point(s->x[i]) || !finite_point(s->velocity[i])) {
            s->frame_valid = false;
            return;
        }
        if (mesh->inv_area[i] != 0.0F) {
            cap_shape_residual(s->x[i], anchors[i], &guide[i]);
            for (int k = 0; k < 3; ++k) s->velocity[i][k] = (s->x[i][k] - x_before[i][k]) * inv_dt;
        }
    }
}

ClothMetrics cloth_measure(const ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                           const float anchors[][3], const ClothCapsule *body, int count) {
    if (!s || !s->frame_valid || s->mesh != mesh || !cloth_frame_valid(mesh, m, dt, anchors, NULL, body, count)) return (ClothMetrics){0};
    ClothMetrics out = {.finite = true};
    for (int i = 0; i < mesh->vertices; ++i) {
        for (int k = 0; k < 3; ++k) out.finite &= isfinite(s->x[i][k]) && isfinite(s->velocity[i][k]);
        out.speed = fmaxf(out.speed, glm_vec3_norm((float *)s->velocity[i]));
        if (mesh->inv_area[i] == 0) out.pin_error = fmaxf(out.pin_error, glm_vec3_distance((float *)s->x[i], (float *)anchors[i]));
    }
    for (int i = 0; i < mesh->edges; ++i) {
        const ClothEdge *e = mesh->edge + i;
        float length = glm_vec3_distance((float *)s->x[e->a], (float *)s->x[e->b]);
        out.stretch = fmaxf(out.stretch, length / e->length - 1.0F);
    }
    for (int j = 0; j < mesh->faces; ++j) {
        const ClothFace face = mesh->face[j];
        for (int i = 0; i < count; ++i) {
            if (!capsule_active(m, body + i)) continue;
            vec3 weights, normal;
            out.penetration = fmaxf(out.penetration, cloth_face_contact(s->x[face.a], s->x[face.b], s->x[face.c],
                                                                        body + i, m->thickness, weights, normal));
        }
    }
    out.finite &= isfinite(out.speed) && isfinite(out.stretch) && isfinite(out.penetration) && isfinite(out.pin_error);
    return out;
}
