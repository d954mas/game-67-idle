#ifndef GARMENT_RENDER_BINDING_H
#define GARMENT_RENDER_BINDING_H

#include "garment/cloth.h"
#include <math.h>

typedef struct {
  uint32_t face;
  float u, v, height_m;
} ClothRenderBinding;

typedef struct {
  /* Vertex vectors preserve the coherence of adjacent faces; face vectors are unit normals. */
  float vertex[CLOTH_VERTICES][3];
  float face[2 * CLOTH_EDGES / 3][3];
} ClothRenderNormals;

/* Evaluate once per dressed proxy. Duplicated seam vertices have independent normals. */
bool cloth_render_normals(const ClothMesh *mesh, const float positions[][3], ClothRenderNormals *out);
float cloth_render_normal_agreement(const ClothRenderBinding *binding, const ClothFace *faces,
                                    const ClothRenderNormals *normals);
/* The reference is the same binding's normal agreement in the rest pose. */
void cloth_render_position_folded(const ClothRenderBinding *binding, const ClothFace *faces,
                                  const float positions[][3], const ClothRenderNormals *normals,
                                  float rest_agreement, float out[3]);

/* Bind in the garment's rest frame. Signed normal height preserves thickness
 * through rigid rotation. */
static inline void cloth_render_position(const ClothRenderBinding *binding,
                                         const ClothFace *faces,
                                         const float positions[][3],
                                         float out[3]) {
  const ClothFace face = faces[binding->face];
  const float *a = positions[face.a], *b = positions[face.b],
              *c = positions[face.c];
  float ab[3], ac[3];
  for (int k = 0; k < 3; ++k) {
    ab[k] = b[k] - a[k];
    ac[k] = c[k] - a[k];
    out[k] = a[k] + binding->u * ab[k] + binding->v * ac[k];
  }
  if (binding->height_m == 0.0F)
    return;
  float n[] = {ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2],
               ab[0] * ac[1] - ab[1] * ac[0]};
  float length = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  if (length > 1e-10F) {
    for (int k = 0; k < 3; ++k)
      out[k] += binding->height_m * n[k] / length;
  }
}

#endif
