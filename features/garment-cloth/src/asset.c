#include "garment/asset.h"
#include "core/nt_assert.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* The records are the wire format: these pin their layout on every target. */
_Static_assert(sizeof(GarmentBlobHeader) == 20 && sizeof(GarmentBlobSection) == 16, "blob header layout");
_Static_assert(sizeof(ClothMaterial) == 60 && offsetof(ClothMaterial, iterations) == 20 &&
                   offsetof(ClothMaterial, strain_passes) == 28 && offsetof(ClothMaterial, friction) == 52 &&
                   offsetof(ClothMaterial, compression_limit) == 56,
               "material layout");
_Static_assert(sizeof(GarmentBlobPiece) == 76 && offsetof(GarmentBlobPiece, material) == 16, "piece layout");
_Static_assert(sizeof(GarmentBlobBone) == 52, "bone layout");
_Static_assert(sizeof(ClothFace) == 6 && sizeof(ClothTether) == 8 && sizeof(ClothBackstop) == 8, "face, tether and backstop layout");
_Static_assert(sizeof(ClothEdge) == 28 && offsetof(ClothEdge, length) == 16 && offsetof(ClothEdge, kind) == 24,
               "edge layout");
_Static_assert(sizeof(GarmentSkin) == 24 && offsetof(GarmentSkin, weight) == 8, "skin layout");
_Static_assert(sizeof(GarmentSphere) == 20 && offsetof(GarmentSphere, center) == 4 && offsetof(GarmentSphere, radius) == 16,
               "sphere layout");
_Static_assert(sizeof(GarmentCone) == 4, "cone layout");
_Static_assert(sizeof(GarmentCorrective) == 32 && offsetof(GarmentCorrective, direction) == 12 &&
                   offsetof(GarmentCorrective, backstop) == 24,
               "corrective layout");

enum { STRINGS, BONES, SPHERES, CONES, CORRECTIVE_NAMES, PIECE, REST, INV_AREA, FACES, EDGES, TETHERS, SKIN,
       MAX_DISTANCE, BACKSTOP, DIRECTION, CORRECTIVES, KINDS };
static const struct {
    uint32_t tag;
    uint16_t stride, align;
    bool outfit;
} k_kinds[KINDS] = {
    [STRINGS] = {GARMENT_SECTION_STRINGS, 1, 1, true},
    [BONES] = {GARMENT_SECTION_BONES, sizeof(GarmentBlobBone), _Alignof(GarmentBlobBone), true},
    [SPHERES] = {GARMENT_SECTION_SPHERES, sizeof(GarmentSphere), _Alignof(GarmentSphere), true},
    [CONES] = {GARMENT_SECTION_CONES, sizeof(GarmentCone), _Alignof(GarmentCone), true},
    [CORRECTIVE_NAMES] = {GARMENT_SECTION_CORRECTIVE_NAMES, sizeof(uint32_t), _Alignof(uint32_t), true},
    [PIECE] = {GARMENT_SECTION_PIECE, sizeof(GarmentBlobPiece), _Alignof(GarmentBlobPiece), false},
    [REST] = {GARMENT_SECTION_REST, 3 * sizeof(float), _Alignof(float), false},
    [INV_AREA] = {GARMENT_SECTION_INV_AREA, sizeof(float), _Alignof(float), false},
    [FACES] = {GARMENT_SECTION_FACES, sizeof(ClothFace), _Alignof(ClothFace), false},
    [EDGES] = {GARMENT_SECTION_EDGES, sizeof(ClothEdge), _Alignof(ClothEdge), false},
    [TETHERS] = {GARMENT_SECTION_TETHERS, sizeof(ClothTether), _Alignof(ClothTether), false},
    [SKIN] = {GARMENT_SECTION_SKIN, sizeof(GarmentSkin), _Alignof(GarmentSkin), false},
    [MAX_DISTANCE] = {GARMENT_SECTION_MAX_DISTANCE, sizeof(float), _Alignof(float), false},
    [BACKSTOP] = {GARMENT_SECTION_BACKSTOP, sizeof(ClothBackstop), _Alignof(ClothBackstop), false},
    [DIRECTION] = {GARMENT_SECTION_DIRECTION, 3 * sizeof(float), _Alignof(float), false},
    [CORRECTIVES] = {GARMENT_SECTION_CORRECTIVES, sizeof(GarmentCorrective), _Alignof(GarmentCorrective), false},
};

/* One allocation per loaded blob: the solver's pointer structs and the bone names they address. */
typedef struct {
    GarmentSet set;
    GarmentAsset pieces[GARMENT_PIECES_MAX];
    ClothMesh meshes[GARMENT_PIECES_MAX];
    const char *corrective_names[GARMENT_CORRECTIVES_MAX];
    GarmentBone bones[];
} AssetView;

typedef struct {
    const uint8_t *base;
    const GarmentBlobSection *at[GARMENT_PIECES_MAX + 1][KINDS];
    const char *strings;
    uint32_t string_bytes;
} Blob;

static const GarmentBlobSection *section(const Blob *b, int piece, int kind, bool required) {
    const GarmentBlobSection *s = b->at[piece][kind];
    NT_ASSERT(s || !required);
    return s;
}

static const void *records(const Blob *b, const GarmentBlobSection *s) {
    return s ? b->base + s->offset : NULL;
}

static const char *string(const Blob *b, uint32_t offset) {
    NT_ASSERT(offset < b->string_bytes);
    return b->strings + offset;
}

static uint32_t count_of(const GarmentBlobSection *s) {
    return s ? s->count : 0U;
}

const GarmentSet *garment_asset_view(const void *blob, uint32_t size) {
    Blob b = {.base = blob};
    NT_ASSERT(b.base && ((uintptr_t)b.base & 3U) == 0 && size >= sizeof(GarmentBlobHeader));
    const GarmentBlobHeader *h = blob;
    NT_ASSERT(h->magic == GARMENT_BLOB_MAGIC && h->version == GARMENT_BLOB_VERSION && h->header_size == sizeof *h &&
              h->size == size && h->piece_count <= GARMENT_PIECES_MAX &&
              h->section_count <= (size - sizeof *h) / sizeof(GarmentBlobSection));
    const GarmentBlobSection *table = (const GarmentBlobSection *)(b.base + h->header_size);
    const uint32_t payload = h->header_size + (uint32_t)h->section_count * (uint32_t)sizeof *table;
    for (int i = 0; i < h->section_count; ++i) {
        const GarmentBlobSection *s = &table[i];
        int kind = 0;
        while (kind < KINDS && k_kinds[kind].tag != s->tag) ++kind;
        NT_ASSERT(kind < KINDS);
        const bool outfit = s->piece == GARMENT_BLOB_OUTFIT;
        NT_ASSERT(outfit == k_kinds[kind].outfit && (outfit || s->piece < h->piece_count) && s->stride == k_kinds[kind].stride &&
                  s->offset % GARMENT_BLOB_ALIGN == 0 && s->offset >= payload && s->offset <= size &&
                  s->count <= (size - s->offset) / s->stride && ((uintptr_t)(b.base + s->offset) % k_kinds[kind].align) == 0);
        const GarmentBlobSection **slot = &b.at[outfit ? GARMENT_PIECES_MAX : s->piece][kind];
        NT_ASSERT(*slot == NULL);
        *slot = s;
    }
    const int outfit = GARMENT_PIECES_MAX;
    const GarmentBlobSection *strings = section(&b, outfit, STRINGS, true);
    b.strings = records(&b, strings);
    b.string_bytes = strings->count;
    NT_ASSERT(b.string_bytes > 0 && b.strings[b.string_bytes - 1] == '\0');
    if (h->piece_count == 0) return NULL;

    const GarmentBlobSection *bone_section = section(&b, outfit, BONES, true);
    const uint32_t bones = bone_section->count;
    NT_ASSERT(bones >= 1 && bones <= GARMENT_BONES_MAX);
    const GarmentBlobSection *sphere_section = section(&b, outfit, SPHERES, false);
    const GarmentBlobSection *cone_section = section(&b, outfit, CONES, false);
    const uint32_t spheres = count_of(sphere_section), cones = count_of(cone_section);
    NT_ASSERT(spheres <= CLOTH_SHAPES && cones <= CLOTH_SHAPES);
    const GarmentSphere *sphere = records(&b, sphere_section);
    for (uint32_t i = 0; i < spheres; ++i) NT_ASSERT(sphere[i].bone < bones);
    const GarmentCone *cone = records(&b, cone_section);
    for (uint32_t i = 0; i < cones; ++i) NT_ASSERT(cone[i].a < spheres && cone[i].b < spheres);
    const GarmentBlobSection *name_section = section(&b, outfit, CORRECTIVE_NAMES, false);
    const uint32_t correctives = count_of(name_section);
    NT_ASSERT(correctives <= GARMENT_CORRECTIVES_MAX);

    AssetView *view = calloc(1, sizeof *view + bones * sizeof(GarmentBone));
    NT_ASSERT(view);
    const GarmentBlobBone *source_bones = records(&b, bone_section);
    for (uint32_t i = 0; i < bones; ++i) {
        view->bones[i].name = string(&b, source_bones[i].name);
        memcpy(view->bones[i].inverse_bind, source_bones[i].inverse_bind, sizeof view->bones[i].inverse_bind);
    }
    const uint32_t *name_offset = records(&b, name_section);
    for (uint32_t i = 0; i < correctives; ++i) {
        view->corrective_names[i] = string(&b, name_offset[i]);
        for (uint32_t j = 0; j < i; ++j) NT_ASSERT(strcmp(view->corrective_names[i], view->corrective_names[j]) != 0);
    }
    view->set = (GarmentSet){.id = string(&b, h->id), .bones = view->bones, .bone_count = (int)bones,
                             .spheres = sphere, .sphere_count = (int)spheres, .cones = cone, .cone_count = (int)cones,
                             .pieces = view->pieces, .piece_count = h->piece_count,
                             .corrective_names = view->corrective_names, .corrective_count = (int)correctives};
    for (int p = 0; p < h->piece_count; ++p) {
        const GarmentBlobSection *piece_section = section(&b, p, PIECE, true);
        NT_ASSERT(piece_section->count == 1);
        const GarmentBlobPiece *piece = records(&b, piece_section);
        const uint32_t n = section(&b, p, REST, true)->count;
        const GarmentBlobSection *face_section = section(&b, p, FACES, true), *edge_section = section(&b, p, EDGES, true);
        const GarmentBlobSection *tethers = section(&b, p, TETHERS, false);
        const GarmentBlobSection *shapes = section(&b, p, CORRECTIVES, false);
        const uint32_t edges = edge_section->count, faces = face_section->count;
        NT_ASSERT(n >= 3 && n <= CLOTH_VERTICES && edges >= 3 && edges <= CLOTH_EDGES && faces >= 1 && faces <= 2 * edges / 3 &&
                  section(&b, p, INV_AREA, true)->count == n && section(&b, p, SKIN, true)->count == n &&
                  section(&b, p, MAX_DISTANCE, true)->count == n && section(&b, p, BACKSTOP, true)->count == n &&
                  section(&b, p, DIRECTION, true)->count == n && (!tethers || tethers->count == n) &&
                  (!shapes || (correctives > 0 && shapes->count == correctives * n)));
        const ClothFace *face = records(&b, face_section);
        for (uint32_t i = 0; i < faces; ++i) NT_ASSERT(face[i].a < n && face[i].b < n && face[i].c < n);
        const ClothEdge *edge = records(&b, edge_section);
        for (uint32_t i = 0; i < edges; ++i) {
            const ClothEdge *e = &edge[i];
            NT_ASSERT(e->a >= 0 && (uint32_t)e->a < n && e->b >= 0 && (uint32_t)e->b < n && e->c >= 0 && (uint32_t)e->c < n &&
                      e->d >= -1 && e->d < (int)n);
        }
        const GarmentCorrective *shape = records(&b, shapes);
        for (uint32_t i = 0; i < count_of(shapes); ++i)
            NT_ASSERT(isfinite(shape[i].backstop.d) && shape[i].backstop.d >= 0.0F && shape[i].backstop.R >= 0.0F);
        const ClothTether *tether = records(&b, tethers);
        for (uint32_t i = 0; i < count_of(tethers); ++i) NT_ASSERT(tether[i].pin >= -1 && tether[i].pin < (int)n);
        const GarmentSkin *skin = records(&b, section(&b, p, SKIN, true));
        for (uint32_t i = 0; i < n; ++i)
            for (int j = 0; j < 4; ++j) NT_ASSERT(skin[i].bone[j] < bones);
        NT_ASSERT(piece->render_vertices >= 0);

        view->meshes[p] = (ClothMesh){
            .vertices = (int)n, .faces = (int)faces, .edges = (int)edges,
            .rest = records(&b, section(&b, p, REST, true)), .inv_area = records(&b, section(&b, p, INV_AREA, true)),
            .face = face, .edge = edge, .tether = tether, .max_distance = records(&b, section(&b, p, MAX_DISTANCE, true)),
            .backstop = records(&b, section(&b, p, BACKSTOP, true)),
        };
        view->pieces[p] = (GarmentAsset){
            .id = string(&b, piece->id), .mesh = &view->meshes[p], .skin = skin,
            .direction = records(&b, section(&b, p, DIRECTION, true)), .corrective = shape,
            .material = piece->material,
            .render_vertices = piece->render_vertices, .linear_inertia = piece->linear_inertia,
            .angular_inertia = piece->angular_inertia,
        };
    }
    return &view->set;
}

void garment_asset_free(const GarmentSet *set) {
    free((void *)set);
}
