# Render a library glb in-engine (pack → load)

Concrete recipe for putting a real library model on screen instead of the debug
shape renderer. Engine is read-only — public API only. Working templates:
`external/neotolis-engine/examples/atlas/{build_packs.c,main.c}` (single mesh +
runtime load) and `external/neotolis-engine/examples/sponza` (multi-primitive scene).

## 1. PACK (build time, a native pack-builder)

Create a pack builder — copy `examples/atlas/build_packs.c` to `src/build_packs.c`
and wire it as a CMake custom command (mirror that example's CMakeLists) so it runs
on build, emitting `<game>.ntpack` + a generated `*_assets.h` of asset ids.

- Stream layout MUST match the `mesh_inst` shaders (2 streams, no normal):

      NtStreamLayout layout[] = {
        {"position", "POSITION", NT_STREAM_FLOAT32, 3, false},
        {"uv0", "TEXCOORD_0", NT_STREAM_FLOAT32, 2, false},
      };

  with `tangent_mode = NT_TANGENT_NONE`.
- Pack the shaders + a neutral texture once: `nt_builder_add_shader` for
  `assets/shaders/mesh_inst.vert` / `.frag`, and a 1x1 white texture via
  `nt_builder_add_texture_raw(ctx, white_rgba, 1, 1, "little_lives/white", &opts)`.
  The frag is `texture(u_texture, v_uv) * v_color`; library glbs are untextured,
  so colour is delivered per-instance and the texture stays white.
- Single-primitive glb: `nt_builder_add_mesh(ctx, "assets/meshes/x.glb", &(nt_mesh_opts_t){.layout=layout, .stream_count=2, .tangent_mode=NT_TANGENT_NONE})`.
- Multi-primitive glb (most furniture — engine issue #248: scene import reads only
  `primitives[0]`'s material): use the scene API instead of `add_mesh`:

      nt_glb_scene_t sc; nt_builder_parse_glb_scene(&sc, "assets/meshes/bed.glb");
      for each mesh mi, for each primitive pi:
        snprintf(rid, "...mesh/bed/%u_%u", mi, pi);
        nt_builder_add_scene_mesh(ctx, &sc, mi, pi, rid, &opts);   // one resource per primitive
        // record sc.materials[sc.meshes[mi].material_index].base_color[4] for rid
      nt_builder_free_glb_scene(&sc);

## 2. LOAD (runtime, in the game — e.g. `src/clean_seed_main.c`)

The required engine libs are already linked. Mirror `examples/atlas/main.c`.

- Register activators once: `nt_resource_set_activator(NT_ASSET_MESH, nt_gfx_activate_mesh, nt_gfx_deactivate_mesh)` and the same for `NT_ASSET_TEXTURE`.
- Init component systems + the renderer: `nt_entity_init`, `nt_transform_comp_init`,
  `nt_mesh_comp_init`, `nt_material_comp_init`, `nt_drawable_comp_init`,
  `nt_mesh_renderer_init`.
- Request resources (ids come from the generated `*_assets.h`):
  `nt_resource_request(ASSET_MESH_..., NT_ASSET_MESH)`, the two `mesh_inst` shaders,
  and the white texture.
- Create the material: `nt_material_create` with vs/fs = mesh_inst, texture
  `u_texture` = white, `attr_map` position→0 / uv0→1, `color_mode = NT_COLOR_MODE_FLOAT4`.
- Create one entity per mesh (per primitive for multi-material): add transform +
  mesh + material + drawable comps; set per-instance colour via
  `nt_drawable_comp_set_color(ent, r, g, b, a)` (use the recorded baseColorFactor).
- Each frame, in the 3D pass (after the shape flush so GL state doesn't interleave):
  set the mesh handle, update transforms, build `nt_render_item_t` with
  `nt_sort_key_opaque(mat, mesh)` + `nt_batch_key(mat, mesh)`, then
  `nt_mesh_renderer_draw_list(items, n)`.
- On context restore: `nt_resource_invalidate(NT_ASSET_MESH/_TEXTURE)` +
  `nt_mesh_renderer_restore_gpu()`.

## Build / verify

    cmake --build --preset native-debug          # re-packs + recompiles
    build/game_seed/native-debug/game_seed.exe    # run

Screenshot-verify (qualitative, never pixel-diff): the model renders in the 3D
scene, correctly depth-sorted and tinted; for furniture, multi-material colours
read right. Prove the single cube first, then swap to a real multi-primitive glb.
