#include "garment/cloth.h"
#include "garment/contact.h"
#include "core/nt_assert.h"
#include "math/nt_math.h"

#include <float.h>
#include <math.h>
#include <string.h>

static bool finite_point(const float p[3]) {
    return isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2]);
}

static bool frame_motion_valid(const ClothFrame *frame) {
    if (!frame) return true;
    return finite_point(frame->linear_accel) && isfinite(frame->angular_vel_y) && isfinite(frame->angular_accel_y) &&
           isfinite(frame->linear_inertia) && frame->linear_inertia >= 0.0F && frame->linear_inertia <= 1.0F &&
           isfinite(frame->angular_inertia) && frame->angular_inertia >= 0.0F && frame->angular_inertia <= 1.0F;
}

static bool shapes_valid(const ClothShapes *sh, float thickness) {
    if (!sh) return true;
    if (sh->sphere_count < 0 || sh->sphere_count > CLOTH_SHAPES || (sh->sphere_count && !sh->spheres) ||
        sh->cone_count < 0 || sh->cone_count > CLOTH_SHAPES || (sh->cone_count && !sh->cones) ||
        sh->convex_count < 0 || sh->convex_count > CLOTH_CONVEXES || (sh->convex_count && !sh->convexes)) return false;
    for (int i = 0; i < sh->sphere_count; ++i) {
        const ClothSphere *s = &sh->spheres[i];
        if (!finite_point(s->c) || !finite_point(s->prev) || !isfinite(s->r) || s->r < 0 || !isfinite(s->r + thickness))
            return false;
    }
    for (int i = 0; i < sh->cone_count; ++i)
        if (sh->cones[i].a >= sh->sphere_count || sh->cones[i].b >= sh->sphere_count) return false;
    for (int i = 0; i < sh->convex_count; ++i) {
        const ClothConvex *c = &sh->convexes[i];
        if (c->plane_count < 1 || c->plane_count > CLOTH_CONVEX_PLANES) return false;
        for (int f = 0; f < c->plane_count; ++f) {
            float length = glm_vec3_norm((float *)c->planes[f]);
            if (!isfinite(length) || length < 1e-8F || !isfinite(c->planes[f][3])) return false;
        }
    }
    if (sh->convex_move) {
        for (int r = 0; r < 3; ++r)
            for (int k = 0; k < 4; ++k)
                if (!isfinite(sh->convex_move[r][k])) return false;
    }
    return true;
}

static bool inputs_valid(const ClothMesh *mesh, const ClothMaterial *m, const ClothReference *ref,
                         const float *max_distance, const ClothShapes *shapes) {
    NT_ASSERT(mesh && mesh->vertices >= 3 && mesh->vertices <= CLOTH_VERTICES && m);
    if (!ref) return false;
    const float *r = max_distance ? max_distance : mesh->max_distance;
    for (int i = 0; i < mesh->vertices; ++i) {
        float length2 = glm_vec3_norm2((float *)ref[i].b);
        if (!finite_point(ref[i].x) || !isfinite(length2) || fabsf(length2 - 1.0F) > 0.002F || !isfinite(r[i]) ||
            r[i] < 0.0F) return false;
    }
    return shapes_valid(shapes, m->thickness);
}

/* Mesh and material are cooked and trusted; only the per-frame inputs are checked. */
bool cloth_frame_valid(const ClothMesh *mesh, const ClothMaterial *m, float dt, const ClothReference *ref,
                       const float *max_distance, const ClothShapes *shapes) {
    NT_ASSERT(mesh && m);
    if (!isfinite(dt) || dt <= 0) return false;
    float scale = m->density / (dt * dt);
    if (!isfinite(scale) || !isfinite(m->stretch * scale) || !isfinite(m->bend * scale)) return false;
    return inputs_valid(mesh, m, ref, max_distance, shapes);
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
    float min[3], max[3];
    float u[3];
    float ra, rb, dr, r_min, r_max, r_max2, face_edge2;
    float len, inv_len, cone_a, inv_cone_a, cos_slope, sin_slope;
    const ClothSphere *a, *b;
    bool degenerate;
} ShapeBox;

typedef struct {
    ShapeBox boxes[2 * CLOTH_SHAPES];
    float all_min[3], all_max[3];
    float min_face_edge2;
    int count;
} ShapeBroadphase;

typedef struct {
    int plane_count;
    vec4 planes[CLOTH_CONVEX_PLANES];
} ConvexPlanes;

typedef struct {
    float x_prev[CLOTH_VERTICES][3], v_normals[CLOTH_VERTICES][3];
    float normal[CLOTH_VERTICES][3]; /* Sum of this substep's contact normals, for the strain passes. */
    float w[CLOTH_VERTICES];
    ShapeBroadphase bp;
    ConvexPlanes convex[CLOTH_CONVEXES];
    int convex_count;
} StepWork;

/* Solver arrays stay off the call stack, which is 64 KiB on the web. Public entry points never nest or call user
   code, so they share this. */
static _Thread_local StepWork work;

static inline bool point_in_box(const float p[3], const float bmin[3], const float bmax[3]) {
    return p[0] >= bmin[0] && p[0] <= bmax[0] && p[1] >= bmin[1] && p[1] <= bmax[1] && p[2] >= bmin[2] && p[2] <= bmax[2];
}

static inline bool box_overlaps(const float amin[3], const float amax[3], const float bmin[3], const float bmax[3]) {
    return amin[0] <= bmax[0] && amax[0] >= bmin[0] && amin[1] <= bmax[1] && amax[1] >= bmin[1] &&
           amin[2] <= bmax[2] && amax[2] >= bmin[2];
}

static void add_box(ShapeBroadphase *bp, const ClothSphere *sa, const ClothSphere *sb, float thickness) {
    ShapeBox *b = &bp->boxes[bp->count++];
    b->a = sa;
    b->b = sb;
    b->ra = sa->r + thickness;
    b->rb = sb->r + thickness;
    b->dr = b->ra - b->rb;
    b->r_min = fminf(b->ra, b->rb);
    b->r_max = fmaxf(b->ra, b->rb);
    b->r_max2 = b->r_max * b->r_max;
    /* A chord of length l over radius r sags l^2 / 8r; above the thickness a vertex test misses the face. */
    b->face_edge2 = 8.0F * b->r_min * thickness;
    bp->min_face_edge2 = fminf(bp->min_face_edge2, b->face_edge2);
    vec3 ba;
    glm_vec3_sub((float *)sb->c, (float *)sa->c, ba);
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
        b->len = b->inv_len = b->cone_a = b->inv_cone_a = b->sin_slope = 0.0F;
        b->cos_slope = 1.0F;
        glm_vec3_zero(b->u);
    }
    for (int k = 0; k < 3; ++k) {
        b->min[k] = fminf(sa->c[k], sb->c[k]) - b->r_max;
        b->max[k] = fmaxf(sa->c[k], sb->c[k]) + b->r_max;
        bp->all_min[k] = fminf(bp->all_min[k], b->min[k]);
        bp->all_max[k] = fmaxf(bp->all_max[k], b->max[k]);
    }
}

static void build_broadphase(const ClothShapes *sh, float thickness, ShapeBroadphase *bp) {
    bp->count = 0;
    bp->min_face_edge2 = FLT_MAX;
    for (int k = 0; k < 3; ++k) {
        bp->all_min[k] = FLT_MAX;
        bp->all_max[k] = -FLT_MAX;
    }
    if (!sh) return;
    bool in_cone[CLOTH_SHAPES] = {0};
    for (int i = 0; i < sh->cone_count; ++i) {
        const ClothCone c = sh->cones[i];
        in_cone[c.a] = in_cone[c.b] = true;
        add_box(bp, &sh->spheres[c.a], &sh->spheres[c.b], thickness);
    }
    for (int i = 0; i < sh->sphere_count; ++i)
        if (!in_cone[i]) add_box(bp, &sh->spheres[i], &sh->spheres[i], thickness);
}

static void prepare_convexes(const ClothShapes *sh, StepWork *w) {
    w->convex_count = sh ? sh->convex_count : 0;
    for (int i = 0; i < w->convex_count; ++i) {
        const ClothConvex *c = &sh->convexes[i];
        w->convex[i].plane_count = c->plane_count;
        for (int f = 0; f < c->plane_count; ++f)
            glm_vec4_scale((float *)c->planes[f], 1.0F / glm_vec3_norm((float *)c->planes[f]), w->convex[i].planes[f]);
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

/* The round-cone SDF of cloth_point_cone_contact with precomputed terms and no sqrt on a miss. */
static float box_point_contact(const float p[3], const ShapeBox *bx, float normal[3], float *t_out) {
    float depth = 0.0F, t = 0.0F;
    if (bx->degenerate) {
        if (bx->ra >= bx->rb) {
            depth = box_sphere_contact(p, bx->a->c, bx->ra, normal);
        } else {
            t = 1.0F;
            depth = box_sphere_contact(p, bx->b->c, bx->rb, normal);
        }
    } else {
        vec3 pa;
        glm_vec3_sub((float *)p, (float *)bx->a->c, pa);
        float y = glm_vec3_dot(pa, (float *)bx->u);
        if (y < -bx->ra || y > bx->len + bx->rb) return 0.0F;
        vec3 w = {pa[0] - y * bx->u[0], pa[1] - y * bx->u[1], pa[2] - y * bx->u[2]};
        float w2 = glm_vec3_norm2(w);
        if (w2 >= bx->r_max2) return 0.0F;
        float x = sqrtf(w2);
        float s = (-x * bx->dr + y * bx->cone_a) * bx->inv_len;
        if (s <= 0.0F) {
            depth = box_sphere_contact(p, bx->a->c, bx->ra, normal);
        } else if (s >= bx->cone_a) {
            t = 1.0F;
            depth = box_sphere_contact(p, bx->b->c, bx->rb, normal);
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
    *t_out = t;
    return depth > 0.0F ? depth : 0.0F;
}

static void shape_motion(const ShapeBox *bx, float t, float out[3]) {
    for (int k = 0; k < 3; ++k) {
        float ma = bx->a->c[k] - bx->a->prev[k], mb = bx->b->c[k] - bx->b->prev[k];
        out[k] = ma + t * (mb - ma);
    }
}

/* Push x out along n by depth, then Coulomb friction on the substep's displacement relative to the shape point
   (Macklin et al. 2014, 6.1): static below mu * depth, kinetic above it. */
static void resolve_contact(float x[3], const float x_prev[3], const float n[3], float depth, const float motion[3],
                            float mu) {
    glm_vec3_muladds((float *)n, depth, x);
    if (mu <= 0.0F) return;
    vec3 rel;
    for (int k = 0; k < 3; ++k) rel[k] = x[k] - x_prev[k] - motion[k];
    glm_vec3_muladds((float *)n, -glm_vec3_dot(rel, (float *)n), rel);
    float slip = glm_vec3_norm(rel), limit = mu * depth;
    if (slip <= 1e-12F) return;
    glm_vec3_muladds(rel, slip < limit ? -1.0F : -limit / slip, x);
}

static void project_shapes(ClothState *s, const ClothMesh *mesh, const float *w, const float x_prev[][3],
                           const ShapeBroadphase *bp, float thickness, float mu, bool faces, float (*normals)[3]) {
    if (bp->count <= 0) return;
    /* Discrete projection to the nearest exit: the reference constraints, not a remembered side, say where the
       cloth belongs. */
    for (int i = 0; i < mesh->vertices; ++i) {
        if (w[i] == 0.0F || !point_in_box(s->x[i], bp->all_min, bp->all_max)) continue;
        for (int bi = 0; bi < bp->count; ++bi) {
            const ShapeBox *bx = &bp->boxes[bi];
            if (!point_in_box(s->x[i], bx->min, bx->max)) continue;
            vec3 normal, motion;
            float t = 0.0F;
            float depth = box_point_contact(s->x[i], bx, normal, &t);
            if (depth <= 0.0F) continue;
            shape_motion(bx, t, motion);
            resolve_contact(s->x[i], x_prev[i], normal, depth, motion, mu);
            if (normals) glm_vec3_add(normals[i], normal, normals[i]);
        }
    }
    if (!faces || mesh->faces <= 0) return;
    for (int f = 0; f < mesh->faces; ++f) {
        const ClothFace face = mesh->face[f];
        float e0 = glm_vec3_distance2((float *)mesh->rest[face.a], (float *)mesh->rest[face.b]);
        float e1 = glm_vec3_distance2((float *)mesh->rest[face.b], (float *)mesh->rest[face.c]);
        float e2 = glm_vec3_distance2((float *)mesh->rest[face.c], (float *)mesh->rest[face.a]);
        float max_e2 = fmaxf(e0, fmaxf(e1, e2));
        if (max_e2 <= bp->min_face_edge2) continue;
        const float *pa = s->x[face.a], *pb = s->x[face.b], *pc = s->x[face.c];
        float fmin[3], fmax[3];
        for (int k = 0; k < 3; ++k) {
            fmin[k] = fminf(pa[k], fminf(pb[k], pc[k]));
            fmax[k] = fmaxf(pa[k], fmaxf(pb[k], pc[k]));
        }
        if (!box_overlaps(fmin, fmax, bp->all_min, bp->all_max)) continue;
        float min_e2 = fminf(e0, fminf(e1, e2));
        const int ids[] = {face.a, face.b, face.c};
        for (int bi = 0; bi < bp->count; ++bi) {
            const ShapeBox *bx = &bp->boxes[bi];
            if (max_e2 <= bx->face_edge2 || !box_overlaps(fmin, fmax, bx->min, bx->max)) continue;
            vec3 bary, normal;
            float depth = cloth_face_contact(pa, pb, pc, bx->a, bx->b, thickness, bary, normal);
            if (depth <= 0.0F) continue;
            float weight = 0.0F, largest = 0.0F;
            for (int i = 0; i < 3; ++i) {
                float bw = bary[i] * w[ids[i]];
                weight += bary[i] * bw;
                largest = fmaxf(largest, bw);
            }
            if (weight < 1e-8F || largest < 1e-8F) continue;
            /* Clamp against the shortest face edge so an irregular triangle cannot spike strain. */
            float impulse = fminf(depth / weight, 0.25F * sqrtf(min_e2) / largest);
            for (int i = 0; i < 3; ++i) {
                if (w[ids[i]] <= 0.0F) continue;
                glm_vec3_muladds(normal, impulse * bary[i] * w[ids[i]], s->x[ids[i]]);
                if (normals) glm_vec3_add(normals[ids[i]], normal, normals[ids[i]]);
            }
        }
    }
}

static void move_point(const float (*move)[4], const float p[3], float out[3]) {
    if (!move) {
        glm_vec3_copy((float *)p, out);
        return;
    }
    for (int r = 0; r < 3; ++r) out[r] = move[r][0] * p[0] + move[r][1] * p[1] + move[r][2] * p[2] + move[r][3];
}

/* A vertex inside a convex leaves through the face it entered by, judged from its previous position carried with the
   convex; a vertex already inside at the substep start takes the nearest face. */
static void project_convexes(ClothState *s, const ClothMesh *mesh, const float *w, const float x_prev[][3],
                             const StepWork *wk, const float (*move)[4], float thickness, float mu,
                             float (*normals)[3]) {
    for (int c = 0; c < wk->convex_count; ++c) {
        const ConvexPlanes *cv = &wk->convex[c];
        for (int i = 0; i < mesh->vertices; ++i) {
            if (w[i] == 0.0F) continue;
            int nearest = -1;
            float nearest_d = -FLT_MAX;
            bool inside = true;
            for (int f = 0; f < cv->plane_count && inside; ++f) {
                float d = glm_vec3_dot((float *)cv->planes[f], s->x[i]) + cv->planes[f][3];
                inside = d < thickness;
                if (d > nearest_d) {
                    nearest_d = d;
                    nearest = f;
                }
            }
            if (!inside) continue;
            vec3 from;
            move_point(move, x_prev[i], from);
            int entry = -1;
            float enter = -1.0F;
            for (int f = 0; f < cv->plane_count; ++f) {
                float a = glm_vec3_dot((float *)cv->planes[f], from) + cv->planes[f][3] - thickness;
                float b = glm_vec3_dot((float *)cv->planes[f], s->x[i]) + cv->planes[f][3] - thickness;
                if (a < 0.0F) continue;
                float t = a / (a - b);
                if (t > enter) {
                    enter = t;
                    entry = f;
                }
            }
            int face = entry >= 0 ? entry : nearest;
            float depth = thickness - (glm_vec3_dot((float *)cv->planes[face], s->x[i]) + cv->planes[face][3]);
            vec3 motion;
            glm_vec3_sub(from, (float *)x_prev[i], motion);
            resolve_contact(s->x[i], x_prev[i], cv->planes[face], depth, motion, mu);
            if (normals) glm_vec3_add(normals[i], (float *)cv->planes[face], normals[i]);
        }
    }
}

static bool project_max_distance(ClothState *s, const ClothMesh *mesh, const float *w, const ClothReference *ref,
                                 const float *r) {
    bool moved = false;
    for (int i = 0; i < mesh->vertices; ++i) {
        if (w[i] == 0.0F) continue;
        vec3 offset;
        glm_vec3_sub(s->x[i], (float *)ref[i].x, offset);
        float distance2 = glm_vec3_norm2(offset);
        if (distance2 <= r[i] * r[i]) continue;
        glm_vec3_muladds(offset, r[i] / sqrtf(distance2) - 1.0F, s->x[i]);
        moved = true;
    }
    return moved;
}

/* How deep x lies inside its backstop, and the push back to its surface. An infinite radius is the half-space
   behind the plane d along b, which is what a flat or concave body gives. */
static float backstop_depth(const float x[3], const ClothReference *ref, ClothBackstop bs, float push[3]) {
    if (bs.R <= 0.0F) return 0.0F;
    vec3 offset;
    glm_vec3_sub((float *)x, (float *)ref->x, offset);
    if (isinf(bs.R)) {
        float depth = glm_vec3_dot(offset, (float *)ref->b) - bs.d;
        glm_vec3_scale((float *)ref->b, -depth, push);
        return depth;
    }
    glm_vec3_muladds((float *)ref->b, -(bs.d + bs.R), offset); /* now from the centre */
    float distance = glm_vec3_norm(offset);
    float depth = bs.R - distance;
    if (depth <= 0.0F) return depth;
    if (distance > 1e-7F) glm_vec3_scale(offset, depth / distance, push);
    else glm_vec3_scale((float *)ref->b, -depth, push);
    return depth;
}

static void project_backstop(ClothState *s, const ClothMesh *mesh, const float *w, const ClothReference *ref) {
    for (int i = 0; i < mesh->vertices; ++i) {
        vec3 push;
        if (w[i] != 0.0F && backstop_depth(s->x[i], &ref[i], mesh->backstop[i], push) > 0.0F)
            glm_vec3_add(s->x[i], push, s->x[i]);
    }
}

static void project_tethers(ClothState *s, const ClothMesh *mesh, const float *w) {
    for (int i = 0; i < mesh->vertices; ++i) {
        ClothTether t = mesh->tether[i];
        if (t.pin < 0 || w[i] == 0.0F) continue;
        vec3 offset;
        glm_vec3_sub(s->x[i], s->x[t.pin], offset);
        float distance2 = glm_vec3_norm2(offset);
        if (distance2 > t.max_length * t.max_length) {
            glm_vec3_scale(offset, t.max_length / sqrtf(distance2), offset);
            glm_vec3_add(s->x[t.pin], offset, s->x[i]);
        }
    }
}

static void project_tangent_if_contact(float corr[3], const float normal_sum[3]) {
    vec3 n;
    glm_vec3_copy((float *)normal_sum, n);
    float nlen = glm_vec3_norm(n);
    if (nlen <= 1e-7F) return;
    glm_vec3_scale(n, 1.0F / nlen, n);
    float into = glm_vec3_dot(corr, n);
    if (into < 0.0F) glm_vec3_muladds(n, -into, corr);
}

static bool limit_strain_pass(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, const float *w,
                              const float (*normals)[3], bool reverse) {
    float warp_ratio = 1.0F + m->max_edge_strain;
    float weft_ratio = 1.0F + (m->weft_max_strain > 0.0F ? m->weft_max_strain : m->max_edge_strain);
    bool moved = false;
    for (int k = 0; k < mesh->edges; ++k) {
        const ClothEdge *e = &mesh->edge[reverse ? mesh->edges - 1 - k : k];
        vec3 delta;
        glm_vec3_sub(s->x[e->b], s->x[e->a], delta);
        float ratio = (e->kind == CLOTH_EDGE_WARP) ? warp_ratio : weft_ratio;
        float length = glm_vec3_norm(delta), target = ratio * e->length;
        float wa = w[e->a], wb = w[e->b];
        if (length <= target || wa + wb == 0.0F) continue;
        moved = true;
        float scale = (length - target) / (length * (wa + wb));
        if (wa > 0.0F) {
            vec3 corr;
            glm_vec3_scale(delta, scale * wa, corr);
            project_tangent_if_contact(corr, normals[e->a]);
            glm_vec3_add(s->x[e->a], corr, s->x[e->a]);
        }
        if (wb > 0.0F) {
            vec3 corr;
            glm_vec3_scale(delta, -scale * wb, corr);
            project_tangent_if_contact(corr, normals[e->b]);
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

static void kinematic_weights(const ClothMesh *mesh, const float *r, float *w) {
    for (int i = 0; i < mesh->vertices; ++i) w[i] = r[i] > 0.0F ? mesh->inv_area[i] : 0.0F;
}

/* Integrate -> [max distance -> tethers -> bend, stretch -> backstop -> collision] x iterations -> max distance ->
   strain passes -> backstop -> collision -> velocity from positions. Collision has the last word, since penetration
   is the visible defect and the backstop pushes the same way. The strain limit comes after max distance: where the
   constraints cannot all hold, the fabric keeps its limit and the excess beyond max distance, which the measures
   report, shows the defect instead of a torn triangle. */
void cloth_step(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt, const ClothFrame *frame,
                const ClothReference *ref, const float *max_distance, const ClothShapes *shapes) {
    if (!s) return;
    s->frame_valid = s->mesh == mesh && frame_motion_valid(frame) &&
                     cloth_frame_valid(mesh, m, dt, ref, max_distance, shapes);
    if (!s->frame_valid) return;
    const float *r = max_distance ? max_distance : mesh->max_distance;
    StepWork *wk = &work;
    float (*x_prev)[3] = wk->x_prev;
    float *w = wk->w;
    size_t v_count = (size_t)mesh->vertices;
    memset(wk->normal, 0, v_count * sizeof wk->normal[0]);
    kinematic_weights(mesh, r, w);
    build_broadphase(shapes, m->thickness, &wk->bp);
    prepare_convexes(shapes, wk);
    const float (*move)[4] = shapes ? shapes->convex_move : NULL;

    float inv_dt = 1.0F / dt;
    float retain = expf(-m->drag * dt);
    float aero_damp = m->aero_drag > 0.0F ? (1.0F - expf(-m->aero_drag * dt)) : 0.0F;
    const float max_fict = 28.0F; /* m/s^2: a root-acceleration spike must not fling the cloth */
    if (aero_damp > 0.0F) compute_vertex_normals(mesh, s->x, wk->v_normals);

    for (int i = 0; i < mesh->vertices; ++i) {
        glm_vec3_copy(s->x[i], x_prev[i]);
        if (w[i] == 0.0F) {
            glm_vec3_copy((float *)ref[i].x, s->x[i]);
            continue;
        }
        vec3 accel = {0.0F, -9.81F, 0.0F};
        if (frame) {
            vec3 fict = {-frame->linear_inertia * frame->linear_accel[0], -frame->linear_inertia * frame->linear_accel[1],
                         -frame->linear_inertia * frame->linear_accel[2]};
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
            float vn = glm_vec3_dot(s->velocity[i], wk->v_normals[i]);
            glm_vec3_muladds(wk->v_normals[i], -aero_damp * vn, s->velocity[i]);
        }
        glm_vec3_muladds(s->velocity[i], dt, s->x[i]);
    }

    /* Weights are inverse area; multiplying compliance by density cancels the same factor in mass. */
    float scale = m->density / (dt * dt);
    float weft_s = m->weft_stretch_scale > 0.0F ? m->weft_stretch_scale : 1.0F;
    float diag_s = m->diag_stretch_scale > 0.0F ? m->diag_stretch_scale : 1.0F;
    float weft_b = m->weft_bend_scale > 0.0F ? m->weft_bend_scale : 1.0F;
    memset(s->stretch_lambda, 0, (size_t)mesh->edges * sizeof(float));
    memset(s->bend_lambda, 0, (size_t)mesh->edges * sizeof(float));
    const float mu = m->friction;

    for (int pass = 0; pass < m->iterations; ++pass) {
        project_max_distance(s, mesh, w, ref, r);
        if (mesh->tether) project_tethers(s, mesh, w);
        for (int k = 0; k < mesh->edges; ++k) {
            int i = (pass % 2 == 0) ? k : mesh->edges - 1 - k;
            const ClothEdge *e = mesh->edge + i;
            if (e->d >= 0) {
                float b_scale = (e->kind == CLOTH_EDGE_WARP) ? 1.0F : weft_b;
                bend(s, e, w, m->bend * b_scale * scale, &s->bend_lambda[i]);
            }
        }
        for (int k = 0; k < mesh->edges; ++k) {
            int i = (pass % 2 == 0) ? k : mesh->edges - 1 - k;
            const ClothEdge *e = mesh->edge + i;
            float s_scale = e->kind == CLOTH_EDGE_WEFT ? weft_s : (e->kind == CLOTH_EDGE_DIAG ? diag_s : 1.0F);
            stretch(s, e, w, m->stretch * s_scale * scale, m->compression_limit, &s->stretch_lambda[i]);
        }
        project_backstop(s, mesh, w, ref);
        bool last = pass == m->iterations - 1;
        project_shapes(s, mesh, w, (const float (*)[3])x_prev, &wk->bp, m->thickness, mu, last, wk->normal);
        project_convexes(s, mesh, w, (const float (*)[3])x_prev, wk, move, m->thickness, mu, wk->normal);
    }

    bool moved = project_max_distance(s, mesh, w, ref, r);
    for (int pass = 0; pass < m->strain_passes; ++pass)
        if (limit_strain_pass(s, mesh, m, w, (const float (*)[3])wk->normal, pass % 2 != 0)) moved = true;
        else break;
    if (moved) {
        project_backstop(s, mesh, w, ref);
        project_shapes(s, mesh, w, (const float (*)[3])x_prev, &wk->bp, m->thickness, mu, true, NULL);
        project_convexes(s, mesh, w, (const float (*)[3])x_prev, wk, move, m->thickness, mu, NULL);
    }

    for (int i = 0; i < mesh->vertices; ++i) {
        for (int k = 0; k < 3; ++k) s->velocity[i][k] = (s->x[i][k] - x_prev[i][k]) * inv_dt;
        if (!finite_point(s->x[i]) || !finite_point(s->velocity[i])) s->frame_valid = false;
    }
}

/* Moves p along dir until it leaves the shape; the local sphere at the contact's axial parameter gives the exact ray
   exit for a sphere and converges on a cone. */
static void exit_shape_along(float p[3], const float dir[3], const ShapeBox *bx) {
    for (int iteration = 0; iteration < 8; ++iteration) {
        vec3 normal, q, delta;
        float t = 0.0F;
        if (!point_in_box(p, bx->min, bx->max) || box_point_contact(p, bx, normal, &t) <= 0.0F) return;
        glm_vec3_lerp((float *)bx->a->c, (float *)bx->b->c, t, q);
        glm_vec3_sub(p, q, delta);
        float radius = bx->ra + t * (bx->rb - bx->ra);
        float along = glm_vec3_dot(delta, (float *)dir);
        float disc = along * along - (glm_vec3_norm2(delta) - radius * radius);
        float step = disc > 0.0F ? -along + sqrtf(disc) : 0.0F;
        glm_vec3_muladds((float *)dir, fmaxf(step, 0.0F) + 1e-5F, p);
    }
}

/* b names the side of the body, not of a seat or a floor, so a convex is left through its nearest face. */
static void exit_convex_nearest(float p[3], const ConvexPlanes *cv, float thickness) {
    float nearest_d = -FLT_MAX;
    int nearest = 0;
    for (int f = 0; f < cv->plane_count; ++f) {
        float d = glm_vec3_dot((float *)cv->planes[f], p) + cv->planes[f][3];
        if (d >= thickness) return;
        if (d > nearest_d) {
            nearest_d = d;
            nearest = f;
        }
    }
    glm_vec3_muladds((float *)cv->planes[nearest], thickness - nearest_d, p);
}

bool cloth_reset(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, const ClothReference *ref,
                 const float *max_distance, const ClothShapes *shapes) {
    if (!s) return false;
    s->mesh = NULL;
    s->frame_valid = false;
    if (!inputs_valid(mesh, m, ref, max_distance, shapes)) return false;
    const float *r = max_distance ? max_distance : mesh->max_distance;
    StepWork *wk = &work;
    build_broadphase(shapes, m->thickness, &wk->bp);
    prepare_convexes(shapes, wk);
    for (int i = 0; i < mesh->vertices; ++i) {
        glm_vec3_copy((float *)ref[i].x, s->x[i]);
        glm_vec3_zero(s->velocity[i]);
        if (r[i] <= 0.0F) continue;
        vec3 outward;
        glm_vec3_negate_to((float *)ref[i].b, outward);
        for (int pass = 0; pass < 2; ++pass) {
            for (int bi = 0; bi < wk->bp.count; ++bi) exit_shape_along(s->x[i], outward, &wk->bp.boxes[bi]);
            for (int c = 0; c < wk->convex_count; ++c) exit_convex_nearest(s->x[i], &wk->convex[c], m->thickness);
        }
    }
    memset(s->stretch_lambda, 0, sizeof s->stretch_lambda);
    memset(s->bend_lambda, 0, sizeof s->bend_lambda);
    s->mesh = mesh;
    s->frame_valid = true;
    return true;
}

ClothMetrics cloth_measure(const ClothState *s, const ClothMesh *mesh, const ClothMaterial *m,
                           const ClothReference *ref, const float *max_distance, const ClothShapes *shapes) {
    if (!s || !s->frame_valid || s->mesh != mesh || !inputs_valid(mesh, m, ref, max_distance, shapes))
        return (ClothMetrics){0};
    const float *r = max_distance ? max_distance : mesh->max_distance;
    StepWork *wk = &work;
    build_broadphase(shapes, m->thickness, &wk->bp);
    prepare_convexes(shapes, wk);
    ClothMetrics out = {.finite = true};
    for (int i = 0; i < mesh->vertices; ++i) {
        out.finite &= finite_point(s->x[i]) && finite_point(s->velocity[i]);
        out.speed = fmaxf(out.speed, glm_vec3_norm((float *)s->velocity[i]));
        out.max_distance_excess_m = fmaxf(out.max_distance_excess_m,
                                          glm_vec3_distance((float *)s->x[i], (float *)ref[i].x) - r[i]);
        vec3 push;
        if (r[i] > 0.0F)
            out.backstop_depth_m = fmaxf(out.backstop_depth_m, backstop_depth(s->x[i], &ref[i], mesh->backstop[i], push));
        float held = 0.0F;
        for (int bi = 0; bi < wk->bp.count; ++bi) {
            vec3 normal;
            float t;
            const ShapeBox *bx = &wk->bp.boxes[bi];
            if (r[i] > 0.0F && point_in_box(s->x[i], bx->min, bx->max))
                out.penetration = fmaxf(out.penetration, box_point_contact(s->x[i], bx, normal, &t));
            if (point_in_box(ref[i].x, bx->min, bx->max)) held = fmaxf(held, box_point_contact(ref[i].x, bx, normal, &t));
        }
        if (held > r[i]) {
            ++out.held_vertices;
            out.held_depth_m = fmaxf(out.held_depth_m, held - r[i]);
        }
        for (int c = 0; c < wk->convex_count && r[i] > 0.0F; ++c) {
            float outside = -FLT_MAX;
            for (int f = 0; f < wk->convex[c].plane_count; ++f)
                outside = fmaxf(outside, glm_vec3_dot(wk->convex[c].planes[f], (float *)s->x[i]) + wk->convex[c].planes[f][3]);
            out.penetration = fmaxf(out.penetration, m->thickness - outside);
        }
    }
    for (int i = 0; i < mesh->edges; ++i) {
        const ClothEdge *e = mesh->edge + i;
        out.stretch = fmaxf(out.stretch, glm_vec3_distance((float *)s->x[e->a], (float *)s->x[e->b]) / e->length - 1.0F);
    }
    out.finite &= isfinite(out.speed) && isfinite(out.stretch) && isfinite(out.penetration) &&
                  isfinite(out.max_distance_excess_m) && isfinite(out.backstop_depth_m) && isfinite(out.held_depth_m);
    return out;
}
