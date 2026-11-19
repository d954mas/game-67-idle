#include "garment/cloth.h"
#include "garment/contact.h"
#include "math/nt_math.h"

#include <float.h>
#include <string.h>

static bool finite_point(const float p[3]) {
    return isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2]);
}

bool cloth_mesh_valid(const ClothMesh *mesh) {
    if (!mesh || mesh->vertices < 3 || mesh->vertices > CLOTH_VERTICES ||
        mesh->edges < 3 || mesh->edges > CLOTH_EDGES || mesh->faces < 1 || mesh->faces > 2 * mesh->edges / 3 ||
        !mesh->rest || !mesh->inv_area || !mesh->edge || !mesh->face) return false;
    for (int i = 0; i < mesh->vertices; ++i) {
        if (!finite_point(mesh->rest[i]) || !isfinite(mesh->inv_area[i]) || mesh->inv_area[i] < 0) return false;
        if (mesh->tether) {
            ClothTether t = mesh->tether[i];
            if (t.pin < -1 || t.pin >= mesh->vertices || !isfinite(t.max_length) || t.max_length < 0 ||
                (t.pin >= 0 && mesh->inv_area[t.pin] != 0.0F)) return false;
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
            !isfinite(e->length) || e->length <= 0 || !isfinite(e->angle) || fabsf(e->angle) > NT_PI) return false;
    }
    return true;
}

bool cloth_frame_valid(const ClothMesh *mesh, const ClothMaterial *m, float dt,
                       const float anchors[][3], const ClothGuide *guide, const ClothCapsule *body, int count) {
    if (!mesh || mesh->vertices < 3 || mesh->vertices > CLOTH_VERTICES || !m || !anchors || count < 0 || (count && !body) ||
        !isfinite(dt) || dt <= 0 || !isfinite(m->density) || m->density <= 0 || !isfinite(m->stretch) || m->stretch < 0 ||
        !isfinite(m->bend) || m->bend < 0 || !isfinite(m->drag) || m->drag < 0 ||
        !isfinite(m->thickness) || m->thickness < 0 || m->iterations < 1 ||
        !isfinite(m->max_edge_strain) || m->max_edge_strain < 0 ||
        m->strain_passes < 0 || m->strain_passes > CLOTH_MAX_STRAIN_PASSES) return false;
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
        if (!finite_point(body[i].a) || !finite_point(body[i].b) ||
            !isfinite(body[i].radius) || body[i].radius < 0 || !isfinite(body[i].radius + m->thickness)) return false;
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
    memcpy(s->previous, pose, bytes);
    s->mesh = mesh;
    s->frame_valid = true;
}

static void stretch(ClothState *s, const ClothEdge *e, const float *w, float alpha, float *lambda) {
    vec3 dx;
    glm_vec3_sub(s->x[e->b], s->x[e->a], dx);
    float length = glm_vec3_norm(dx), weight = w[e->a] + w[e->b];
    if (length < 1e-8F || weight <= 0) return;
    float dl = (e->length - length - alpha * *lambda) / (weight + alpha);
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
    glm_vec3_cross(n1, n2, cross);
    float angle = atan2f(glm_vec3_dot(edge, cross) / length, glm_vec3_dot(n1, n2));
    float error = angle - e->angle;
    if (error > NT_PI) error -= 2.0F * NT_PI;
    if (error < -NT_PI) error += 2.0F * NT_PI;
    glm_vec3_scale(n1, -length / a2, grad[2]);
    glm_vec3_scale(n2, -length / b2, grad[3]);
    float u = glm_vec3_dot(ac, edge) / edge2, v = glm_vec3_dot(ad, edge) / edge2;
    glm_vec3_scale(grad[2], -u, grad[1]);
    glm_vec3_muladds(grad[3], -v, grad[1]);
    glm_vec3_scale(grad[2], u - 1.0F, grad[0]);
    glm_vec3_muladds(grad[3], v - 1.0F, grad[0]);
    float weight = 0;
    for (int i = 0; i < 4; ++i) weight += w[ids[i]] * glm_vec3_norm2(grad[i]);
    if (weight <= 0) return;
    float dl = (-error - alpha * *lambda) / (weight + alpha);
    *lambda += dl;
    for (int i = 0; i < 4; ++i) glm_vec3_muladds(grad[i], dl * w[ids[i]], s->x[ids[i]]);
}

static void project_body(ClothState *s, const ClothMesh *mesh, float thickness, const ClothCapsule *body, int count) {
    for (int f = 0; f < mesh->faces; ++f) {
        const ClothFace face = mesh->face[f];
        int ids[] = {face.a, face.b, face.c};
        for (int j = 0; j < count; ++j) {
            vec3 bary, normal;
            float depth = cloth_face_contact(s->x[face.a], s->x[face.b], s->x[face.c], body + j, thickness, bary, normal);
            if (depth <= 0) continue;
            float weight = 0;
            for (int i = 0; i < 3; ++i) weight += bary[i] * bary[i] * mesh->inv_area[ids[i]];
            if (weight < 1e-8F) continue;
            float largest = 0;
            for (int i = 0; i < 3; ++i) largest = fmaxf(largest, bary[i] * mesh->inv_area[ids[i]]);
            /* Near a pinned seam a tiny free barycentric weight can amplify a millimeter contact into meters. */
            float span = glm_vec3_distance((float *)mesh->rest[face.a], (float *)mesh->rest[face.b]);
            float impulse = fminf(depth / weight, 0.25F * span / largest);
            for (int i = 0; i < 3; ++i) {
                glm_vec3_muladds(normal, impulse * bary[i] * mesh->inv_area[ids[i]], s->x[ids[i]]);
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

static void limit_strain(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m,
                         const ClothCapsule *body, int count) {
    float ratio = 1.0F + m->max_edge_strain;
    bool pending_contact = false;
    for (int pass = 0; pass < m->strain_passes; ++pass) {
        bool moved = false;
        for (int k = 0; k < mesh->edges; ++k) {
            const ClothEdge *e = &mesh->edge[pass % 2 == 0 ? k : mesh->edges - 1 - k];
            vec3 delta;
            glm_vec3_sub(s->x[e->b], s->x[e->a], delta);
            float length = glm_vec3_norm(delta), target = ratio * e->length;
            float wa = mesh->inv_area[e->a], wb = mesh->inv_area[e->b];
            if (length <= target || wa + wb == 0) continue;
            moved = true;
            float scale = (length - target) / (length * (wa + wb));
            glm_vec3_muladds(delta, scale * wa, s->x[e->a]);
            glm_vec3_muladds(delta, -scale * wb, s->x[e->b]);
        }
        if (!moved && !pending_contact) break;
        pending_contact = count > 0;
        /* A contact can lengthen a neighboring edge. A remaining sweep must observe that change. */
        if (pending_contact && (pass % 2 != 0 || pass == m->strain_passes - 1)) {
            project_body(s, mesh, m->thickness, body, count);
            pending_contact = false;
        }
    }
}

void cloth_step(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                const float anchors[][3], const ClothGuide *guide, const ClothCapsule *body, int count) {
    if (!s) return;
    s->frame_valid = s->mesh == mesh && cloth_frame_valid(mesh, m, dt, anchors, guide, body, count);
    if (!s->frame_valid) return;
    float retain = expf(-m->drag * dt);
    for (int i = 0; i < mesh->vertices; ++i) {
        vec3 dx;
        glm_vec3_sub(s->x[i], s->previous[i], dx);
        glm_vec3_copy(s->x[i], s->previous[i]);
        if (mesh->inv_area[i] == 0) glm_vec3_copy((float *)anchors[i], s->x[i]);
        else {
            glm_vec3_muladds(dx, retain, s->x[i]);
            s->x[i][1] -= 9.81F * dt * dt;
        }
    }
    /* Weights are inverse area; multiplying compliance by density cancels the same factor in mass. */
    float scale = m->density / (dt * dt);
    memset(s->stretch_lambda, 0, (size_t)mesh->edges * sizeof(float));
    memset(s->bend_lambda, 0, (size_t)mesh->edges * sizeof(float));
    for (int pass = 0; pass < m->iterations; ++pass) {
        for (int k = 0; k < mesh->edges; ++k) {
            int i = (pass % 2 == 0) ? k : mesh->edges - 1 - k;
            const ClothEdge *e = mesh->edge + i;
            stretch(s, e, mesh->inv_area, m->stretch * scale, &s->stretch_lambda[i]);
            if (e->d >= 0) bend(s, e, mesh->inv_area, m->bend * scale, &s->bend_lambda[i]);
        }
        if (guide) {
            for (int i = 0; i < mesh->vertices; ++i) {
                if (mesh->inv_area[i] == 0.0F) continue;
                vec3 offset;
                glm_vec3_sub(s->x[i], (float *)anchors[i], offset);
                float inward = -glm_vec3_dot(offset, (float *)guide[i].normal) - guide[i].backstop;
                if (inward > 0) glm_vec3_muladds((float *)guide[i].normal, inward, offset);
                float length = glm_vec3_norm(offset);
                if (length > guide[i].travel) glm_vec3_scale(offset, guide[i].travel / length, offset);
                glm_vec3_add((float *)anchors[i], offset, s->x[i]);
            }
        }
        if (mesh->tether) project_tethers(s, mesh);
        project_body(s, mesh, m->thickness, body, count);
    }
    if (m->strain_passes) limit_strain(s, mesh, m, body, count);
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
    double travel = (double)g->travel, backstop = (double)g->backstop;
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
    double inward = -dot3(delta, normal) - backstop;
    if (inward <= 0.0 && dot3(delta, delta) <= travel * travel) return;
    if (inward > 0.0) {
        for (int k = 0; k < 3; ++k) delta[k] += inward * normal[k];
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
        if (dot3(actual, actual) <= travel * travel && dot3(actual, normal) >= -backstop) return;
    }
    memcpy(x, anchor, 3 * sizeof *x);
}

void cloth_step_shaped(ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                       const float anchors[][3], const ClothGuide *guide, const ClothCapsule *body, int count) {
    cloth_step(s, mesh, m, dt, anchors, guide, body, count);
    if (!s || !s->frame_valid || !guide) return;
    for (int i = 0; i < mesh->vertices; ++i) {
        if (!finite_point(s->x[i]) || !finite_point(s->previous[i])) {
            s->frame_valid = false;
            return;
        }
        if (mesh->inv_area[i] != 0.0F) cap_shape_residual(s->x[i], anchors[i], &guide[i]);
    }
}

ClothMetrics cloth_measure(const ClothState *s, const ClothMesh *mesh, const ClothMaterial *m, float dt,
                           const float anchors[][3], const ClothCapsule *body, int count) {
    if (!s || !s->frame_valid || s->mesh != mesh || !cloth_frame_valid(mesh, m, dt, anchors, NULL, body, count)) return (ClothMetrics){0};
    ClothMetrics out = {.finite = true};
    for (int i = 0; i < mesh->vertices; ++i) {
        for (int k = 0; k < 3; ++k) out.finite &= isfinite(s->x[i][k]) && isfinite(s->previous[i][k]);
        out.speed = fmaxf(out.speed, glm_vec3_distance((float *)s->x[i], (float *)s->previous[i]) / dt);
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
