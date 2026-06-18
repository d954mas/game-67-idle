# Ozz KayKit Runtime Proof

Date: 2026-06-17

Status: review proof, not final character art.

## Decision

The procedural/blockout miner proof was rejected as debug art. The first native
visual proof now uses real kit sources:

- character source: KayKit Character Animations 1.1,
  `tmp/mine-cards/external/kaykit/free_1_1/KayKit_Character_Animations_1.1/Animations/gltf/Rig_Medium/Rig_Medium_Tools.glb`;
- character license: KayKit local `License.txt`, CC0;
- animation: KayKit `Pickaxing`, converted to Ozz archives under
  `gamedesign/projects/mine-cards/visual/skeletal_spike/ozz_runtime/`;
- tool source: local kit `Pickaxe_Stone.gltf` from
  `C:\Users\ROG\YandexDisk\gamedev\assets\my\tanki\Cube World - Aug 2023\Tools\glTF\`;
- runtime bridge: generated C mesh data plus game-side CPU skinning driven by
  `extensions/skeletal_animation/` model matrices.

## Runtime Path

```text
KayKit GLB + kit pickaxe GLTF
-> tools/assets/generate_kaykit_skinned_mesh_header.py
-> src/mine_cards_kaykit_mesh.gen.h
-> extensions/skeletal_animation samples Ozz clip
-> src/clean_seed_main.c copies Ozz model matrices
-> src/mine_cards_model_proof.c CPU-skins vertices into a dynamic VBO
-> native game_seed render proof
```

No files under `external/neotolis-engine` are edited.

## Evidence

- GIF: `build/captures/mine_cards_ozz_kaykit_miner_v2_animation.gif`
- Sheet: `build/captures/mine_cards_ozz_kaykit_miner_v2_animation_sheet.png`
- Native run log: `build/logs/native_devapi_9143_20260617_185908_075.log`
- Build: `cmake --build --preset native-debug --target game_seed`

## Limits

- This is a real skinned kit mesh proof, not final Mine Cards art.
- The CPU-skinned renderer is local proof code. T0007 tracks turning it into a
  reusable production module with asset format, material handling, attachments,
  and performance budget.
- The pickaxe is visible and socket-driven, but its final orientation/material
  pass belongs to the production art iteration.
