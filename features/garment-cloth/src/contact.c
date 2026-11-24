#include "garment/contact.h"

#include "math/nt_math.h"

#include <math.h>
#include <string.h>

typedef struct {
    float distance2, weights[3], delta[3];
    float t_axis;
} Witness;

static void perp_unit(const float u[3], float out[3]) {
    vec3 ref = {1, 0, 0};
    if (fabsf(u[0]) > 0.9F) glm_vec3_copy((vec3){0, 1, 0}, ref);
    glm_vec3_cross((float *)u, ref, out);
    float len = glm_vec3_norm(out);
    if (len > 1e-7F) glm_vec3_scale(out, 1.0F / len, out);
    else glm_vec3_copy((vec3){0, 1, 0}, out);
}

static float sphere_contact(const float p[3], const float center[3], float radius, float normal[3]) {
    vec3 delta;
    glm_vec3_sub((float *)p, (float *)center, delta);
    float d2 = glm_vec3_norm2(delta);
    if (d2 >= radius * radius) return 0.0F;
    float d = sqrtf(d2);
    if (d > 1e-7F) glm_vec3_scale(delta, 1.0F / d, normal);
    else glm_vec3_copy((vec3){0, 1, 0}, normal);
    return radius - d;
}

float cloth_point_cone_contact(const float p[3], const ClothSphere *sa, const ClothSphere *sb, float margin,
                               float normal[3], float *t_out) {
    float ra = sa->r + margin;
    float rb = sb->r + margin;
    float r_max = fmaxf(ra, rb);
    for (int k = 0; k < 3; ++k) {
        float low = fminf(sa->c[k], sb->c[k]) - r_max;
        float high = fmaxf(sa->c[k], sb->c[k]) + r_max;
        if (p[k] < low || p[k] > high) return 0.0F;
    }

    vec3 ba, pa;
    glm_vec3_sub((float *)sb->c, (float *)sa->c, ba);
    float len2 = glm_vec3_norm2(ba);
    float dr = ra - rb;

    /* Degenerate length or one sphere fully contains the other: reduce to the dominating sphere. */
    if (len2 <= 1e-12F || dr * dr >= len2) {
        if (ra >= rb) {
            if (t_out) *t_out = 0.0F;
            return sphere_contact(p, sa->c, ra, normal);
        }
        if (t_out) *t_out = 1.0F;
        return sphere_contact(p, sb->c, rb, normal);
    }

    float len = sqrtf(len2);
    vec3 u;
    glm_vec3_scale(ba, 1.0F / len, u);
    glm_vec3_sub((float *)p, (float *)sa->c, pa);
    float y = glm_vec3_dot(pa, u);
    vec3 w;
    glm_vec3_copy(pa, w);
    glm_vec3_muladds(u, -y, w);
    float x = glm_vec3_norm(w);

    float a = sqrtf(fmaxf(0.0F, len2 - dr * dr));
    float s = (-x * dr + y * a) / len;
    if (s <= 0.0F) {
        if (t_out) *t_out = 0.0F;
        return sphere_contact(p, sa->c, ra, normal);
    }
    if (s >= a) {
        if (t_out) *t_out = 1.0F;
        return sphere_contact(p, sb->c, rb, normal);
    }

    float signed_dist = (x * a + y * dr) / len - ra;
    if (signed_dist >= 0.0F) return 0.0F;
    if (t_out) *t_out = glm_clamp(s / a, 0.0F, 1.0F);
    vec3 w_hat;
    if (x > 1e-7F) glm_vec3_scale(w, 1.0F / x, w_hat);
    else perp_unit(u, w_hat);
    float cos_slope = a / len, sin_slope = dr / len;
    for (int k = 0; k < 3; ++k) normal[k] = cos_slope * w_hat[k] + sin_slope * u[k];
    float nlen = glm_vec3_norm(normal);
    if (nlen > 1e-7F) glm_vec3_scale(normal, 1.0F / nlen, normal);
    else glm_vec3_copy((vec3){0, 1, 0}, normal);
    return -signed_dist;
}

float cloth_point_cone_contact_swept(const float p[3], const float prev_p[3], const ClothSphere *sa,
                                     const ClothSphere *sb, float margin, const float fallback_normal[3],
                                     float normal[3], float *t_out) {
    float t = 0.0F;
    float depth = cloth_point_cone_contact(p, sa, sb, margin, normal, &t);
    if (t_out) *t_out = t;
    if (depth <= 0.0F) return depth;
    return cloth_cone_exit_side(p, prev_p, sa, sb, margin, fallback_normal, t, depth, normal);
}

float cloth_cone_exit_side(const float p[3], const float prev_p[3], const ClothSphere *sa, const ClothSphere *sb,
                           float margin, const float fallback_normal[3], float t, float depth, float normal[3]) {
    vec3 q_curr, delta_curr;
    glm_vec3_lerp((float *)sa->c, (float *)sb->c, t, q_curr);
    glm_vec3_sub((float *)p, q_curr, delta_curr);
    float curr_d2 = glm_vec3_norm2(delta_curr);
    float ra = sa->r + margin;
    float rb = sb->r + margin;
    float r_local = ra + t * (rb - ra);

    vec3 recover = {0, 0, 0};
    bool use_recover = false;

    if (prev_p) {
        vec3 prev_ba, prev_pa;
        glm_vec3_sub((float *)sb->prev, (float *)sa->prev, prev_ba);
        glm_vec3_sub((float *)prev_p, (float *)sa->prev, prev_pa);
        float prev_len2 = glm_vec3_norm2(prev_ba);
        float t_prev = prev_len2 > 1e-12F ? glm_clamp(glm_vec3_dot(prev_pa, prev_ba) / prev_len2, 0.0F, 1.0F) : 0.0F;
        vec3 q_prev, delta_prev;
        glm_vec3_lerp((float *)sa->prev, (float *)sb->prev, t_prev, q_prev);
        glm_vec3_sub((float *)prev_p, q_prev, delta_prev);
        float prev_d2 = glm_vec3_norm2(delta_prev);
        if (prev_d2 > 1e-8F && glm_vec3_dot(delta_curr, delta_prev) < 0.0F) {
            glm_vec3_scale(delta_prev, 1.0F / sqrtf(prev_d2), recover);
            use_recover = true;
        }
    }

    if (!use_recover && curr_d2 < 1e-8F && fallback_normal) {
        float flen = glm_vec3_norm((float *)fallback_normal);
        if (flen > 1e-6F) {
            glm_vec3_scale((float *)fallback_normal, 1.0F / flen, recover);
            use_recover = true;
        }
    }

    if (!use_recover) return depth;

    /* Exact ray exit from delta_curr along recover onto sphere of radius r_local. */
    float b_dot = glm_vec3_dot(delta_curr, recover);
    float c_term = curr_d2 - r_local * r_local;
    float disc = b_dot * b_dot - c_term;
    if (disc < 0.0F) return depth;
    float exit_depth = -b_dot + sqrtf(disc);
    if (exit_depth <= 0.0F) return depth;
    glm_vec3_copy(recover, normal);
    return exit_depth;
}

static void candidate(Witness *w, vec3 delta, float u, float v, float t, float t_axis) {
    float d2 = glm_vec3_norm2(delta);
    if (d2 >= w->distance2) return;
    w->distance2 = d2;
    glm_vec3_copy(delta, w->delta);
    glm_vec3_copy((vec3){u, v, t}, w->weights);
    w->t_axis = t_axis;
}

static void edge_witness(Witness *w, vec3 p, vec3 q, int i, vec3 origin, vec3 axis, float axis2) {
    vec3 edge, offset, delta;
    glm_vec3_sub(q, p, edge);
    glm_vec3_sub(p, origin, offset);
    float len2 = glm_vec3_norm2(edge);
    float cross = glm_vec3_dot(edge, axis);
    float e = glm_vec3_dot(edge, offset), f = glm_vec3_dot(axis, offset);
    float divisor = len2 * axis2 - cross * cross;
    float u = divisor > 1e-12F * len2 * axis2 ? glm_clamp((cross * f - e * axis2) / divisor, 0, 1) : 0;
    float v = axis2 > 1e-16F ? glm_clamp((cross * u + f) / axis2, 0, 1) : 0;
    u = len2 > 1e-16F ? glm_clamp((cross * v - e) / len2, 0, 1) : 0;
    glm_vec3_copy(offset, delta);
    glm_vec3_muladds(edge, u, delta);
    glm_vec3_muladds(axis, -v, delta);
    float weights[3] = {0};
    weights[i] = 1.0F - u;
    weights[(i + 1) % 3] = u;
    candidate(w, delta, weights[0], weights[1], weights[2], v);
}

float cloth_face_contact(const float a[3], const float b[3], const float c[3], const ClothSphere *sa,
                         const ClothSphere *sb, float margin, float weights[3], float normal[3]) {
    float ra = sa->r + margin;
    float rb = sb->r + margin;
    float r_max = fmaxf(ra, rb);
    for (int k = 0; k < 3; ++k) {
        float low = fminf(sa->c[k], sb->c[k]) - r_max, high = fmaxf(sa->c[k], sb->c[k]) + r_max;
        if ((a[k] < low && b[k] < low && c[k] < low) || (a[k] > high && b[k] > high && c[k] > high)) return 0;
    }

    /* For tapered cones, sample exact round-cone SDF on triangle vertices and edge midpoints
       first, and also check axis-to-face interior crossing. */
    float best_depth = 0.0F;
    if (fabsf(ra - rb) > 1e-6F) {
        const float *verts[3] = {a, b, c};
        for (int i = 0; i < 3; ++i) {
            vec3 n;
            float d = cloth_point_cone_contact(verts[i], sa, sb, margin, n, NULL);
            if (d > best_depth) {
                best_depth = d;
                weights[0] = (i == 0) ? 1.0F : 0.0F;
                weights[1] = (i == 1) ? 1.0F : 0.0F;
                weights[2] = (i == 2) ? 1.0F : 0.0F;
                glm_vec3_copy(n, normal);
            }
            vec3 mid;
            const float *next = verts[(i + 1) % 3];
            for (int k = 0; k < 3; ++k) mid[k] = 0.5F * (verts[i][k] + next[k]);
            d = cloth_point_cone_contact(mid, sa, sb, margin, n, NULL);
            if (d > best_depth) {
                best_depth = d;
                weights[0] = weights[1] = weights[2] = 0.0F;
                weights[i] = 0.5F;
                weights[(i + 1) % 3] = 0.5F;
                glm_vec3_copy(n, normal);
            }
        }
    }

    vec3 p[3] = {{a[0],a[1],a[2]}, {b[0],b[1],b[2]}, {c[0],c[1],c[2]}};
    vec3 origin = {sa->c[0],sa->c[1],sa->c[2]}, axis, ab, ac, n;
    glm_vec3_sub((float *)sb->c, origin, axis);
    float axis2 = glm_vec3_norm2(axis);
    glm_vec3_sub(p[1], p[0], ab);
    glm_vec3_sub(p[2], p[0], ac);
    glm_vec3_cross(ab, ac, n);
    float n2 = glm_vec3_norm2(n);
    Witness w = {.distance2 = r_max * r_max, .t_axis = 0.0F};
    if (n2 > 1e-18F) {
        vec3 offset;
        glm_vec3_sub(origin, p[0], offset);
        float start = glm_vec3_dot(n, offset), slope = glm_vec3_dot(n, axis);
        /* A segment can enter the projected face from an edge. Test both caps and the plane crossing. */
        float times[3] = {0, 1, fabsf(slope) > 1e-12F ? glm_clamp(-start / slope, 0, 1) : 0};
        for (int i = 0; i < 3; ++i) {
            vec3 at, cross, delta;
            glm_vec3_copy(offset, at);
            glm_vec3_muladds(axis, times[i], at);
            glm_vec3_cross(at, ac, cross);
            float v = glm_vec3_dot(n, cross) / n2;
            glm_vec3_cross(ab, at, cross);
            float t = glm_vec3_dot(n, cross) / n2;
            if (v < 0 || t < 0 || v + t > 1) continue;
            glm_vec3_scale(n, -glm_vec3_dot(at, n) / n2, delta);
            candidate(&w, delta, 1.0F - v - t, v, t, times[i]);
        }
    }
    for (int i = 0; i < 3; ++i) edge_witness(&w, p[i], p[(i + 1) % 3], i, origin, axis, axis2);
    if (w.distance2 < r_max * r_max) {
        float r_local = ra + w.t_axis * (rb - ra);
        if (w.distance2 < r_local * r_local) {
            float distance = sqrtf(w.distance2);
            float d = r_local - distance;
            if (d > best_depth) {
                best_depth = d;
                glm_vec3_copy(w.weights, weights);
                if (distance > 1e-7F) glm_vec3_scale(w.delta, 1.0F / distance, normal);
                else if (n2 > 1e-18F) glm_vec3_scale(n, 1.0F / sqrtf(n2), normal);
                else glm_vec3_copy((vec3){0, 1, 0}, normal);
            }
        }
    }
    return best_depth;
}
