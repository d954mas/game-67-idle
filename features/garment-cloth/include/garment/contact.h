#ifndef GARMENT_CONTACT_H
#define GARMENT_CONTACT_H

#include "garment/cloth.h"

/* A shape is the round cone joining spheres a and b; a == b is the sphere alone. */

/* Exact closed-form round-cone contact for one point at the current centres. Returns the depth (> 0 when inside
   radius(t) + margin), the unit outward normal and the axial parameter t_out in [0, 1]. */
float cloth_point_cone_contact(const float p[3], const ClothSphere *a, const ClothSphere *b, float margin,
                               float normal[3], float *t_out);

/* Closest points on the triangle and the cone axis, face interiors and crossing segments included. */
float cloth_face_contact(const float a[3], const float b[3], const float c[3], const ClothSphere *sa,
                         const ClothSphere *sb, float margin, float weights[3], float normal[3]);

#endif
