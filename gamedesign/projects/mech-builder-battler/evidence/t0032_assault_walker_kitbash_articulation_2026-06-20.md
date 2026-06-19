# T0032 Assault Walker Kitbash Articulation Pass

## Result

The sourced CC-BY Assault Walker now has visible attached mechanical parts that
make it read more like a modular Roblox-like mech build at gameplay size. This
does not replace the downloaded hero model; it adds runtime kitbash articulation
around it.

## What Changed

- Re-enabled selected starter mesh parts as overlays only when the Assault
  Walker hero is active.
- Added attached cannon, visor/vent, hydraulic, upper actuator, leg piston, and
  optional rocket module mount overlays.
- Tuned overlay placement and scale for the Assault Walker source model rather
  than the old procedural body.
- Animated overlays from existing movement, walk phase, recoil, heat, and
  rocket state.
- Added T0032 hangar, battle-entry, and battle movement screenshots to the
  DevAPI smoke.

## Screenshot Evidence

- Hangar mech read:
  `build/captures/mech_t0032_assault_kitbash_hangar_smoke.png`
- Battle entry read:
  `build/captures/mech_t0032_assault_kitbash_battle_entry_smoke.png`
- Battle movement read:
  `build/captures/mech_t0032_assault_kitbash_battle_smoke.png`
- Product gate:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T01-32-10_desktop-assault-kitbash-articulation.md`

## Asset Source Context

- Current hero source: Poly Pizza `Mech Assault Walker` by Alimayo Arango,
  CC-BY 3.0, already recorded in
  `gamedesign/projects/mech-builder-battler/references/hero_mech_asset_sourcing_2026-06-20.md`.
- This pass uses existing project-authored kitbash meshes as attachments on top
  of the sourced hero model. It does not introduce a new downloaded model.

## Validation

```powershell
cmake --build --preset native-debug --target game_seed
py -3.12 tools\mech-builder-battler\devapi_playable_smoke.py 9124
node tools\taskboard\cli.mjs validate
node tools\product_gate\review.mjs --project mech-builder-battler --task T0032 --surface desktop-assault-kitbash-articulation --screenshot build\captures\mech_t0032_assault_kitbash_hangar_smoke.png --verdict pass --strict --visual-strict ...
```

## Review Notes

- Strict visual gate passed with all visual rubric scores at 4.
- Remaining visual debt: this is still runtime kitbash animation, not a true
  rigged/source-authored mech animation pass. The next art slice should either
  download/integrate another permissively licensed mech/part source or generate
  authored standalone mech textures for metal, plastic, armor, vents, and studs
  world surfaces.
