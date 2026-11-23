#include "garment/cloth.h"
#include "garment/contact.h"
#include "math/nt_math.h"

#include <float.h>
#include <math.h>
#include <string.h>

static bool finite_point(const float p[3]) {
    return isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2]);
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
    return finite_point(frame->linear_accel) && isfinite(frame->anchor_shift_y) && isfinite(frame->angular_vel_y) && isfinite(frame->angular_accel_y) &&
           isfinite(frame->linear_inertia) && frame->linear_inertia >= 0.0F && frame->linear_inertia <= 1.0F &&
           isfinite(frame->angular_inertia) && frame->angular_inertia >= 0.0F && frame->angular_inertia <= 1.0F;
}

bool cloth_frame_valid(const ClothMesh *mesh, const ClothMaterial *m, float dt,
                       const float anchors[][3], const ClothGuide *guide, const ClothCapsule *body, int count) {
    if (!mesh || mesh->vertices < 3 || mesh->vertices > CLOTH_VERTICES || !m || !anchors ||
        count < 0 || count > CLOTH_CAPSULES || (count && !body) ||
        !isfinite(dt) || dt <= 0 || !isfinite(m->density) || m->density <= 0 || !isfinite(m->stretch) || m->stretch < 0 ||
        !isfinite(m->bend) || m->bend < 0 || !isfinite(m->drag) || m->drag < 0 ||
        !isfinite(m->thickness) || m->thickness < 0 || m->iterations < 1 ||
        !isfinite(m->max_edge_strain) || m->max_edge_strain < 0 ||
        m->strain_passes < 0 || m->strain_passes > CLOTH_MAX_STRAIN_PASSES ||
        !isfinite(m->aero_drag) || m->aero_drag < 0 ||
        !isfinite(m->anim_drive) || m->anim_drive < 0 ||
        !isfinite(m->weft_stretch_scale) || m->weft_stretch_scale < 0 ||
        !isfinite(m->diag_stretch_scale) || m->diag_stretch_scale < 0 ||
        !isfinite(m->weft_bend_scale) || m->weft_bend_scale < 0 ||
        !isfinite(m->weft_max_strain) || m->weft_max_strain < 0 ||
        !isfinite(m->contact_friction) || m->contact_friction < 0 || m->contact_friction > 1.0F ||
        !isfinite(m->max_speed_mps) || m->max_speed_mps < 0 ||
        !isfinite(m->reset_carry_range_m) || m->reset_carry_range_m < 0 ||
        !isfinite(m->support_drape) || m->support_drape < 0.0F || m->support_drape > 1.0F ||
        !isfinite(m->compression_limit) || m->compression_limit < 0 || m->compression_limit > 0.5F) return false;
    float scale = m->density / (dt * dt);
    if (!isfinite(scale) || !isfinite(m->stretch * scale) || !isfinite(m->bend * scale)) return false;
    for (int i = 0; i < mesh->vertices; ++i) {
        if (!finite_point(anchors[i])) return false;
        if (guide) {
            const ClothGuide *g = guide + i;
            float length2 = glm_vec3_norm2((float *)g->normal);
            if (!isfinite(length2) || fabsf(length2 - 1.0F) > 0.002F ||
                !isfinite(g->travel) || g->travel < 0 || !isfinite(g->backstop) || g->backstop < 0) return false;
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

static inline float fast_atan_unit(float z) {
    float z2 = z * z;
    return z * (0.9998660F + z2 * (-0.3302995F + z2 * (0.1801410F + z2 * (-0.0851330F + z2 * 0.0208351F))));
}

static inline float fast_atan2f(float y, float x) {
    float ay = fabsf(y), ax = fabsf(x);
    if (ax == 0.0F && ay == 0.0F) return 0.0F;
    float r = (ax >= ay) ? fast_atan_unit(ay / ax) : (0.5F * NT_PI - fast_atan_unit(ax / ay));
    if (x < 0.0F) r = NT_PI - r;
    return y < 0.0F ? -r : r;
}

static void stretch(ClothState *s, const ClothEdge *e, const float *w, float alpha,
                    float compress_limit, float *lambda) {
    float wa = w[e->a], wb = w[e->b];
    float weight = wa + wb;
    if (weight <= 0.0F) return;
    vec3 dx;
    glm_vec3_sub(s->x[e->b], s->x[e->a], dx);
    float len2 = glm_vec3_norm2(dx);
    if (len2 < 1e-16F) return;
    float length = sqrtf(len2);
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
    float inv_len = dl / length;
    if (wa > 0.0F) glm_vec3_muladds(dx, -inv_len * wa, s->x[e->a]);
    if (wb > 0.0F) glm_vec3_muladds(dx, inv_len * wb, s->x[e->b]);
}

static void bend(ClothState *s, const ClothEdge *e, const float *w, float alpha, float *lambda) {
    float wa = w[e->a], wb = w[e->b], wc = w[e->c], wd = w[e->d];
    if (wa + wb + wc + wd <= 0.0F) return;
    vec3 edge, ac, ad, n1, n2, cross, grad[4];
    glm_vec3_sub(s->x[e->b], s->x[e->a], edge);
    glm_vec3_sub(s->x[e->c], s->x[e->a], ac);
    glm_vec3_sub(s->x[e->d], s->x[e->a], ad);
    glm_vec3_cross(edge, ac, n1);
    glm_vec3_cross(ad, edge, n2);
    float edge2 = glm_vec3_norm2(edge), a2 = glm_vec3_norm2(n1), b2 = glm_vec3_norm2(n2);
    if (edge2 < 1e-12F || a2 < 1e-15F || b2 < 1e-15F) return;
    /* Guard sliver triangles whose small altitude h = |n| / L explodes dihedral gradients. */
    float min_norm = 0.05F * edge2;
    float min_norm2 = min_norm * min_norm;
    if (a2 < min_norm2 || b2 < min_norm2) return;
    float length = sqrtf(edge2);
    float norm1 = sqrtf(a2), norm2 = sqrtf(b2);
    float reg_norm = 0.15F * edge2;
    float reg_norm2 = reg_norm * reg_norm;
    float a2_reg = a2 > reg_norm2 ? a2 : reg_norm2;
    float b2_reg = b2 > reg_norm2 ? b2 : reg_norm2;
    float inv_edge2 = 1.0F / edge2;
    glm_vec3_cross(n1, n2, cross);
    float angle = fast_atan2f(glm_vec3_dot(edge, cross) / length, glm_vec3_dot(n1, n2));
    float error = angle - e->angle;
    if (error > NT_PI) error -= 2.0F * NT_PI;
    if (error < -NT_PI) error += 2.0F * NT_PI;
    glm_vec3_scale(n1, -length / a2_reg, grad[2]);
    glm_vec3_scale(n2, -length / b2_reg, grad[3]);
    float u = glm_vec3_dot(ac, edge) * inv_edge2, v = glm_vec3_dot(ad, edge) * inv_edge2;
    glm_vec3_scale(grad[2], -u, grad[1]);
    glm_vec3_muladds(grad[3], -v, grad[1]);
    glm_vec3_scale(grad[2], u - 1.0F, grad[0]);
    glm_vec3_muladds(grad[3], v - 1.0F, grad[0]);
    float weight = wa * glm_vec3_norm2(grad[0]) + wb * glm_vec3_norm2(grad[1]) +
                   wc * glm_vec3_norm2(grad[2]) + wd * glm_vec3_norm2(grad[3]);
    if (weight <= 0.0F) return;
    /* Grinspun Discrete Shells geometric compliance factor: (A1 + A2) / (3 * L^2). */
    float geom = fmaxf(0.15F, fminf(2.0F, (norm1 + norm2) * (inv_edge2 * (1.0F / 6.0F))));
    float alpha_e = alpha * geom;
    float dl = (-error - alpha_e * *lambda) / (weight + alpha_e);
    *lambda += dl;
    if (wa > 0.0F) glm_vec3_muladds(grad[0], dl * wa, s->x[e->a]);
    if (wb > 0.0F) glm_vec3_muladds(grad[1], dl * wb, s->x[e->b]);
    if (wc > 0.0F) glm_vec3_muladds(grad[2], dl * wc, s->x[e->c]);
    if (wd > 0.0F) glm_vec3_muladds(grad[3], dl * wd, s->x[e->d]);
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
    if (c && c->has_prev) {
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
    vec3 center, normal, u, v;
    float depth, u_scale, v_scale;
    int plane_count;
    vec4 planes[CLOTH_SUPPORT_PLANES];
} SupportPlane;

static bool prepare_support(const ClothSupport *face, SupportPlane *p) {
    if (!face) return true;
    if (!finite_point(face->center) || !finite_point(face->half_u) || !finite_point(face->half_v) ||
        !isfinite(face->depth) || face->depth < 0.0F) return false;
    glm_vec3_cross((float *)face->half_u, (float *)face->half_v, p->normal);
    float area = glm_vec3_norm(p->normal);
    if (!isfinite(area) || area <= 1e-12F) return false;
    glm_vec3_scale(p->normal, 1.0F / area, p->normal);
    /* The inverse actor transform can turn a rectangle into a parallelogram. */
    glm_vec3_cross((float *)face->half_v, p->normal, p->u);
    glm_vec3_cross(p->normal, (float *)face->half_u, p->v);
    glm_vec3_scale(p->u, 1.0F / area, p->u);
    glm_vec3_scale(p->v, 1.0F / area, p->v);
    p->depth = face->depth;
    p->u_scale = 1.0F / glm_vec3_norm(p->u);
    p->v_scale = 1.0F / glm_vec3_norm(p->v);
    glm_vec3_copy((float *)face->center, p->center);
    p->plane_count = face->plane_count;
    if (p->plane_count < 0 || p->plane_count > CLOTH_SUPPORT_PLANES ||
        (p->plane_count && (p->depth <= 0 || p->plane_count < 4))) return false;
    for (int i = 0; i < p->plane_count; ++i) {
        float length = glm_vec3_norm((float *)face->planes[i]);
        if (!isfinite(length) || length < 1e-8F || !isfinite(face->planes[i][3])) return false;
        glm_vec4_scale((float *)face->planes[i], 1.0F / length, p->planes[i]);
    }
    return finite_point(p->u) && finite_point(p->v);
}

static bool clip_interval(float a, float b, float low, float high, float *enter, float *leave) {
    float delta = b - a;
    if (fabsf(delta) < 1e-8F) return a >= low && a <= high;
    float t0 = (low - a) / delta, t1 = (high - a) / delta;
    *enter = fmaxf(*enter, fminf(t0, t1));
    *leave = fminf(*leave, fmaxf(t0, t1));
    return *leave - *enter > 1e-5F;
}

static float plane_distance(vec4 plane, vec3 offset) {
    return glm_vec3_dot(plane, offset) + plane[3];
}

static bool clip_hull(SupportPlane *p, vec3 from, vec3 to, float *enter, float *leave, int *entry_face) {
    for (int i = 0; i < p->plane_count; ++i) {
        float a = plane_distance(p->planes[i], from), b = plane_distance(p->planes[i], to);
        if (a > 0.0F && b > 0.0F) return false;
        if (a >= 0.0F && b < a) {
            float t = a / (a - b);
            if (t >= *enter) {
                *enter = t;
                if (entry_face) *entry_face = i;
            }
        } else if (b > 0.0F) {
            *leave = fminf(*leave, a / (a - b));
        }
        if (*enter + 1e-5F >= *leave) return false;
    }
    return true;
}

static bool support_blocks(SupportPlane *p, const float from[3], const float to[3]) {
    vec3 a, b;
    glm_vec3_sub((float *)from, p->center, a);
    glm_vec3_sub((float *)to, p->center, b);
    float enter = 0.0F, leave = 1.0F;
    if (p->plane_count) return clip_hull(p, a, b, &enter, &leave, NULL);
    return clip_interval(glm_vec3_dot(a, p->normal), glm_vec3_dot(b, p->normal),
                         -p->depth, 0.0F, &enter, &leave) &&
           clip_interval(glm_vec3_dot(a, p->u), glm_vec3_dot(b, p->u), -1.0F, 1.0F, &enter, &leave) &&
           clip_interval(glm_vec3_dot(a, p->v), glm_vec3_dot(b, p->v), -1.0F, 1.0F, &enter, &leave);
}

static int sheet_root(int parents[], int i) {
    while (parents[i] != i) {
        parents[i] = parents[parents[i]];
        i = parents[i];
    }
    return i;
}

typedef struct {
    int parents[CLOTH_VERTICES];
    bool below[CLOTH_VERTICES], hangs[CLOTH_VERTICES];
} DrapeWork;

static const float (*drape_anchors(const ClothMesh *mesh, const float anchors[][3], SupportPlane *p,
                                  float weight, float result[][3], DrapeWork *w))[3] {
    if (!p || weight == 0.0F || p->depth <= 0.0F || !mesh->tether) return anchors;
    int *parents = w->parents;
    bool *below = w->below, *hangs = w->hangs;
    bool any = false;
    for (int i = 0; i < mesh->vertices; ++i) {
        vec3 offset;
        glm_vec3_sub((float *)anchors[i], p->center, offset);
        parents[i] = i;
        below[i] = mesh->inv_area[i] > 0.0F && glm_vec3_dot(offset, p->normal) < -p->depth;
        any |= below[i];
    }
    if (!any) return anchors;
    memset(hangs, 0, (size_t)mesh->vertices * sizeof *hangs);
    for (int i = 0; i < mesh->edges; ++i)
        parents[sheet_root(parents, mesh->edge[i].a)] = sheet_root(parents, mesh->edge[i].b);
    for (int i = 0; i < mesh->vertices; ++i)
        if (below[i]) hangs[sheet_root(parents, i)] = true;

    memcpy(result, anchors, (size_t)mesh->vertices * sizeof result[0]);
    for (int i = 0; i < mesh->vertices; ++i) {
        int pin = mesh->tether[i].pin;
        if (pin < 0 || mesh->inv_area[i] == 0.0F || !hangs[sheet_root(parents, i)]) continue;
        /* When a solid separates the seam from the hanging panel, the animated leg is an unreachable guide. */
        vec3 offset, target;
        glm_vec3_sub((float *)mesh->rest[i], (float *)mesh->rest[pin], offset);
        glm_vec3_add(offset, (float *)anchors[pin], offset);
        glm_vec3_sub(offset, p->center, offset);
        float height = glm_vec3_dot(offset, p->normal);
        glm_vec3_sub((float *)anchors[i], p->center, target);
        float u = fabsf(glm_vec3_dot(target, p->u)), v = fabsf(glm_vec3_dot(target, p->v));
        float target_height = glm_vec3_dot(target, p->normal);
        float margin = fminf((1.0F - u) * p->u_scale, (1.0F - v) * p->v_scale);
        if (height >= 0.0F || target_height >= 0.0F || margin <= 0.0F || !support_blocks(p, anchors[pin], anchors[i])) continue;
        /* The animated panel selects its side of the rim. The seam supplies only the hanging length. */
        vec3 tangent, hanging;
        glm_vec3_copy(target, tangent);
        glm_vec3_muladds(p->normal, -target_height, tangent);
        glm_vec3_copy((float *)anchors[i], hanging);
        glm_vec3_muladds(p->normal, height - target_height, hanging);
        float radius = fmaxf(u, v);
        if (radius > 1e-6F) glm_vec3_muladds(tangent, 1.04F / radius - 1.0F, hanging);
        float blend = fminf(1.0F, fminf(margin, -target_height) / (0.25F * p->depth));
        blend = blend * blend * (3.0F - 2.0F * blend);
        glm_vec3_lerp((float *)anchors[i], hanging, weight * blend * fminf(1.0F, -height / p->depth), result[i]);
    }
    return (const float (*)[3])result;
}

static bool project_support_edge(ClothState *s, const ClothMesh *mesh, const ClothEdge *edge, SupportPlane *p,
                                 float thick) {
    float wa = mesh->inv_area[edge->a], wb = mesh->inv_area[edge->b];
    if (wa == 0.0F && wb == 0.0F) return false;
    vec3 a, b;
    glm_vec3_sub(s->x[edge->a], p->center, a);
    glm_vec3_sub(s->x[edge->b], p->center, b);
    float ha = glm_vec3_dot(a, p->normal), hb = glm_vec3_dot(b, p->normal);
    float enter = 0.0F, leave = 1.0F;
    if (!clip_interval(ha, hb, -p->depth, 0.0F, &enter, &leave)) return false;
    float ua = glm_vec3_dot(a, p->u), ub = glm_vec3_dot(b, p->u);
    if (!clip_interval(ua, ub, -1.0F, 1.0F, &enter, &leave)) return false;
    float va = glm_vec3_dot(a, p->v), vb = glm_vec3_dot(b, p->v);
    if (!clip_interval(va, vb, -1.0F, 1.0F, &enter, &leave)) return false;

    if (p->plane_count && !clip_hull(p, a, b, &enter, &leave, NULL)) return false;
    float t = (enter + leave) * 0.5F, u = ua + t * (ub - ua), v = va + t * (vb - va), h = ha + t * (hb - ha);
    float du = (1.0F - fabsf(u)) * p->u_scale, dv = (1.0F - fabsf(v)) * p->v_scale;
    float distance = fminf(du, dv);
    vec3 normal;
    if (du < dv) glm_vec3_scale(p->u, copysignf(p->u_scale, u), normal);
    else glm_vec3_scale(p->v, copysignf(p->v_scale, v), normal);
    /* Fabric that spans the entire thickness must pass around the rim. */
    if (fminf(ha, hb) > -p->depth && -h < distance) {
        distance = -h;
        glm_vec3_copy(p->normal, normal);
    } else if (fmaxf(ha, hb) < 0.0F && h + p->depth < distance) {
        distance = h + p->depth;
        glm_vec3_negate_to(p->normal, normal);
    }
    if (p->plane_count) {
        vec3 midpoint;
        glm_vec3_lerp(a, b, t, midpoint);
        distance = FLT_MAX;
        for (int f = 0; f < p->plane_count; ++f) {
            float alignment = glm_vec3_dot(p->planes[f], p->normal);
            if ((fminf(ha, hb) <= -p->depth && alignment > 0.999F) ||
                (fmaxf(ha, hb) >= 0.0F && alignment < -0.999F)) continue;
            float gap = -plane_distance(p->planes[f], midpoint);
            if (gap < distance) {
                distance = gap;
                glm_vec3_copy(p->planes[f], normal);
            }
        }
        if (distance == FLT_MAX) return false;
    }
    float t0 = 1.0F - t, weight = wa * t0 * t0 + wb * t * t;
    if (weight <= 1e-7F) return false;
    float correction = (distance + thick) / weight;
    glm_vec3_muladds(normal, correction * wa * t0, s->x[edge->a]);
    glm_vec3_muladds(normal, correction * wb * t, s->x[edge->b]);
    return true;
}

static bool project_support(ClothState *s, const ClothMesh *mesh, SupportPlane *p, float thick,
                            const float previous[][3], float anchor_shift_y, ContactAccum *acc) {
    if (!p) return false;
    bool hit = false;
    for (int i = 0; i < mesh->vertices; ++i) {
        if (mesh->inv_area[i] == 0.0F) continue;
        vec3 offset;
        glm_vec3_sub(s->x[i], p->center, offset);
        float height = glm_vec3_dot(offset, p->normal);
        float u = glm_vec3_dot(offset, p->u), v = glm_vec3_dot(offset, p->v);
        if (height >= thick || fabsf(u) > 1.0F + thick / p->u_scale ||
            fabsf(v) > 1.0F + thick / p->v_scale) continue;
        if (p->plane_count) {
            int face = -1;
            float nearest = -FLT_MAX;
            for (int f = 0; f < p->plane_count; ++f) {
                float d = plane_distance(p->planes[f], offset);
                if (d >= thick) { face = -1; break; }
                if (d > nearest) { face = f; nearest = d; }
            }
            /* A fast fall can end nearer the underside than the entry face. */
            if (previous && height < 0.0F) {
                vec3 old;
                glm_vec3_sub((float *)previous[i], p->center, old);
                old[1] -= anchor_shift_y;
                float enter = 0.0F, leave = 1.0F;
                int entry_face = -1;
                if (glm_vec3_dot(old, p->normal) >= 0.0F &&
                    clip_hull(p, old, offset, &enter, &leave, &entry_face) && entry_face >= 0) {
                    face = entry_face;
                    nearest = plane_distance(p->planes[face], offset);
                }
            }
            if (face < 0) continue;
            float shift = thick - nearest;
            glm_vec3_muladds(p->planes[face], shift, s->x[i]);
            if (acc) record_contact(acc, i, p->planes[face], NULL, 0.0F, 0.0F, shift);
            hit = true;
            continue;
        }
        if (fabsf(u) > 1.0F || fabsf(v) > 1.0F) continue;
        bool crossed_top = false;
        if (previous) {
            glm_vec3_sub((float *)previous[i], p->center, offset);
            offset[1] -= anchor_shift_y;
            float old_height = glm_vec3_dot(offset, p->normal);
            if (old_height >= -1e-6F) {
                float t = height < 0.0F && old_height > 0.0F ? old_height / (old_height - height) : 1.0F;
                float old_u = glm_vec3_dot(offset, p->u), old_v = glm_vec3_dot(offset, p->v);
                crossed_top = fabsf(old_u + t * (u - old_u)) <= 1.0F && fabsf(old_v + t * (v - old_v)) <= 1.0F;
            }
        }
        vec3 normal;
        glm_vec3_copy(p->normal, normal);
        float distance = -height;
        if (!crossed_top && height < 0.0F) {
            if (height < -p->depth) continue;
            /* Initial overlaps need the nearest exit; a long panel may already hang below the object. */
            if (p->depth + height < distance) {
                distance = p->depth + height;
                glm_vec3_negate(normal);
            }
            float side_u = (1.0F - fabsf(u)) * p->u_scale;
            float side_v = (1.0F - fabsf(v)) * p->v_scale;
            if (side_u < distance && side_u <= side_v) {
                distance = side_u;
                glm_vec3_scale(p->u, copysignf(p->u_scale, u), normal);
            } else if (side_v < distance) {
                distance = side_v;
                glm_vec3_scale(p->v, copysignf(p->v_scale, v), normal);
            }
        }
        float shift = distance + thick;
        glm_vec3_muladds(normal, shift, s->x[i]);
        if (acc) record_contact(acc, i, normal, NULL, 0.0F, 0.0F, shift);
        hit = true;
    }
    if (p->depth > 0.0F) {
        for (int i = 0; i < mesh->edges; ++i)
            if (project_support_edge(s, mesh, &mesh->edge[i], p, thick)) hit = true;
    }
    return hit;
}

typedef struct {
    float min[3], max[3];
    float u[3];
    float ra, rb, dr, r_min, r_max, r_max2, safe_edge2;
    float len, inv_len, cone_a, inv_cone_a, cos_slope, sin_slope;
    int idx;
    bool degenerate;
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
    CapsuleBox boxes[CLOTH_CAPSULES];
    float all_min[3], all_max[3];
    float min_safe_edge2;
    float min_face_thresh2;
    int count;
} CapsuleBroadphase;

typedef struct {
    float draped[CLOTH_VERTICES][3], x_prev[CLOTH_VERTICES][3], v_normals[CLOTH_VERTICES][3];
    uint8_t anim_ok[CLOTH_VERTICES];
    ContactAccum acc;
    CapsuleBroadphase bp;
} StepWork;

typedef struct {
    int deg[CLOTH_VERTICES], off[CLOTH_VERTICES + 1], nb[2 * CLOTH_EDGES], cur[CLOTH_VERTICES];
    int hop[CLOTH_VERTICES], order[CLOTH_VERTICES];
    float disp[CLOTH_VERTICES][3];
    CapsuleBroadphase bp;
} ContactPoseWork;

typedef struct {
    float draped[CLOTH_VERTICES][3], saved[CLOTH_VERTICES][3];
    ContactPoseWork pose;
} ResetWork;

/* Solver arrays stay off the call stack, which is 64 KiB on the web. Public entry points never nest
   or call user code, so they share the union; drape runs inside a step or a reset and has its own. */
static _Thread_local struct {
    union {
        StepWork step;
        ResetWork reset;
    } entry;
    DrapeWork drape;
} work;

static void build_broadphase(const ClothMaterial *m, const ClothCapsule *body, int count, CapsuleBroadphase *bp) {
    bp->count = 0;
    bp->min_safe_edge2 = FLT_MAX;
    bp->min_face_thresh2 = FLT_MAX;
    for (int k = 0; k < 3; ++k) {
        bp->all_min[k] = FLT_MAX;
        bp->all_max[k] = -FLT_MAX;
    }
    float thickness = m->thickness;
    for (int j = 0; j < count; ++j) {
        float ra = body[j].radius + thickness;
        float rb = cloth_capsule_radius_b(body + j) + thickness;
        float r_max = fmaxf(ra, rb);
        CapsuleBox *b = &bp->boxes[bp->count++];
        b->idx = j;
        b->ra = ra;
        b->rb = rb;
        b->dr = ra - rb;
        b->r_min = fminf(ra, rb);
        b->r_max = r_max;
        b->r_max2 = r_max * r_max;
        /* Chord sagitta <= 0.5*thickness even at 14% strain when L_rest^2 <= 3*thickness*r_min. */
        b->safe_edge2 = 3.0F * thickness * b->r_min;
        if (b->safe_edge2 < bp->min_safe_edge2) bp->min_safe_edge2 = b->safe_edge2;
        float ft = 1.5F * b->r_min;
        float ft2 = ft * ft;
        if (ft2 < bp->min_face_thresh2) bp->min_face_thresh2 = ft2;
        vec3 ba;
        glm_vec3_sub((float *)body[j].b, (float *)body[j].a, ba);
        float len2 = glm_vec3_norm2(ba);
        b->degenerate = len2 <= 1e-12F || b->dr * b->dr >= len2;
        if (!b->degenerate) {
            b->len = sqrtf(len2);
            b->inv_len = 1.0F / b->len;
            glm_vec3_scale(ba, b->inv_len, b->u);
            b->cone_a = sqrtf(fmaxf(0.0F, len2 - b->dr * b->dr));
            b->inv_cone_a = b->cone_a > 1e-8F ? (1.0F / b->cone_a) : 0.0F;
            b->cos_slope = b->cone_a * b->inv_len;
            b->sin_slope = b->dr * b->inv_len;
        } else {
            b->len = 0.0F;
            b->inv_len = 0.0F;
            glm_vec3_zero(b->u);
            b->cone_a = 0.0F;
            b->inv_cone_a = 0.0F;
            b->cos_slope = 1.0F;
            b->sin_slope = 0.0F;
        }
        for (int k = 0; k < 3; ++k) {
            b->min[k] = fminf(body[j].a[k], body[j].b[k]) - r_max;
            b->max[k] = fmaxf(body[j].a[k], body[j].b[k]) + r_max;
            if (b->min[k] < bp->all_min[k]) bp->all_min[k] = b->min[k];
            if (b->max[k] > bp->all_max[k]) bp->all_max[k] = b->max[k];
        }
    }
}

static inline float box_sphere_contact(const float p[3], const float center[3], float radius, float normal[3]) {
    vec3 delta;
    glm_vec3_sub((float *)p, (float *)center, delta);
    float d2 = glm_vec3_norm2(delta);
    if (d2 >= radius * radius) return 0.0F;
    float d = sqrtf(d2);
    if (d > 1e-7F) glm_vec3_scale(delta, 1.0F / d, normal);
    else glm_vec3_copy((vec3){0.0F, 1.0F, 0.0F}, normal);
    return radius - d;
}

/* Precomputed round-cone SDF matching cloth_point_capsule_contact_swept with zero sqrt on misses. */
static float box_point_contact_swept(const float p[3], const float prev_p[3],
                                     const ClothCapsule *cap, const CapsuleBox *bx,
                                     float margin, const float fallback[3],
                                     float normal[3], float *t_out) {
    float depth = 0.0F, t = 0.0F;
    if (bx->degenerate) {
        if (bx->ra >= bx->rb) {
            t = 0.0F;
            depth = box_sphere_contact(p, cap->a, bx->ra, normal);
        } else {
            t = 1.0F;
            depth = box_sphere_contact(p, cap->b, bx->rb, normal);
        }
    } else {
        vec3 pa;
        glm_vec3_sub((float *)p, (float *)cap->a, pa);
        float y = glm_vec3_dot(pa, (float *)bx->u);
        if (y < -bx->ra || y > bx->len + bx->rb) return 0.0F;
        vec3 w = {pa[0] - y * bx->u[0], pa[1] - y * bx->u[1], pa[2] - y * bx->u[2]};
        float w2 = glm_vec3_norm2(w);
        if (w2 >= bx->r_max2) return 0.0F;
        float x = sqrtf(w2);
        float s = (-x * bx->dr + y * bx->cone_a) * bx->inv_len;
        if (s <= 0.0F) {
            t = 0.0F;
            depth = box_sphere_contact(p, cap->a, bx->ra, normal);
        } else if (s >= bx->cone_a) {
            t = 1.0F;
            depth = box_sphere_contact(p, cap->b, bx->rb, normal);
        } else {
            float signed_dist = x * bx->cos_slope + y * bx->sin_slope - bx->ra;
            if (signed_dist >= 0.0F) return 0.0F;
            t = glm_clamp(s * bx->inv_cone_a, 0.0F, 1.0F);
            vec3 w_hat;
            if (x > 1e-7F) {
                glm_vec3_scale(w, 1.0F / x, w_hat);
            } else {
                vec3 ref = {fabsf(bx->u[0]) > 0.9F ? 0.0F : 1.0F, fabsf(bx->u[0]) > 0.9F ? 1.0F : 0.0F, 0.0F};
                glm_vec3_cross((float *)bx->u, ref, w_hat);
                float wlen = glm_vec3_norm(w_hat);
                if (wlen > 1e-7F) glm_vec3_scale(w_hat, 1.0F / wlen, w_hat);
                else glm_vec3_copy((vec3){0.0F, 1.0F, 0.0F}, w_hat);
            }
            for (int k = 0; k < 3; ++k) normal[k] = bx->cos_slope * w_hat[k] + bx->sin_slope * bx->u[k];
            depth = -signed_dist;
        }
    }
    if (depth <= 0.0F) return 0.0F;
    if (cap->flags == CLOTH_COLLIDER_INSIDE && (cap->has_prev || fallback)) {
        return cloth_point_capsule_contact_swept(p, prev_p, cap, margin, fallback, normal, t_out);
    }
    *t_out = t;
    return depth;
}

static void project_body(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m,
                         const float x_prev[][3], const ClothGuide *guide,
                         const ClothCapsule *body, const CapsuleBroadphase *bp,
                         const ClothFrame *frame, float inv_dt, bool full_contact, ContactAccum *acc) {
    float thickness = m->thickness;
    if (frame) {
        float y_min = thickness;
        const float floor_n[3] = {0.0F, 1.0F, 0.0F};
        for (int i = 0; i < mesh->vertices; ++i) {
            if (mesh->inv_area[i] == 0.0F) continue;
            if (s->x[i][1] < y_min) {
                float depth = y_min - s->x[i][1];
                s->x[i][1] = y_min;
                if (acc) record_contact(acc, i, floor_n, NULL, 0.0F, inv_dt, depth);
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
            float depth = box_point_contact_swept(s->x[i], x_prev[i], cap, bx, thickness,
                                                  fallback, normal, &t);
            if (depth <= 0.0F) continue;
            glm_vec3_muladds(normal, depth, s->x[i]);
            if (acc) record_contact(acc, i, normal, cap, t, inv_dt, depth);
        }
    }
    if (!full_contact) return;

    /* 2. Exact round-cone SDF on edge midpoints (skip chords whose sagitta stays within thickness). */
    const float min_safe_edge2 = bp->min_safe_edge2;
    for (int e = 0; e < mesh->edges; ++e) {
        const ClothEdge *edge = &mesh->edge[e];
        float wa = mesh->inv_area[edge->a], wb = mesh->inv_area[edge->b];
        float elen2 = edge->length * edge->length;
        if (wa + wb <= 0.0F || elen2 <= min_safe_edge2) continue;
        vec3 mid;
        for (int k = 0; k < 3; ++k) mid[k] = 0.5F * (s->x[edge->a][k] + s->x[edge->b][k]);
        if (!point_in_box(mid, all_min, all_max)) continue;
        vec3 prev_mid, fb_avg;
        const float *fallback = NULL;
        bool lazy_init = false;
        for (int bi = 0; bi < active_count; ++bi) {
            const CapsuleBox *bx = &boxes[bi];
            if (elen2 <= bx->safe_edge2 || !point_in_box(mid, bx->min, bx->max)) continue;
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
            float depth = box_point_contact_swept(mid, prev_mid, cap, bx, thickness,
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

    /* 3. Face-interior fallback only when a triangle is large relative to a capsule radius. */
    const float min_face_thresh2 = bp->min_face_thresh2;
    for (int f = 0; f < mesh->faces; ++f) {
        const ClothFace face = mesh->face[f];
        float e0 = glm_vec3_distance2((float *)mesh->rest[face.a], (float *)mesh->rest[face.b]);
        float e1 = glm_vec3_distance2((float *)mesh->rest[face.b], (float *)mesh->rest[face.c]);
        float e2 = glm_vec3_distance2((float *)mesh->rest[face.c], (float *)mesh->rest[face.a]);
        float max_e2 = fmaxf(e0, fmaxf(e1, e2));
        if (max_e2 <= min_face_thresh2) continue;
        const float *pa = s->x[face.a], *pb = s->x[face.b], *pc = s->x[face.c];
        float fmin[3], fmax[3];
        for (int k = 0; k < 3; ++k) {
            fmin[k] = fminf(pa[k], fminf(pb[k], pc[k]));
            fmax[k] = fmaxf(pa[k], fmaxf(pb[k], pc[k]));
        }
        if (!box_overlaps(fmin, fmax, all_min, all_max)) continue;
        float min_e2 = fminf(e0, fminf(e1, e2));
        int ids[] = {face.a, face.b, face.c};
        for (int bi = 0; bi < active_count; ++bi) {
            const CapsuleBox *bx = &boxes[bi];
            float thresh = 1.5F * bx->r_min;
            if (max_e2 <= thresh * thresh || !box_overlaps(fmin, fmax, bx->min, bx->max)) continue;
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
            float impulse = fminf(depth / weight, 0.25F * sqrtf(min_e2) / largest);
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
    float inward = -glm_vec3_dot(offset, (float *)g->normal) - g->backstop;
    if (inward > 0.0F) glm_vec3_muladds((float *)g->normal, inward, offset);
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

static bool limit_strain_pass(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m,
                              const ContactAccum *acc, bool reverse) {
    float warp_ratio = 1.0F + m->max_edge_strain;
    float weft_ratio = 1.0F + (m->weft_max_strain > 0.0F ? m->weft_max_strain : m->max_edge_strain);
    bool moved = false;
    for (int k = 0; k < mesh->edges; ++k) {
        const ClothEdge *e = &mesh->edge[reverse ? mesh->edges - 1 - k : k];
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
    return moved;
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

void cloth_step(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                const ClothFrame *frame, const float anchors[][3], const ClothGuide *guide,
                const ClothCapsule *body, int count) {
    if (!s) return;
    SupportPlane plane;
    SupportPlane *support = frame && frame->support ? &plane : NULL;
    s->frame_valid = s->mesh == mesh && frame_motion_valid(frame) && prepare_support(frame ? frame->support : NULL, &plane) &&
                     cloth_frame_valid(mesh, m, dt, anchors, guide, body, count);
    if (!s->frame_valid) return;

    StepWork *w = &work.entry.step;
    anchors = drape_anchors(mesh, anchors, support, m->support_drape, w->draped, &work.drape);
    float (*x_prev)[3] = w->x_prev;
    float (*v_normals)[3] = w->v_normals;
    uint8_t *anim_ok = w->anim_ok;
    ContactAccum *acc = &w->acc;
    size_t v_count = (size_t)mesh->vertices;
    memset(acc->normal, 0, v_count * sizeof acc->normal[0]);
    memset(acc->vel, 0, v_count * sizeof acc->vel[0]);
    memset(acc->depth, 0, v_count * sizeof acc->depth[0]);
    memset(acc->count, 0, v_count * sizeof acc->count[0]);

    CapsuleBroadphase *bp = &w->bp;
    build_broadphase(m, body, count, bp);

    float inv_dt = 1.0F / dt;
    float retain = expf(-m->drag * dt);
    float aero_damp = m->aero_drag > 0.0F ? (1.0F - expf(-m->aero_drag * dt)) : 0.0F;
    const float max_fict = 28.0F; /* m/s^2: a root-acceleration spike must not fling the cloth */

    float anchor_shift_y = frame ? frame->anchor_shift_y : 0.0F;
    if (anchor_shift_y != 0.0F) {
        for (int i = 0; i < mesh->vertices; ++i)
            if (mesh->inv_area[i] > 0.0F) s->x[i][1] += anchor_shift_y;
    }
    if (aero_damp > 0.0F) compute_vertex_normals(mesh, s->x, v_normals);

    for (int i = 0; i < mesh->vertices; ++i) {
        glm_vec3_copy(s->x[i], x_prev[i]);
        if (mesh->inv_area[i] == 0.0F) {
            glm_vec3_copy((float *)anchors[i], s->x[i]);
            for (int k = 0; k < 3; ++k) s->velocity[i][k] = (anchors[i][k] - x_prev[i][k]) * inv_dt;
            continue;
        }
        vec3 accel = {0.0F, -9.81F, 0.0F};
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
            if (!anim_ok[i] || bp->count == 0 || !point_in_box(anchors[i], bp->all_min, bp->all_max)) continue;
            /* Do not drive cloth toward an anchor occluded inside a body collider. */
            for (int bi = 0; bi < bp->count; ++bi) {
                const CapsuleBox *bx = &bp->boxes[bi];
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
        project_body(s, mesh, m, x_prev, guide, body, bp, frame, inv_dt, pass == m->iterations - 1, acc);
        project_support(s, mesh, support, m->thickness, x_prev, anchor_shift_y, acc);
    }

    bool pending_contact = false;
    for (int pass = 0; pass < m->strain_passes; ++pass) {
        bool moved = limit_strain_pass(s, mesh, m, acc, pass % 2 != 0);
        if (!moved && !pending_contact) break;
        pending_contact = bp->count || support || frame;
        if (pending_contact && (pass % 2 != 0 || pass + 1 == m->strain_passes)) {
            /* A length projection can enter a collider that was clear on the preceding sweep. */
            project_body(s, mesh, m, x_prev, guide, body, bp, frame, inv_dt, true, acc);
            project_support(s, mesh, support, m->thickness, x_prev, anchor_shift_y, acc);
            pending_contact = false;
        }
    }

    for (int i = 0; i < mesh->vertices; ++i) {
        if (mesh->inv_area[i] == 0.0F) continue;
        vec3 v_free, v;
        glm_vec3_copy(s->velocity[i], v_free);
        for (int k = 0; k < 3; ++k) v[k] = (s->x[i][k] - x_prev[i][k]) * inv_dt;
        if (acc->count[i] > 0) {
            vec3 cn, v_body;
            glm_vec3_copy(acc->normal[i], cn);
            float nlen = glm_vec3_norm(cn);
            float inv_n = 1.0F / (float)acc->count[i];
            for (int k = 0; k < 3; ++k) v_body[k] = acc->vel[i][k] * inv_n;
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
                    float dv_fric = m->contact_friction * fminf(acc->depth[i] * inv_dt, 2.0F);
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
        if (!finite_point(s->x[i]) || !finite_point(v)) s->frame_valid = false;
    }
}

static float reset_support_weight(const float point[3], const ClothMaterial *m, const ClothCapsule *body,
                                  int count, float range) {
    float reach = 0.0F;
    for (int i = 0; i < count; ++i) {
        if (body[i].flags != CLOTH_COLLIDER_INSIDE) continue;
        vec3 normal;
        float depth = cloth_point_capsule_contact(point, &body[i], m->thickness + range, normal, NULL);
        reach = fmaxf(reach, depth);
    }
    return fminf(1.0F, reach / range);
}

static void reset_contact_pose(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m,
                               const float anchors[][3], const ClothGuide *guide,
                               const ClothCapsule *body, int count, float carry_range, SupportPlane *support,
                               ContactPoseWork *w) {
    int n = mesh->vertices;
    int *deg = w->deg, *off = w->off, *nb = w->nb, *cur = w->cur, *hop = w->hop, *order = w->order;
    memset(deg, 0, (size_t)n * sizeof *deg);
    for (int e = 0; e < mesh->edges; ++e) {
        ++deg[mesh->edge[e].a];
        ++deg[mesh->edge[e].b];
    }
    off[0] = 0;
    for (int i = 0; i < n; ++i) off[i + 1] = off[i] + deg[i];
    memcpy(cur, off, (size_t)n * sizeof cur[0]);
    for (int e = 0; e < mesh->edges; ++e) {
        int a = mesh->edge[e].a, b = mesh->edge[e].b;
        nb[cur[a]++] = b;
        nb[cur[b]++] = a;
    }

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

    float (*disp)[3] = w->disp;
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
            if (carry_range > 0.0F && glm_vec3_norm2(avg_disp) > 0.0F) {
                float weight = reset_support_weight(anchors[v], m, body, count, carry_range);
                glm_vec3_muladds(avg_disp, weight, s->x[v]);
            } else {
                glm_vec3_add(s->x[v], avg_disp, s->x[v]);
            }
        }
        const float *gn = guide ? guide[v].normal : NULL;
        vec3 cn_sum = {0.0F, 0.0F, 0.0F};
        bool hit = false;
        for (int pass = 0; pass < 2; ++pass) {
            for (int c = 0; c < count; ++c) {
                if (body[c].flags != CLOTH_COLLIDER_INSIDE) continue;
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
                        if (body[c].flags != CLOTH_COLLIDER_INSIDE) continue;
                        vec3 cn2;
                        depenetrate_reset_vertex(s->x[v], gn, true, parent_avg, body + c, m->thickness, cn2);
                    }
                }
            }
        }
        glm_vec3_sub(s->x[v], (float *)anchors[v], disp[v]);
    }

    build_broadphase(m, body, count, &w->bp);
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
        project_body(s, mesh, m, s->x, guide, body, &w->bp, NULL, 1.0F, true, NULL);
        project_support(s, mesh, support, m->thickness, NULL, 0.0F, NULL);
    }
}

bool cloth_reset_supported(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m,
                           const float anchors[][3], const ClothGuide *guide,
                           const ClothCapsule *body, int count, const ClothSupport *face) {
    const float evaluation_dt = 1.0F / 60.0F;
    if (!s) return false;
    SupportPlane plane;
    SupportPlane *support = face ? &plane : NULL;
    if (!prepare_support(face, &plane) || !cloth_frame_valid(mesh, m, evaluation_dt, anchors, guide, body, count)) {
        s->mesh = NULL;
        s->frame_valid = false;
        return false;
    }
    if (!cloth_mesh_valid(mesh)) {
        s->mesh = NULL;
        s->frame_valid = false;
        return false;
    }
    ResetWork *w = &work.entry.reset;
    anchors = drape_anchors(mesh, anchors, support, m->support_drape, w->draped, &work.drape);
    cloth_reset(s, mesh, anchors);
    if (!s->frame_valid) return false;
    if (!count && !project_support(s, mesh, support, m->thickness, NULL, 0.0F, NULL)) return true;
    reset_contact_pose(s, mesh, m, anchors, guide, body, count, 0.0F, support, &w->pose);
    if (!count || m->reset_carry_range_m == 0.0F) return true;

    /* A deeply folded sheet can need support to propagate beyond the nearest collider. */
    ClothMetrics propagated = cloth_measure(s, mesh, m, evaluation_dt, anchors, body, count);
    if (propagated.finite && propagated.stretch == 0.0F) return true;
    size_t bytes = (size_t)mesh->vertices * sizeof s->x[0];
    memcpy(w->saved, s->x, bytes);
    memcpy(s->x, anchors, bytes);
    reset_contact_pose(s, mesh, m, anchors, guide, body, count, m->reset_carry_range_m, support, &w->pose);
    ClothMetrics local = cloth_measure(s, mesh, m, evaluation_dt, anchors, body, count);
    if (!local.finite || (propagated.finite &&
        (local.stretch >= propagated.stretch || local.penetration > propagated.penetration + 1e-6F)))
        memcpy(s->x, w->saved, bytes);
    return true;
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
            vec3 weights, normal;
            out.penetration = fmaxf(out.penetration, cloth_face_contact(s->x[face.a], s->x[face.b], s->x[face.c],
                                                                        body + i, m->thickness, weights, normal));
        }
    }
    out.finite &= isfinite(out.speed) && isfinite(out.stretch) && isfinite(out.penetration) && isfinite(out.pin_error);
    return out;
}
