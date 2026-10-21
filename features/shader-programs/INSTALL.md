# Shader Programs Install

## Build

Resolve the in-place module from the Studio root. Keep the path literal: the
game build scans it to check `dependencies.json`.

```cmake
set(SHADER_PROGRAMS_DIR "${GAME_REPO_ROOT}/features/shader-programs")
target_sources(game PRIVATE "${SHADER_PROGRAMS_DIR}/src/shader_programs.c")
target_include_directories(game PRIVATE "${SHADER_PROGRAMS_DIR}/include")
```

Add `{"id": "shader-programs", "source": "features/shader-programs", "version": "0.1.0"}`
to the game's `dependencies.json`.

## Compose

1. Create materials with `shader_program_material(vs, fs, &desc)`; skip a draw
   while `shader_program_ready(material)` is false.
2. Every frame while the context is live, call `shader_programs_update()` after
   `nt_resource_step()`.
3. On context restore: `nt_resource_invalidate(NT_ASSET_SHADER_CODE)`, then
   `shader_programs_restore_gpu()`, and draw nothing that frame.
4. Before `nt_gfx_shutdown`, call `shader_programs_shutdown()`.

## Verify

```powershell
node features/validate_contracts.mjs
```

## Uninstall

Remove the source and include path, the calls above and the dependency entry.
