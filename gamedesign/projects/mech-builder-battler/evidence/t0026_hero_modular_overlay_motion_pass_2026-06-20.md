# T0026 Hero Modular Overlay Motion Pass

## Result

The hero mech no longer uses the orange shape-rendered hardpoint circles over
the downloaded CC0 Quaternius body. Visible module read now comes from
mesh-rendered rails, vents, sockets, upper armor plates, and hydraulic-style
parts using a dedicated solid-lit mesh material.

This is still a transitional asset-first pass: the downloaded GLB remains the
hero base, and the next larger visual slice should source or author a stronger
modular/rigged Roblox-like mech instead of adding more kitbash overlays.

## Visual Changes

- Added a solid mesh material/shader path for mesh parts that do not have UVs.
- Rendered selected modular overlays as real mesh renderer items with depth,
  lighting, color, and sort/batch keys instead of shape debug primitives.
- Removed the previous shape-rendered hardpoint overlay that created orange
  wire circles on the hangar screenshot.
- Added subtle idle/recoil/movement offsets to upper module rails, vents, and
  hydraulic pieces so the mech reads less static during WASD movement.
- Culled lower floating plate overlays after screenshot review because they
  did not align with the downloaded GLB silhouette.

## Evidence

- Native build:
  `cmake --build --preset native-debug --target game_seed`.
- DevAPI smoke:
  `py -3.12 tools/mech-builder-battler/devapi_playable_smoke.py 9124`.
- Hangar screenshot:
  `build/captures/mech_t0026_hero_modular_overlay_hangar_smoke.png`.
- Movement screenshot:
  `build/captures/mech_t0026_hero_modular_motion_smoke.png`.
- Smoke log:
  `build/logs/native_devapi_9124_20260620_001035_797.log`.
- Strict product gate:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T00-10-35_desktop-hero-modular-overlay.md`.

## Remaining Gap

- The hero is improved, but it is not the final visual bar for a mech game.
- The next priority should be true downloaded/third-party model sourcing:
  CC0/CC-BY Roblox-like or low-poly modular mech/robot candidates with clear
  license/provenance, then replacing or kitbashing the current hero base.
- Texture work for the next asset should explicitly decide whether each texture
  is tileable, unique asset material, mech material, or decal before runtime
  integration.
