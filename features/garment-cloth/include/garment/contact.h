#ifndef GARMENT_CONTACT_H
#define GARMENT_CONTACT_H

#include "garment/cloth.h"

/* A shape is the round cone joining spheres a and b; a == b is the sphere alone. */

/* Exact closed-form round-cone contact for one point at the current centres. Returns the depth (> 0 when inside
   radius(t) + margin), the unit outward normal and the axial parameter t_out in [0, 1]. */
float cloth_point_cone_contact(const float p[3], const ClothSphere *a, const ClothSphere *b, float margin,
                               float normal[3], float *t_out);

/* The same contact, exiting on the side the point was on at the previous substep when it crossed the axis between
   prev_p and p; a point on the axis exits along fallback_normal. */
float cloth_point_cone_contact_swept(const float p[3], const float prev_p[3], const ClothSphere *a,
                                     const ClothSphere *b, float margin, const float fallback_normal[3],
                                     float normal[3], float *t_out);

/* The side recovery of cloth_point_cone_contact_swept for a contact already found at axial t with the given depth
   and normal; returns the depth along the chosen exit and updates normal. */
float cloth_cone_exit_side(const float p[3], const float prev_p[3], const ClothSphere *a, const ClothSphere *b,
                           float margin, const float fallback_normal[3], float t, float depth, float normal[3]);

/* Closest points on the triangle and the cone axis, face interiors and crossing segments included. */
float cloth_face_contact(const float a[3], const float b[3], const float c[3], const ClothSphere *sa,
                         const ClothSphere *sb, float margin, float weights[3], float normal[3]);

#endif
