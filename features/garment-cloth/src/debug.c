#include "garment/debug.h"
#include "math/nt_math.h"

#include <math.h>

static const float shape_color[] = {0.18F, 1.0F, 0.38F};
static const float reference_color[] = {0.62F, 0.62F, 0.66F};
static const float backstop_color[] = {1.0F, 0.52F, 0.10F};
static const float bone_color[] = {0.90F, 0.40F, 1.0F};
static const float pinned_color[] = {1.0F, 1.0F, 0.25F};
static const float slack_color[] = {0.12F, 0.82F, 1.0F};
static const float taut_color[] = {1.0F, 0.68F, 0.10F};
static const float over_color[] = {1.0F, 0.18F, 0.12F};
static const float pi = 3.14159265359F;

void cloth_debug_mesh(const ClothMesh *mesh, const float positions[][3], const float *weights,
                      unsigned layers, ClothDebugLine emit, void *user) {
    if (!mesh || !positions || !emit) return;
    if (layers & CLOTH_DEBUG_WEIGHTS) {
        float strongest = 0.0F;
        for (int i = 0; !weights && i < mesh->vertices; ++i) strongest = fmaxf(strongest, mesh->max_distance[i]);
        for (int e = 0; e < mesh->edges; ++e) {
            const int vertices[] = {mesh->edge[e].a, mesh->edge[e].b};
            float middle[3];
            for (int k = 0; k < 3; ++k)
                middle[k] = 0.5F * (positions[vertices[0]][k] + positions[vertices[1]][k]);
            for (int side = 0; side < 2; ++side) {
                const int i = vertices[side];
                const float w = weights ? weights[i] : (strongest > 0.0F ? mesh->max_distance[i] / strongest : 0.0F);
                const float rgb[] = {0.16F + 0.84F * w, 0.42F - 0.24F * w, 1.0F - 0.86F * w};
                emit(user, CLOTH_DEBUG_WEIGHTS, positions[i], middle, rgb);
            }
        }
        for (int i = 0; i < mesh->vertices; ++i) {
            if (mesh->max_distance[i] != 0.0F) continue;
            for (int k = 0; k < 3; ++k) {
                float a[3], b[3];
                for (int c = 0; c < 3; ++c) a[c] = b[c] = positions[i][c];
                a[k] -= 0.005F;
                b[k] += 0.005F;
                emit(user, CLOTH_DEBUG_WEIGHTS, a, b, pinned_color);
            }
        }
    }
    if ((layers & CLOTH_DEBUG_TETHERS) && mesh->tether) {
        for (int i = 0; i < mesh->vertices; ++i) {
            ClothTether t = mesh->tether[i];
            if (t.pin < 0 || mesh->max_distance[i] == 0.0F) continue;
            float distance = glm_vec3_distance((float *)positions[i], (float *)positions[t.pin]);
            const float *color = distance > t.max_length + 1e-5F ? over_color :
                                 distance >= 0.98F * t.max_length ? taut_color : slack_color;
            emit(user, CLOTH_DEBUG_TETHERS, positions[i], positions[t.pin], color);
        }
    }
}

static void round_point(const float center[3], float r, const float axis[3], const float ray[3],
                        float angle, float p[3]) {
    float axial = r * cosf(angle), radial = r * sinf(angle);
    for (int k = 0; k < 3; ++k) p[k] = center[k] + axial * axis[k] + radial * ray[k];
}

static void arc(const float center[3], float r, const float axis[3], const float ray[3],
                float start, float end, int steps, ClothDebugLine emit, void *user) {
    float previous[3];
    round_point(center, r, axis, ray, start, previous);
    for (int i = 1; i <= steps; ++i) {
        float next[3];
        round_point(center, r, axis, ray, start + (end - start) * (float)i / (float)steps, next);
        emit(user, CLOTH_DEBUG_SHAPES, previous, next, shape_color);
        glm_vec3_copy(next, previous);
    }
}

void cloth_debug_reference(const ClothMesh *mesh, const float positions[][3], const ClothReference *ref,
                           const float *max_distance, const ClothBackstop *backstop, unsigned layers, ClothDebugLine emit,
                           void *user) {
    if (!mesh || !positions || !ref || !emit) return;
    const float *r = max_distance ? max_distance : mesh->max_distance;
    const ClothBackstop *backstops = backstop ? backstop : mesh->backstop;
    if (layers & CLOTH_DEBUG_REFERENCE) {
        for (int e = 0; e < mesh->edges; ++e)
            emit(user, CLOTH_DEBUG_REFERENCE, ref[mesh->edge[e].a].x, ref[mesh->edge[e].b].x, reference_color);
    }
    for (int i = 0; i < mesh->vertices; ++i) {
        if ((layers & CLOTH_DEBUG_MAX_DISTANCE) && r[i] > 0.0F) {
            float share = glm_vec3_distance((float *)positions[i], (float *)ref[i].x) / r[i];
            const float rgb[] = {fminf(1.0F, share), 0.3F * (1.0F - fminf(1.0F, share)), share > 1.0F ? 0.0F : 1.0F - share};
            emit(user, CLOTH_DEBUG_MAX_DISTANCE, ref[i].x, positions[i], rgb);
        }
        const ClothBackstop bs = backstops[i];
        if ((layers & CLOTH_DEBUG_BACKSTOP) && bs.R > 0.0F && r[i] > 0.0F) {
            float surface[3];
            for (int k = 0; k < 3; ++k) surface[k] = ref[i].x[k] + ref[i].b[k] * fmaxf(bs.d, 0.003F);
            emit(user, CLOTH_DEBUG_BACKSTOP, ref[i].x, surface, backstop_color);
        }
    }
}

void cloth_debug_cone(const ClothSphere *sa, const ClothSphere *sb, ClothDebugLine emit, void *user) {
    if (!sa || !sb || !emit) return;
    vec3 axis, u, v;
    glm_vec3_sub((float *)sb->c, (float *)sa->c, axis);
    const float length = glm_vec3_norm(axis);
    const float ra = sa->r, rb = sb->r;
    if (length <= 1e-6F || length <= fabsf(ra - rb)) {
        static const float basis[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};
        for (int k = 0; k < 3; ++k)
            arc(ra >= rb ? sa->c : sb->c, fmaxf(ra, rb), basis[k], basis[(k+1)%3],
                0, 2*pi, 16, emit, user);
        return;
    }
    glm_vec3_scale(axis, 1.0F / length, axis);
    vec3 seed = {1, 0, 0};
    if (fabsf(axis[0]) > 0.9F) { seed[0] = 0; seed[1] = 1; }
    glm_vec3_cross(axis, seed, u);
    glm_vec3_normalize(u);
    glm_vec3_cross(axis, u, v);
    /* Unequal spheres meet their common tangent at displaced latitudes. */
    const float slope = (ra - rb) / length;
    const float theta = acosf(slope), radial = sinf(theta);
    const float *centers[] = {sa->c, sb->c};
    const float radii[] = {ra, rb};
    for (int j = 0; j < 2; ++j) {
        vec3 ring;
        for (int k = 0; k < 3; ++k) ring[k] = centers[j][k] + radii[j] * slope * axis[k];
        arc(ring, radii[j] * radial, u, v, 0, 2*pi, 16, emit, user);
    }
    for (int j = 0; j < 4; ++j) {
        vec3 ray, a, b;
        glm_vec3_scale(j % 2 == 0 ? u : v, j < 2 ? 1.0F : -1.0F, ray);
        arc(sa->c, ra, axis, ray, theta, pi, 4, emit, user);
        arc(sb->c, rb, axis, ray, 0, theta, 4, emit, user);
        round_point(sa->c, ra, axis, ray, theta, a);
        round_point(sb->c, rb, axis, ray, theta, b);
        emit(user, CLOTH_DEBUG_SHAPES, a, b, shape_color);
    }
}

void cloth_debug_bone(const float parent[3], const float joint[3], ClothDebugLine emit, void *user) {
    if (parent && joint && emit) emit(user, CLOTH_DEBUG_SKELETON, parent, joint, bone_color);
}
