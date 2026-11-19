#include "garment/contact.h"

#include "math/nt_math.h"

typedef struct {
    float distance2, weights[3], delta[3];
} Witness;

static void candidate(Witness *w, vec3 delta, float u, float v, float t) {
    float d2 = glm_vec3_norm2(delta);
    if (d2 >= w->distance2) return;
    w->distance2 = d2;
    glm_vec3_copy(delta, w->delta);
    glm_vec3_copy((vec3){u, v, t}, w->weights);
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
    candidate(w, delta, weights[0], weights[1], weights[2]);
}

float cloth_face_contact(const float a[3], const float b[3], const float c[3], const ClothCapsule *body,
                         float margin, float weights[3], float normal[3]) {
    float radius = body->radius + margin;
    for (int k = 0; k < 3; ++k) {
        float low = fminf(body->a[k], body->b[k]) - radius, high = fmaxf(body->a[k], body->b[k]) + radius;
        if ((a[k] < low && b[k] < low && c[k] < low) || (a[k] > high && b[k] > high && c[k] > high)) return 0;
    }
    vec3 p[3] = {{a[0],a[1],a[2]}, {b[0],b[1],b[2]}, {c[0],c[1],c[2]}};
    vec3 origin = {body->a[0],body->a[1],body->a[2]}, axis, ab, ac, n;
    glm_vec3_sub((float *)body->b, origin, axis);
    float axis2 = glm_vec3_norm2(axis);
    glm_vec3_sub(p[1], p[0], ab);
    glm_vec3_sub(p[2], p[0], ac);
    glm_vec3_cross(ab, ac, n);
    float n2 = glm_vec3_norm2(n);
    Witness w = {.distance2 = radius * radius};
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
            candidate(&w, delta, 1.0F - v - t, v, t);
        }
    }
    for (int i = 0; i < 3; ++i) edge_witness(&w, p[i], p[(i + 1) % 3], i, origin, axis, axis2);
    if (w.distance2 >= radius * radius) return 0;
    float distance = sqrtf(w.distance2);
    glm_vec3_copy(w.weights, weights);
    if (distance > 1e-7F) glm_vec3_scale(w.delta, 1.0F / distance, normal);
    else if (n2 > 1e-18F) glm_vec3_scale(n, 1.0F / sqrtf(n2), normal);
    else glm_vec3_copy((vec3){0, 1, 0}, normal);
    return radius - distance;
}
