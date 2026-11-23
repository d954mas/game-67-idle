#ifndef GARMENT_CONTACT_H
#define GARMENT_CONTACT_H

#include "garment/cloth.h"

/* Effective end radius of a capsule (radius_b == 0 denotes a uniform capsule). */
static inline float cloth_capsule_radius_b(const ClothCapsule *body) {
    return body->radius_b > 0.0F ? body->radius_b : body->radius;
}

/* Exact closed-form round-cone (tapered capsule) contact for one point.
 * Returns penetration depth (> 0 when inside radius(t) + margin), unit outward normal,
 * and axial parameter t_out in [0, 1] for collider velocity interpolation. */
float cloth_point_capsule_contact(const float p[3], const ClothCapsule *body, float margin,
                                  float normal[3], float *t_out);

/* Point-vs-round-cone contact with temporal previous-side recovery for CLOTH_COLLIDER_INSIDE.
 * When a point crosses the axis between prev_p and p, returns exact ray-to-surface exit along
 * the previous-side outward vector instead of pushing through the opposite side. */
float cloth_point_capsule_contact_swept(const float p[3], const float prev_p[3],
                                        const ClothCapsule *body, float margin,
                                        const float fallback_normal[3],
                                        float normal[3], float *t_out);

/* The temporal side recovery of cloth_point_capsule_contact_swept for a contact already found at axial t with
 * the given depth and normal; returns the depth along the chosen exit and updates normal. */
float cloth_capsule_exit_side(const float p[3], const float prev_p[3], const ClothCapsule *body, float margin,
                              const float fallback_normal[3], float t, float depth, float normal[3]);

/* Closest points on the triangle and capsule axis. Includes face interiors and crossing segments. */
float cloth_face_contact(const float a[3], const float b[3], const float c[3], const ClothCapsule *body,
                         float margin, float weights[3], float normal[3]);

#endif
