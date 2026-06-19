# T0030 Mech Lighting And Material Pop Pass

## Result

The sourced Assault Walker and robot enemies now have stronger material and
lighting response. The mesh shaders use the existing normals for stronger
rim light, specular highlights, fill light, top light, and subtle ground bounce
so the hero reads more like glossy toy/plastic/metal instead of flat colored
geometry.

This is a shader/material tuning pass, not final authored PBR texture work.

## What Changed

- Tuned `mech_mesh_color_inst.frag` for stronger toy-color saturation, rim
  light, specular highlights, top light, and ground bounce.
- Tuned `mech_mesh_solid_inst.frag` so solid mesh overlays keep the same
  brighter material language.
- Tuned `mech_mesh_inst.frag` so textured mesh paths stay consistent with the
  new lighting response.
- Added T0030 hangar and battle screenshot captures to the DevAPI smoke.
- Reordered the smoke battle captures so WASD movement is checked before the
  fast first battle reaches the reward overlay.

## Evidence

- Native build:
  `cmake --build --preset native-debug --target game_seed`.
- DevAPI smoke:
  `py -3.12 tools/mech-builder-battler/devapi_playable_smoke.py 9124`.
- Smoke log:
  `build/logs/native_devapi_9124_20260620_010745_030.log`.
- Battle screenshot:
  `build/captures/mech_t0030_lighting_material_battle_smoke.png`.
- Hangar screenshot:
  `build/captures/mech_t0030_lighting_material_hangar_smoke.png`.
- Strict product gate:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T01-08-20_desktop-mech-lighting-material-pop.md`.

## Remaining Gap

- Final-quality material detail still needs authored textures, surface breakup,
  or generated/source texture work.
- Heavy battle VFX can still make the hero silhouette busy; later camera/layout
  polish should keep the mech cleaner during action.
- Web/mobile export remains deferred until explicitly approved.
