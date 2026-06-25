# Skinned Mesh Renderer Contract v001

Status: T0007 preparation contract.
Owner: reusable `extensions/skeletal_animation/` module.
Boundary: do not edit `external/neotolis-engine`.

## Purpose

Turn the current Mine Cards game-local CPU-skinned proof into a reusable
extension path that can render an Ozz-driven skinned character and socketed
gear in Mine Cards and later games.

The existing extension already owns Ozz loading/sampling through:

- `include/skeletal_animation/nt_skeletal_animation.h`
- `src/nt_skeletal_animation_ozz.cpp`

This contract defines the next layer: runtime mesh data, CPU skinning, rigid
attachments, and a narrow renderer bridge.

## Current Split

Current reusable layer:

- loads `.ozz` skeleton and animation archives;
- samples clips;
- exposes model-space joint matrices;
- resolves attachment joints by name.

Current Mine Cards proof layer:

- generated mesh header: `src/mine_cards_kaykit_mesh.gen.h`;
- local CPU skinning/render code: `src/mine_cards_model_proof.c`;
- hardcoded KayKit symbols and pickaxe attachment offsets.

T0007 moves proof behavior into a reusable module while keeping game-specific
assets/data outside the module.

## Non-Goals

- No changes to `external/neotolis-engine`.
- No broad material/GLB scene importer rewrite.
- No GPU skinning in v001 unless CPU skinning misses the budget.
- No final Mine Cards custom character art.
- No equipment mechanics or inventory UI.

## Coordinate Policy

The game wants Y-up content.

The renderer contract should preserve source-space convention in the mesh asset
metadata, then require an explicit `asset_to_world` or `asset_to_render` matrix
at draw time. Do not hide the current KayKit Y-up to engine Z-up conversion as a
Mine Cards-only hardcode in the reusable module.

Minimum v001 metadata:

- `source_up_axis`;
- `source_forward_axis`;
- `bind_to_model` or `asset_to_model` matrix;
- per-attachment local offset/orientation.

## Runtime Mesh Data

The reusable module receives already-converted runtime data. It does not parse
source GLB in the game loop.

Required skinned mesh fields:

```c
typedef struct nt_skeletal_mesh_vertex {
    float position[3];
    float normal[3];
    float uv[2];
    uint16_t joints[4];
    float weights[4];
} nt_skeletal_mesh_vertex_t;

typedef struct nt_skeletal_mesh_material_slot {
    const char *name;
    uint32_t index_offset;
    uint32_t index_count;
    int texture_id; /* optional; -1 for v001 vertex-color/debug material */
} nt_skeletal_mesh_material_slot_t;

typedef struct nt_skeletal_mesh_socket {
    const char *name;
    const char *joint_name;
    float local_offset[3];
    float local_rotation_quat[4];
    float local_scale[3];
} nt_skeletal_mesh_socket_t;

typedef struct nt_skeletal_mesh_desc {
    const nt_skeletal_mesh_vertex_t *vertices;
    uint32_t vertex_count;
    const uint32_t *indices;
    uint32_t index_count;
    const float *inverse_bind_matrices; /* joint_count * 16, column-major */
    const char *const *joint_names;
    uint32_t joint_count;
    const nt_skeletal_mesh_material_slot_t *material_slots;
    uint32_t material_slot_count;
    const nt_skeletal_mesh_socket_t *sockets;
    uint32_t socket_count;
    float asset_to_model[16];
} nt_skeletal_mesh_desc_t;
```

Optional v001 fallback:

- allow color-only vertices for the KayKit proof;
- keep normal/uv/material fields in the contract so the format does not have to
  change when textured custom art arrives.

## Runtime Instance Data

The module owns GPU buffers and CPU-skinned scratch memory per mesh instance.

Required instance inputs:

- mesh description;
- Ozz model matrices copied from `nt_skeletal_anim_copy_model_matrices`;
- draw transform / MVP input supplied by the game;
- enabled material slots;
- socket attachments.

Required instance outputs:

- updated dynamic VBO for CPU-skinned vertices;
- socket world transforms;
- draw call(s) through existing engine graphics APIs.

## API Shape

Suggested C-facing API:

```c
typedef struct nt_skeletal_mesh nt_skeletal_mesh_t;
typedef struct nt_skeletal_mesh_instance nt_skeletal_mesh_instance_t;

int nt_skeletal_mesh_create(const nt_skeletal_mesh_desc_t *desc,
                            nt_skeletal_mesh_t **out_mesh,
                            char *error,
                            size_t error_cap);

void nt_skeletal_mesh_destroy(nt_skeletal_mesh_t *mesh);

int nt_skeletal_mesh_instance_create(nt_skeletal_mesh_t *mesh,
                                     nt_skeletal_mesh_instance_t **out_instance,
                                     char *error,
                                     size_t error_cap);

void nt_skeletal_mesh_instance_destroy(nt_skeletal_mesh_instance_t *instance);

int nt_skeletal_mesh_instance_update_pose(nt_skeletal_mesh_instance_t *instance,
                                          const float *model_matrices,
                                          int matrix_count,
                                          char *error,
                                          size_t error_cap);

int nt_skeletal_mesh_instance_socket_matrix(const nt_skeletal_mesh_instance_t *instance,
                                            const char *socket_name,
                                            float *out_column_major_matrix_16,
                                            char *error,
                                            size_t error_cap);

void nt_skeletal_mesh_instance_draw(const nt_skeletal_mesh_instance_t *instance,
                                    const float *mvp_column_major_16);
```

Naming may change during implementation, but the ownership split should not:
animation clip sampling remains `nt_skeletal_anim_*`; skinned mesh rendering is
a separate mesh API.

## Skinning Rules

CPU v001:

- skin on CPU into dynamic vertex buffer;
- support up to 4 influences per vertex;
- normalize weights at conversion time;
- fail on missing joint mapping instead of silently drawing broken vertices;
- allow missing normals/uvs only when the material declares debug/color-only
  mode.

Implementation formula:

```text
skinned_position = sum(weight_i * (model_matrix[joint_i] * inverse_bind[joint_i] * bind_position))
```

The module must not invent animation timers. It consumes sampled Ozz matrices.
Game code owns state selection such as idle/mining/hit timing.

## Attachment Rules

Rigid attachments are v001.

Rules:

- sockets are defined in runtime mesh data by stable socket id/name;
- each socket references a joint name and local offset/orientation;
- missing socket or missing joint is an explicit error;
- socket transforms are computed from sampled model matrices, then socket local
  transform, then instance/world transform;
- pickaxe/tool attachment is the first proof target.

Skinned clothing/armor is v002+ unless needed to prove the renderer path.

## Performance Budget

T0007 v001 target:

- first-screen actor only;
- <= 5,000 skinned vertices;
- <= 32 joints;
- update only when the actor is visible;
- CPU skin update budget: <= 0.5 ms average on the native debug development
  machine for one actor;
- draw budget: one skinned character draw plus one rigid tool draw unless
  material slots require more.

If measured CPU skinning exceeds budget:

1. reduce proof mesh vertex count for v0.01;
2. cache unchanged poses when animation is paused;
3. queue GPU skinning task with measurement evidence.

Do not add GPU skinning speculatively before the CPU path is measured.

## Asset Conversion Boundary

For v001 the module can receive generated C arrays. The generator must be
project-specific or tool-specific, not Mine Cards symbols embedded in the
reusable runtime layer.

Allowed:

- generated C header with neutral `nt_skeletal_mesh_desc_t`;
- generated binary asset later;
- game-owned asset registration that passes a descriptor to the extension.

Not allowed:

- `mine_cards_*` symbols inside `extensions/skeletal_animation/src/`;
- direct source GLB parsing in the frame loop;
- hidden absolute paths to local downloads.

## Validation

Minimum proof for T0007:

- build target compiles with `SKELETAL_ANIMATION_OZZ_SOURCE_DIR` enabled;
- Ozz clip samples successfully;
- skinned mesh instance updates from `nt_skeletal_anim_copy_model_matrices`;
- pickaxe/tool socket resolves by name;
- native screenshot or capture shows KayKit/Quaternius-style skinned character
  plus attached tool in `game_seed` or Mining screen;
- `external/neotolis-engine` has no diff.

Suggested commands:

```powershell
cmake --build --preset native-debug --target game_seed
node ai_studio/taskboard/cli.mjs validate
node ai_studio/core_harness/validation/pipeline_validate.mjs --file extensions/skeletal_animation --file tasks/active/T0007-skeletal-extension-cpu-skinned-mesh-renderer-pat.md
```

If a native screenshot changes the player-facing screen, also run the relevant
product/readability gate before claiming the slice.
