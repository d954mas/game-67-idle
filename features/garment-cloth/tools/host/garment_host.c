/* garment_host: the feature's own runtime behind a C ABI, for offline tools driven from Python (tools/garment/host.py).
   One fabric model exists: this library steps the same actor, solver and contact code the game links. */
#include "garment/actor.h"
#include "garment/asset.h"
#include "garment/surface_transfer.h"
#include "hash/nt_hash.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define GH_API __declspec(dllexport)
#else
#define GH_API __attribute__((visibility("default")))
#endif

typedef struct GarmentHost {
    void *blob; /* the view points into it */
    const GarmentSet *view;
    /* The actor's set: a copy of the view whose meshes and directions a reference override may redirect. */
    GarmentSet set;
    GarmentAsset pieces[GARMENT_PIECES_MAX];
    ClothMesh meshes[GARMENT_PIECES_MAX];
    float (*rest[GARMENT_PIECES_MAX])[3];
    float (*direction[GARMENT_PIECES_MAX])[3];
    uint16_t *parent;
    uint32_t *joint_id;
    nt_skeletal_skeleton_t skel;
    GarmentActor actor;
} GarmentHost;

GH_API void gh_free(GarmentHost *h) {
    if (!h) return;
    garment_actor_clear(&h->actor);
    garment_asset_free(h->view);
    for (int p = 0; p < GARMENT_PIECES_MAX; ++p) {
        free(h->rest[p]);
        free(h->direction[p]);
    }
    free(h->parent);
    free(h->joint_id);
    free(h->blob);
    free(h);
}

/* The render transfer frames come from the authored rest, as the game's do: a reference override is a corrective of the
   reference, and the shader's frames do not move with it. */
static bool prepare_transfer(GarmentHost *h) {
    GarmentActor *a = &h->actor;
    int total = 0;
    for (int p = 0; p < h->set.piece_count; ++p) total += h->meshes[p].vertices;
    ClothVertexBasis *one = calloc(CLOTH_VERTICES, sizeof *one);
    a->vertex_basis = calloc((size_t)total, sizeof *a->vertex_basis);
    a->palette = calloc((size_t)total, sizeof *a->palette);
    bool ok = one && a->vertex_basis && a->palette;
    for (int p = 0, offset = 0; ok && p < h->set.piece_count; offset += h->meshes[p++].vertices) {
        ok = cloth_vertex_frames_prepare(&h->meshes[p], one);
        if (ok) memcpy(a->vertex_basis + offset, one, (size_t)h->meshes[p].vertices * sizeof *one);
    }
    free(one);
    if (ok) a->palette_count = total;
    return ok;
}

/* parents[j] is the skeleton parent of joint j (UINT16_MAX at a root) and must precede it, as the engine's skeleton
   does; names are the joint node names the blob's bones are matched by. */
GH_API GarmentHost *gh_load(const void *blob, uint32_t size, int joint_count, const char *const *names,
                            const uint16_t *parents) {
    if (!blob || size < sizeof(GarmentBlobHeader) || joint_count <= 0 || joint_count > UINT16_MAX || !names || !parents)
        return NULL;
    GarmentHost *h = calloc(1, sizeof *h);
    if (!h) return NULL;
    h->blob = malloc(size);
    h->parent = calloc((size_t)joint_count, sizeof *h->parent);
    h->joint_id = calloc((size_t)joint_count, sizeof *h->joint_id);
    if (!h->blob || !h->parent || !h->joint_id) goto fail;
    for (int j = 0; j < joint_count; ++j) {
        if (!names[j] || (parents[j] != NT_SKELETAL_NO_PARENT && parents[j] >= j)) goto fail;
        h->parent[j] = parents[j];
        h->joint_id[j] = nt_hash32_str(names[j]).value;
    }
    h->skel = (nt_skeletal_skeleton_t){.rig_compat_id = {1}, .parent = h->parent, .joint_id = h->joint_id,
                                       .joint_count = (uint16_t)joint_count};
    memcpy(h->blob, blob, size);
    h->view = garment_asset_view(h->blob, size);
    if (!h->view) goto fail;
    h->set = *h->view;
    for (int p = 0; p < h->set.piece_count; ++p) {
        h->pieces[p] = h->view->pieces[p];
        h->meshes[p] = *h->view->pieces[p].mesh;
        h->pieces[p].mesh = &h->meshes[p];
    }
    h->set.pieces = h->pieces;
    if (!garment_actor_prepare(&h->actor, &h->set, &h->skel) || !prepare_transfer(h)) goto fail;
    return h;
fail:
    gh_free(h);
    return NULL;
}

GH_API int gh_piece_count(const GarmentHost *h) { return h ? h->set.piece_count : 0; }

GH_API int gh_piece_vertices(const GarmentHost *h, int piece) {
    return h && piece >= 0 && piece < h->set.piece_count ? h->meshes[piece].vertices : 0;
}

/* pose: joint_count model-space 3x4 row-major matrices; world: the actor root, column-major 4x4, NULL for identity;
   convexes in world space. The rest is GarmentInput. */
GH_API bool gh_step(GarmentHost *h, const float *pose, const float *world, float dt, bool reset, bool render,
                    bool measure, const ClothConvex *convexes, int convex_count, float max_distance_scale,
                    float max_distance_bias) {
    static const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    if (!h || !pose) return false;
    const GarmentInput in = {.pose = (const nt_skeletal_mat34_t *)pose, .world = world ? world : identity, .dt = dt,
                             .reset = reset, .render = render, .convexes = convexes, .convex_count = convex_count,
                             .max_distance_scale = max_distance_scale, .max_distance_bias = max_distance_bias,
                             .measure = measure};
    return garment_actor_update(&h->actor, &h->set, &h->skel, &in);
}

/* Simulated positions at the last substep, in actor space: vertices x 3. */
GH_API bool gh_positions(const GarmentHost *h, int piece, float *out) {
    const ClothState *cloth;
    if (!h || !out || !garment_actor_piece(&h->actor, piece, &cloth, NULL)) return false;
    memcpy(out, cloth->x, (size_t)h->meshes[piece].vertices * sizeof cloth->x[0]);
    return true;
}

/* The reference the solver used at the frame pose: x and b per vertex, vertices x 6. */
GH_API bool gh_reference(const GarmentHost *h, int piece, float *out) {
    const ClothReference *ref;
    if (!h || !out || !garment_actor_piece(&h->actor, piece, NULL, &ref)) return false;
    memcpy(out, ref, (size_t)h->meshes[piece].vertices * sizeof *ref);
    return true;
}

/* The GPU transfer records of the last rendered update, every piece's vertices in order, 12 floats each; returns the
   record count, or 0 before a rendered update. */
GH_API int gh_records(const GarmentHost *h, float *out) {
    if (!h || !h->actor.primed || !h->actor.palette) return 0;
    if (out) memcpy(out, h->actor.palette, (size_t)h->actor.palette_count * sizeof h->actor.palette[0]);
    return h->actor.palette_count;
}

/* finite, steps, max strain, max displacement, max-distance excess, backstop depth, held depth (m), held vertices. */
GH_API bool gh_metrics(const GarmentHost *h, float out[8]) {
    if (!h || !out) return false;
    const GarmentStats *s = &h->actor.stats;
    const float values[8] = {s->finite ? 1.0F : 0.0F, (float)s->steps, s->max_strain, s->max_displacement_m,
                             s->max_distance_excess_m, s->backstop_depth_m, s->held_depth_m, (float)s->held_vertices};
    memcpy(out, values, sizeof values);
    return true;
}

/* Replace a piece's skinned rest (vertices x 3) and body direction at rest (vertices x 3, normalized after skinning,
   as the runtime does) with a corrected shape; NULL restores the cooked one. Takes effect at the next update. */
GH_API bool gh_override(GarmentHost *h, int piece, const float *rest, const float *direction) {
    if (!h || piece < 0 || piece >= h->set.piece_count) return false;
    const int n = h->meshes[piece].vertices;
    const GarmentAsset *cooked = &h->view->pieces[piece];
    struct {
        float (**store)[3];
        const float *source;
    } fields[2] = {{&h->rest[piece], rest}, {&h->direction[piece], direction}};
    for (int k = 0; k < 2; ++k) {
        if (!fields[k].source) {
            free(*fields[k].store);
            *fields[k].store = NULL;
            continue;
        }
        if (!*fields[k].store && !(*fields[k].store = malloc((size_t)n * sizeof **fields[k].store))) return false;
        memcpy(*fields[k].store, fields[k].source, (size_t)n * sizeof **fields[k].store);
    }
    h->meshes[piece].rest = h->rest[piece] ? (const float (*)[3])h->rest[piece] : cooked->mesh->rest;
    h->pieces[piece].direction = h->direction[piece] ? (const float (*)[3])h->direction[piece] : cooked->direction;
    return true;
}
