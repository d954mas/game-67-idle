# Graphics Quality

## Purpose

The base graphics settings every 3D game offers: a two-step Graphics quality
(High, Low) and the rule for the step a device starts on before the player
picks one. Weak phone GPUs are pixel-bound at full resolution (a Mali-G76 at
720x1242 spends a quarter of its frame on the ground shader alone), so Low draws
the world into a smaller target and keeps the UI at the window's resolution.
Low also starts the player without shadows.

The persisted choice and the settings rows live in the `settings` feature
reference (`ai_studio/feature_testing/consumer/src/features/settings/`); this
module is the policy both read.

## Public surface

`include/features/graphics_quality/graphics_quality.h`:

- `graphics_device_query()` — on the web, the canvas's unmasked WebGL renderer
  and whether the page runs on a phone or tablet; natively, a computer with no
  name.
- `graphics_quality_guess(mobile, gpu)` — the starting step: phones Low unless a
  recent flagship GPU (Apple, Immortalis, Xclipse, Mali-G710 and later, Adreno
  730 and later); computers High unless a software renderer or an old Intel HD.
- `graphics_world_scale(quality, mobile)` — the world target's size against
  the window: 1 at High; a half on a phone and two thirds on a computer at Low.
- `graphics_shadows_default(quality)` — shadows start on at High only.
- `graphics_quality_name` / `graphics_quality_parse` — `"high"`, `"low"`.

## Layer

L0: pure C plus one `EM_JS` page read. No engine, UI or save dependency; the
game applies the scale and the shadows to its own renderer.

## Validation

`test_graphics_quality` (CTest label `core`) in the feature consumer, and
`node features/validate_contracts.mjs`.

## Compatibility

`feature.json.version` is exact SemVer. PATCH changes the guess tables or the
scale numbers without changing the surface; MINOR adds API or a step; MAJOR
renames or removes a step or a function, or changes a persisted name.

## Extension points

A game that measures its own frame may override the starting step after the
guess (for example from a frame-time sample on first run) and must still keep
a step the player picked. The world scale is advisory: a renderer without an
offscreen world target may treat Low as its own cheaper path.
