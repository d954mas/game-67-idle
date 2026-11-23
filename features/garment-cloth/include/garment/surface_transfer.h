#ifndef GARMENT_SURFACE_TRANSFER_H
#define GARMENT_SURFACE_TRANSFER_H

#include "garment/cloth.h"

typedef struct {
  float r0_t[3][3]; /* Transpose of orthonormal rest frame R_0 = [T_0, B_0, N_0]. */
  float p0[3];      /* Rest vertex position in local/actor space (m). */
  int ref_neighbor; /* Neighbor vertex index defining tangent orientation. */
} ClothVertexBasis;

/* Build rest-pose smooth orthonormal frames for every proxy vertex. */
bool cloth_vertex_frames_prepare(const ClothMesh *mesh, ClothVertexBasis out[CLOTH_VERTICES]);

/* Evaluate non-degenerate hybrid barycentric + unit-quaternion transfer records (12 floats per vertex):
 * row 0 = (P_curr.xyz, q.x), row 1 = (P_rest.xyz, q.y), row 2 = (q.z, q.w, 0, 0),
 * where q is the unit quaternion of R_v * R_0^T. Blending P_curr, P_rest and hemisphere-aligned q
 * guarantees det(R) == 1 without thickness collapse or normal inversion on sharp folds. */
bool cloth_vertex_quats_eval(const ClothMesh *mesh, const ClothVertexBasis basis[CLOTH_VERTICES],
                             const float posed[][3], float out_packed12[][12]);

#endif
