#include "garment/render_binding.h"

bool cloth_render_normals(const ClothMesh *mesh, const float positions[][3], ClothRenderNormals *out) {
    if (!mesh || !mesh->face || !positions || !out || mesh->vertices < 3 || mesh->vertices > CLOTH_VERTICES ||
        mesh->faces < 1 || mesh->faces > 2*CLOTH_EDGES/3) return false;
    for (int i = 0; i < mesh->vertices; ++i) {
        for (int k = 0; k < 3; ++k) if (!isfinite(positions[i][k])) return false;
    }
    double area_sum[CLOTH_VERTICES][3] = {{0}};
    double area_weight[CLOTH_VERTICES] = {0};
    for (int f = 0; f < mesh->faces; ++f) {
        ClothFace face = mesh->face[f];
        if (face.a >= mesh->vertices || face.b >= mesh->vertices || face.c >= mesh->vertices ||
            face.a == face.b || face.b == face.c || face.c == face.a) return false;
        double ab[3], ac[3];
        for (int k = 0; k < 3; ++k) {
            ab[k] = (double)positions[face.b][k] - (double)positions[face.a][k];
            ac[k] = (double)positions[face.c][k] - (double)positions[face.a][k];
        }
        double area[] = {ab[1]*ac[2] - ab[2]*ac[1], ab[2]*ac[0] - ab[0]*ac[2], ab[0]*ac[1] - ab[1]*ac[0]};
        double ab2 = 0, ac2 = 0, bc2 = 0, area2 = 0;
        for (int k = 0; k < 3; ++k) {
            double bc = ac[k] - ab[k];
            ab2 += ab[k]*ab[k];
            ac2 += ac[k]*ac[k];
            bc2 += bc*bc;
            area2 += area[k]*area[k];
        }
        /* The smallest altitude relative to the longest edge controls normal stability. */
        double span2 = fmax(ab2, fmax(ac2, bc2));
        if (area2 <= 1e-14 * span2 * span2) {
            for (int k = 0; k < 3; ++k) out->face[f][k] = 0;
            continue;
        }
        double length = sqrt(area2);
        area_weight[face.a] += length;
        area_weight[face.b] += length;
        area_weight[face.c] += length;
        for (int k = 0; k < 3; ++k) {
            out->face[f][k] = (float)(area[k] / length);
            area_sum[face.a][k] += area[k];
            area_sum[face.b][k] += area[k];
            area_sum[face.c][k] += area[k];
        }
    }
    /* Opposing incident areas cancel continuously as a fold turns inside out. */
    for (int i = 0; i < mesh->vertices; ++i) {
        for (int k = 0; k < 3; ++k)
            out->vertex[i][k] = area_weight[i] > 0 ? (float)(area_sum[i][k] / area_weight[i]) : 0.0F;
    }
    return true;
}

float cloth_render_normal_agreement(const ClothRenderBinding *binding, const ClothFace *faces,
                                    const ClothRenderNormals *normals) {
    ClothFace face = faces[binding->face];
    const float *normal = normals->face[binding->face];
    /* Edge and seam bindings may extrapolate position. Sample smooth normals on the triangle. */
    double a = fmax(0.0, 1.0 - (double)binding->u - (double)binding->v);
    double b = fmax(0.0, (double)binding->u), c = fmax(0.0, (double)binding->v);
    double dot = 0;
    for (int k = 0; k < 3; ++k) {
        double n = a*(double)normals->vertex[face.a][k] + b*(double)normals->vertex[face.b][k]
            + c*(double)normals->vertex[face.c][k];
        dot += n * (double)normal[k];
    }
    return (float)fmin(1.0, fmax(-1.0, dot / (a + b + c)));
}

void cloth_render_position_folded(const ClothRenderBinding *binding, const ClothFace *faces,
                                  const float positions[][3], const ClothRenderNormals *normals,
                                  float rest_agreement, float out[3]) {
    ClothFace face = faces[binding->face];
    const float *a = positions[face.a], *b = positions[face.b], *c = positions[face.c];
    float height = binding->height_m;
    if (height != 0) {
        float agreement = cloth_render_normal_agreement(binding, faces, normals);
        height *= fminf(1.0F, fmaxf(0.0F, 1.0F + (agreement - rest_agreement)));
    }
    for (int k = 0; k < 3; ++k) {
        float base = a[k] + binding->u*(b[k] - a[k]) + binding->v*(c[k] - a[k]);
        out[k] = base + height * normals->face[binding->face][k];
    }
}
