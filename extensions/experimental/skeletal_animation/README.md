# Experimental Skeletal Animation

Status: incomplete proof, quarantined. This directory is not a production
feature, is not part of the default template/package/CI surface, and has no
game-ready lifecycle or performance guarantee.

## Actual Capability

- Loads compatible Ozz skeleton and animation archives and samples Ozz poses.
- Exposes sampled model-space joint matrices and joint-name lookup.
- Implements a CPU-only skinned-position and socket-transform proof.
- Does not implement rendering, GPU buffers, renderer ownership, or a complete
  skeleton-to-mesh joint mapping contract.
- Has no measured production performance claim.

The former public draw function silently did nothing. It was removed during
quarantine; this extension exposes no draw operation.

## Explicit Opt-In

This directory is never added by the root template or game build. A developer
must add it deliberately and configure both required values:

```powershell
cmake -S extensions/experimental/skeletal_animation -B <build-dir> `
  -DNT_ENABLE_EXPERIMENTAL_SKELETAL_ANIMATION=ON `
  -DSKELETAL_ANIMATION_OZZ_SOURCE_DIR=<ozz-source>
cmake --build <build-dir>
ctest --test-dir <build-dir> --output-on-failure
```

The CMake target and probe names start with `experimental_`; compile and MSVC
runtime flags are target-local. Probe binaries stay in the extension build
directory. Ozz test assets are caller-supplied; no local asset path is assumed.

## Boundary

Read [known-defects.md](docs/known-defects.md) before any use. The retained
checks preserve the proof and quarantine evidence; passing them does not claim
that the listed runtime defects are fixed. Do not ship this code or expose its
headers as a reusable production feature.

No engine change, GPU-skinning project, asset importer, or renderer completion
belongs in this quarantine task.
