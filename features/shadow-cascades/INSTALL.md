# Shadow Cascades Install

## Build

Resolve the in-place module from the Studio root:

```cmake
set(SHADOW_CASCADES_DIR "${GAME_REPO_ROOT}/features/shadow-cascades")
target_sources(game PRIVATE
    "${SHADOW_CASCADES_DIR}/src/csm.c"
    "${SHADOW_CASCADES_DIR}/src/frustum.c")
target_include_directories(game PRIVATE "${SHADOW_CASCADES_DIR}/include")
target_link_libraries(game PRIVATE nt_math)
```

## Compose

1. Fill a `csm_config_t` from the game's shadow settings, check it with
   `csm_config_valid`, and size the depth atlas with `csm_atlas_width` and
   `csm_atlas_height`.
2. Each frame, call `csm_compute` with the camera, then draw each cascade with
   `render_vp` into its `viewport`. Receivers read `light_view`, `lookup_scale`,
   `lookup_offset` and `tile`.
3. To cull, build `frustum_t` planes with `frustum_from_view_proj` and
   `frustum_from_light_view_proj`; for casters, add a `shadow_volume_t` from
   `shadow_volume_from_camera_slice`.

## Verify

```powershell
node features/validate_contracts.mjs
```

## Uninstall

Remove the source and include path, and the calls above.
