#include "garment/render_binding.h"

#include <math.h>
#include <string.h>

static void cross3(const float a[3], const float b[3], float out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static float dot3(const float a[3], const float b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static bool normalize3(float v[3]) {
    float len = sqrtf(dot3(v, v));
    if (!isfinite(len) || len <= 1e-8F) return false;
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
    return true;
}

static void fallback_tangent(const float n[3], float t[3]) {
    float ref[3] = {1.0F, 0.0F, 0.0F};
    if (fabsf(n[0]) > 0.9F) {
        ref[0] = 0.0F;
        ref[1] = 1.0F;
    }
    cross3(n, ref, t);
    if (!normalize3(t)) {
        t[0] = 1.0F;
        t[1] = t[2] = 0.0F;
    }
}

static void build_frame(const float p[3], const float neighbor_p[3], const float n_in[3], float r[3][3]) {
    float n[3] = {n_in[0], n_in[1], n_in[2]};
    if (!normalize3(n)) {
        n[0] = 0.0F;
        n[1] = 0.0F;
        n[2] = 1.0F;
    }
    float e[3] = {neighbor_p[0] - p[0], neighbor_p[1] - p[1], neighbor_p[2] - p[2]};
    float proj = dot3(e, n);
    float t[3] = {e[0] - proj * n[0], e[1] - proj * n[1], e[2] - proj * n[2]};
    if (!normalize3(t)) fallback_tangent(n, t);
    float b[3];
    cross3(n, t, b);
    if (!normalize3(b)) {
        fallback_tangent(n, t);
        cross3(n, t, b);
        normalize3(b);
    }
    /* Columns of R are [T, B, N]. */
    for (int row = 0; row < 3; ++row) {
        r[row][0] = t[row];
        r[row][1] = b[row];
        r[row][2] = n[row];
    }
}

static bool vertex_normals(const ClothMesh *mesh, const float pos[][3], float out[][3]) {
    for (int i = 0; i < mesh->vertices; ++i) {
        for (int k = 0; k < 3; ++k) {
            if (!isfinite(pos[i][k])) return false;
            out[i][k] = 0.0F;
        }
    }
    for (int f = 0; f < mesh->faces; ++f) {
        ClothFace face = mesh->face[f];
        if (face.a >= mesh->vertices || face.b >= mesh->vertices || face.c >= mesh->vertices) return false;
        float ab[3], ac[3], n[3];
        for (int k = 0; k < 3; ++k) {
            ab[k] = pos[face.b][k] - pos[face.a][k];
            ac[k] = pos[face.c][k] - pos[face.a][k];
        }
        cross3(ab, ac, n);
        for (int k = 0; k < 3; ++k) {
            out[face.a][k] += n[k];
            out[face.b][k] += n[k];
            out[face.c][k] += n[k];
        }
    }
    return true;
}

bool cloth_vertex_frames_prepare(const ClothMesh *mesh, ClothVertexBasis out[CLOTH_VERTICES]) {
    if (!mesh || !out || !mesh->rest || !mesh->face ||
        mesh->vertices < 3 || mesh->vertices > CLOTH_VERTICES || mesh->faces < 1) return false;

    float normals[CLOTH_VERTICES][3];
    if (!vertex_normals(mesh, mesh->rest, normals)) return false;

    int best_neighbor[CLOTH_VERTICES];
    float best_score[CLOTH_VERTICES];
    for (int i = 0; i < mesh->vertices; ++i) {
        best_neighbor[i] = -1;
        best_score[i] = -1.0F;
    }

    /* Prefer an incident WARP edge for stable tangent alignment; fall back to longest incident edge. */
    if (mesh->edge && mesh->edges > 0) {
        for (int i = 0; i < mesh->edges; ++i) {
            const ClothEdge *e = mesh->edge + i;
            if (e->a < 0 || e->a >= mesh->vertices || e->b < 0 || e->b >= mesh->vertices) return false;
            float bonus = (e->kind == CLOTH_EDGE_WARP) ? 10.0F : 0.0F;
            float score = bonus + e->length;
            if (score > best_score[e->a]) {
                best_score[e->a] = score;
                best_neighbor[e->a] = e->b;
            }
            if (score > best_score[e->b]) {
                best_score[e->b] = score;
                best_neighbor[e->b] = e->a;
            }
        }
    } else {
        for (int f = 0; f < mesh->faces; ++f) {
            int ids[3] = {mesh->face[f].a, mesh->face[f].b, mesh->face[f].c};
            for (int k = 0; k < 3; ++k) {
                int u = ids[k], v = ids[(k + 1) % 3];
                float d[3] = {
                    mesh->rest[v][0] - mesh->rest[u][0],
                    mesh->rest[v][1] - mesh->rest[u][1],
                    mesh->rest[v][2] - mesh->rest[u][2]
                };
                float len = sqrtf(dot3(d, d));
                if (len > best_score[u]) { best_score[u] = len; best_neighbor[u] = v; }
                if (len > best_score[v]) { best_score[v] = len; best_neighbor[v] = u; }
            }
        }
    }

    for (int i = 0; i < mesh->vertices; ++i) {
        if (best_neighbor[i] < 0) return false;
        float r0[3][3];
        build_frame(mesh->rest[i], mesh->rest[best_neighbor[i]], normals[i], r0);
        for (int r = 0; r < 3; ++r) {
            out[i].p0[r] = mesh->rest[i][r];
            for (int c = 0; c < 3; ++c) out[i].r0_t[r][c] = r0[c][r];
        }
        out[i].ref_neighbor = best_neighbor[i];
    }
    return true;
}

bool cloth_vertex_frames_eval(const ClothMesh *mesh, const ClothVertexBasis basis[CLOTH_VERTICES],
                              const float posed[][3], float out_mat34[][12]) {
    if (!mesh || !basis || !posed || !out_mat34 ||
        mesh->vertices < 3 || mesh->vertices > CLOTH_VERTICES) return false;

    float normals[CLOTH_VERTICES][3];
    if (!vertex_normals(mesh, posed, normals)) return false;

    for (int i = 0; i < mesh->vertices; ++i) {
        int nb = basis[i].ref_neighbor;
        if (nb < 0 || nb >= mesh->vertices) return false;
        float r_curr[3][3];
        build_frame(posed[i], posed[nb], normals[i], r_curr);
        /* M = R_curr * R_0^T; translation = posed[i] - M * p0. */
        float *m = out_mat34[i];
        for (int row = 0; row < 3; ++row) {
            float tx = posed[i][row];
            for (int col = 0; col < 3; ++col) {
                float val = r_curr[row][0] * basis[i].r0_t[0][col] +
                            r_curr[row][1] * basis[i].r0_t[1][col] +
                            r_curr[row][2] * basis[i].r0_t[2][col];
                m[row * 4 + col] = val;
                tx -= val * basis[i].p0[col];
            }
            m[row * 4 + 3] = tx;
        }
    }
    return true;
}
