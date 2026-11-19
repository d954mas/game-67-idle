#ifndef GARMENT_CONTACT_H
#define GARMENT_CONTACT_H

#include "garment/cloth.h"

/* Closest points on the triangle and capsule axis. Includes face interiors and crossing segments. */
float cloth_face_contact(const float a[3], const float b[3], const float c[3], const ClothCapsule *body,
                         float margin, float weights[3], float normal[3]);

#endif
