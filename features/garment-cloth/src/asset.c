#include "garment/asset.h"
#include "core/nt_assert.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* The records are the wire format: these pin their layout on every target. */
_Static_assert(sizeof(GarmentBlobHeader) == 20 && sizeof(GarmentBlobSection) == 16, "blob header layout");
_Static_assert(sizeof(ClothMaterial) == 76 && offsetof(ClothMaterial, iterations) == 20 &&
                   offsetof(ClothMaterial, strain_passes) == 28 && offsetof(ClothMaterial, support_drape) == 72,
               "material layout");
_Static_assert(sizeof(GarmentBlobPiece) == 96 && offsetof(GarmentBlobPiece, material) == 20, "piece layout");
_Static_assert(sizeof(GarmentBlobBone) == 52, "bone layout");
_Static_assert(sizeof(ClothFace) == 6 && sizeof(ClothTether) == 8, "face and tether layout");
_Static_assert(sizeof(ClothEdge) == 28 && offsetof(ClothEdge, length) == 16 && offsetof(ClothEdge, kind) == 24,
               "edge layout");
_Static_assert(sizeof(GarmentVertex) == 44 && offsetof(GarmentVertex, weight) == 8 && offsetof(GarmentVertex, normal) == 24 &&
                   offsetof(GarmentVertex, travel_m) == 36 && offsetof(GarmentVertex, backstop_m) == 40,
               "vertex layout");
_Static_assert(sizeof(GarmentCollider) == 40 && offsetof(GarmentCollider, point_a) == 4 && offsetof(GarmentCollider, point_b) == 16 &&
                   offsetof(GarmentCollider, radius_m) == 28 && offsetof(GarmentCollider, flags) == 36,
               "collider layout");
_Static_assert(sizeof(GarmentLap) == 28 && offsetof(GarmentLap, start_deg) == 12, "lap layout");

enum { STRINGS, OWNED, PIECE, BONES, REST, INV_AREA, FACES, EDGES, TETHERS, DRIVE, VERTICES, COLLIDERS, LAP, KINDS };
static const struct {
    uint32_t tag;
    uint16_t stride, align;
    bool outfit;
} k_kinds[KINDS] = {
    [STRINGS] = {GARMENT_SECTION_STRINGS, 1, 1, true},
    [OWNED] = {GARMENT_SECTION_OWNED, sizeof(uint32_t), _Alignof(uint32_t), true},
    [PIECE] = {GARMENT_SECTION_PIECE, sizeof(GarmentBlobPiece), _Alignof(GarmentBlobPiece), false},
    [BONES] = {GARMENT_SECTION_BONES, sizeof(GarmentBlobBone), _Alignof(GarmentBlobBone), false},
    [REST] = {GARMENT_SECTION_REST, 3 * sizeof(float), _Alignof(float), false},
    [INV_AREA] = {GARMENT_SECTION_INV_AREA, sizeof(float), _Alignof(float), false},
    [FACES] = {GARMENT_SECTION_FACES, sizeof(ClothFace), _Alignof(ClothFace), false},
    [EDGES] = {GARMENT_SECTION_EDGES, sizeof(ClothEdge), _Alignof(ClothEdge), false},
    [TETHERS] = {GARMENT_SECTION_TETHERS, sizeof(ClothTether), _Alignof(ClothTether), false},
    [DRIVE] = {GARMENT_SECTION_DRIVE, sizeof(float), _Alignof(float), false},
    [VERTICES] = {GARMENT_SECTION_VERTICES, sizeof(GarmentVertex), _Alignof(GarmentVertex), false},
    [COLLIDERS] = {GARMENT_SECTION_COLLIDERS, sizeof(GarmentCollider), _Alignof(GarmentCollider), false},
    [LAP] = {GARMENT_SECTION_LAP, sizeof(GarmentLap), _Alignof(GarmentLap), false},
};

/* One allocation per loaded blob: the solver's pointer structs and the bone names they address. */
typedef struct {
    GarmentSet set;
    GarmentAsset pieces[GARMENT_PIECES_MAX];
    ClothMesh meshes[GARMENT_PIECES_MAX];
    GarmentBone bones[]; /* followed by the owned-joint name pointers */
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
    const GarmentBlobSection *strings = section(&b, GARMENT_PIECES_MAX, STRINGS, true);
    b.strings = records(&b, strings);
    b.string_bytes = strings->count;
    NT_ASSERT(b.string_bytes > 0 && b.strings[b.string_bytes - 1] == '\0');
    const GarmentBlobSection *owned = section(&b, GARMENT_PIECES_MAX, OWNED, false);
    if (h->piece_count == 0) return NULL;
    uint32_t bone_total = 0;
    for (int p = 0; p < h->piece_count; ++p) {
        const uint32_t bones = section(&b, p, BONES, true)->count;
        NT_ASSERT(bones >= 1 && bones <= GARMENT_BONES_MAX);
        bone_total += bones;
    }
    AssetView *view = calloc(1, sizeof *view + bone_total * sizeof(GarmentBone) + count_of(owned) * sizeof(const char *));
    NT_ASSERT(view);
    const char **owned_names = (const char **)(view->bones + bone_total);
    const uint32_t *owned_offsets = records(&b, owned);
    for (uint32_t i = 0; i < count_of(owned); ++i) owned_names[i] = string(&b, owned_offsets[i]);
    view->set = (GarmentSet){.id = string(&b, h->id), .pieces = view->pieces, .piece_count = h->piece_count,
                             .owned_joints = owned_names, .owned_joint_count = (int)count_of(owned)};
    GarmentBone *bone = view->bones;
    for (int p = 0; p < h->piece_count; ++p) {
        const GarmentBlobSection *piece_section = section(&b, p, PIECE, true);
        NT_ASSERT(piece_section->count == 1);
        const GarmentBlobPiece *piece = records(&b, piece_section);
        const GarmentBlobSection *bone_section = section(&b, p, BONES, true);
        const int bone_count = (int)bone_section->count;
        const GarmentBlobBone *source_bones = records(&b, bone_section);
        for (int i = 0; i < bone_count; ++i) {
            bone[i].name = string(&b, source_bones[i].name);
            memcpy(bone[i].inverse_bind, source_bones[i].inverse_bind, sizeof bone[i].inverse_bind);
        }

        const uint32_t n = section(&b, p, REST, true)->count;
        const GarmentBlobSection *face_section = section(&b, p, FACES, true), *edge_section = section(&b, p, EDGES, true);
        const GarmentBlobSection *tethers = section(&b, p, TETHERS, false), *drive = section(&b, p, DRIVE, false);
        const uint32_t edges = edge_section->count, faces = face_section->count;
        NT_ASSERT(n >= 3 && n <= CLOTH_VERTICES && edges >= 3 && edges <= CLOTH_EDGES && faces >= 1 && faces <= 2 * edges / 3 &&
                  section(&b, p, INV_AREA, true)->count == n && section(&b, p, VERTICES, true)->count == n &&
                  (!tethers || tethers->count == n) && (!drive || drive->count == n));
        const ClothFace *face = records(&b, face_section);
        for (uint32_t i = 0; i < faces; ++i) NT_ASSERT(face[i].a < n && face[i].b < n && face[i].c < n);
        const ClothEdge *edge = records(&b, edge_section);
        for (uint32_t i = 0; i < edges; ++i) {
            const ClothEdge *e = &edge[i];
            NT_ASSERT(e->a >= 0 && (uint32_t)e->a < n && e->b >= 0 && (uint32_t)e->b < n && e->c >= 0 && (uint32_t)e->c < n &&
                      e->d >= -1 && e->d < (int)n);
        }
        const ClothTether *tether = records(&b, tethers);
        for (uint32_t i = 0; i < count_of(tethers); ++i) NT_ASSERT(tether[i].pin >= -1 && tether[i].pin < (int)n);
        const GarmentVertex *vertex = records(&b, section(&b, p, VERTICES, true));
        for (uint32_t i = 0; i < n; ++i)
            for (int j = 0; j < 4; ++j) NT_ASSERT(vertex[i].bone[j] < bone_count);

        const GarmentBlobSection *collider_section = section(&b, p, COLLIDERS, false), *lap_section = section(&b, p, LAP, false);
        const uint32_t colliders = count_of(collider_section);
        NT_ASSERT(colliders + (lap_section ? 2U : 0U) <= GARMENT_CAPSULES_MAX && (!lap_section || lap_section->count == 1));
        const GarmentCollider *collider = records(&b, collider_section);
        for (uint32_t i = 0; i < colliders; ++i) NT_ASSERT(collider[i].bone_a < bone_count && collider[i].bone_b < bone_count);
        const GarmentLap *lap = records(&b, lap_section);
        for (int k = 0; lap && k < 6; ++k) NT_ASSERT(lap->bone[k] < bone_count);
        NT_ASSERT(piece->render_vertices >= 0);

        view->meshes[p] = (ClothMesh){
            .vertices = (int)n, .faces = (int)faces, .edges = (int)edges,
            .rest = records(&b, section(&b, p, REST, true)), .inv_area = records(&b, section(&b, p, INV_AREA, true)),
            .face = face, .edge = edge, .tether = tether, .anim_drive = records(&b, drive),
        };
        view->pieces[p] = (GarmentAsset){
            .id = string(&b, piece->id), .bones = bone, .bone_count = bone_count, .mesh = &view->meshes[p],
            .vertices = vertex, .material = piece->material, .colliders = collider, .collider_count = (int)colliders,
            .render_vertices = piece->render_vertices, .linear_inertia = piece->linear_inertia,
            .angular_inertia = piece->angular_inertia, .lap = lap, .downward_follow = piece->downward_follow,
        };
        bone += bone_count;
    }
    return &view->set;
}

void garment_asset_free(const GarmentSet *set) {
    free((void *)set);
}
