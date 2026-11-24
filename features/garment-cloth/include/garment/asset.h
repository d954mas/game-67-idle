#ifndef GARMENT_ASSET_H
#define GARMENT_ASSET_H

#include "garment/actor.h"

#include <stdint.h>

/* One cooked outfit, written by tools/garment_blob.py. Little-endian. The section table follows the
   header; each section holds a plain array of one record type and starts at a multiple of
   GARMENT_BLOB_ALIGN from the blob start, so a 4-byte-aligned blob gives every record its alignment. */
#define GARMENT_BLOB_TAG(a, b, c, d) ((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)
#define GARMENT_BLOB_MAGIC GARMENT_BLOB_TAG('G', 'R', 'M', 'T')
enum { GARMENT_BLOB_VERSION = 3, GARMENT_BLOB_ALIGN = 16, GARMENT_BLOB_OUTFIT = 0xFFFF };

/* Outfit-wide sections carry piece = GARMENT_BLOB_OUTFIT; the others carry their piece index. */
#define GARMENT_SECTION_STRINGS GARMENT_BLOB_TAG('S', 'T', 'R', 'S')      /* outfit: char, NUL-terminated UTF-8 */
#define GARMENT_SECTION_BONES GARMENT_BLOB_TAG('B', 'O', 'N', 'E')        /* outfit: GarmentBlobBone */
#define GARMENT_SECTION_SPHERES GARMENT_BLOB_TAG('S', 'P', 'H', 'R')      /* outfit: GarmentSphere, optional */
#define GARMENT_SECTION_CONES GARMENT_BLOB_TAG('C', 'O', 'N', 'E')        /* outfit: GarmentCone, optional */
#define GARMENT_SECTION_CORRECTIVE_NAMES GARMENT_BLOB_TAG('C', 'N', 'A', 'M') /* outfit: uint32 string offset, optional */
#define GARMENT_SECTION_PIECE GARMENT_BLOB_TAG('P', 'I', 'E', 'C')        /* GarmentBlobPiece, one */
#define GARMENT_SECTION_REST GARMENT_BLOB_TAG('R', 'E', 'S', 'T')         /* float[3] */
#define GARMENT_SECTION_INV_AREA GARMENT_BLOB_TAG('I', 'N', 'V', 'A')     /* float */
#define GARMENT_SECTION_FACES GARMENT_BLOB_TAG('F', 'A', 'C', 'E')        /* ClothFace */
#define GARMENT_SECTION_EDGES GARMENT_BLOB_TAG('E', 'D', 'G', 'E')        /* ClothEdge */
#define GARMENT_SECTION_TETHERS GARMENT_BLOB_TAG('T', 'E', 'T', 'H')      /* ClothTether, optional */
#define GARMENT_SECTION_SKIN GARMENT_BLOB_TAG('S', 'K', 'I', 'N')         /* GarmentSkin */
#define GARMENT_SECTION_MAX_DISTANCE GARMENT_BLOB_TAG('M', 'A', 'X', 'D') /* float, m */
#define GARMENT_SECTION_BACKSTOP GARMENT_BLOB_TAG('B', 'K', 'S', 'T')     /* ClothBackstop */
#define GARMENT_SECTION_DIRECTION GARMENT_BLOB_TAG('B', 'D', 'I', 'R')    /* float[3], unit */
/* GarmentCorrective, corrective-major: (corrective count) x vertices; optional, absent where every delta is zero. */
#define GARMENT_SECTION_CORRECTIVES GARMENT_BLOB_TAG('C', 'O', 'R', 'R')

typedef struct {
    uint32_t magic;
    uint16_t version, header_size;
    uint32_t size; /* whole blob in bytes */
    uint32_t id;   /* string offset */
    uint16_t piece_count, section_count;
} GarmentBlobHeader;
typedef struct {
    uint32_t tag;
    uint16_t piece;
    uint16_t stride; /* record size in bytes */
    uint32_t offset, count;
} GarmentBlobSection;
typedef struct {
    uint32_t id; /* string offset */
    int32_t render_vertices;
    float linear_inertia, angular_inertia;
    ClothMaterial material;
} GarmentBlobPiece;
typedef struct {
    uint32_t name; /* string offset */
    float inverse_bind[12];
} GarmentBlobBone;

/* Asserts structure only (magic, version, sizes, counts against the solver limits, offsets, index
   ranges): values are checked by the cook. The set points into the blob, which must outlive it.
   NULL for a valid blob of an outfit without cloth. */
const GarmentSet *garment_asset_view(const void *blob, uint32_t size);
/* NULL is allowed. */
void garment_asset_free(const GarmentSet *set);

#endif
