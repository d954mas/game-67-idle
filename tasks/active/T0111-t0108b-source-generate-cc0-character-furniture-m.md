---
id: T0111
title: "T0108b: source/generate CC0 character + furniture meshes and wire lit mesh pipeline"
status: todo
epic: E007
priority: P2
tags: [little-lives, art, meshes, cc0]
created: 2026-06-22
updated: 2026-06-22
---

## What

Source CC0 3D models for Little Lives and render them in-engine, replacing
procedural shapes. Furniture first; characters + a textured/lit pipeline next.

## Done when

- [x] CC0 furniture models sourced + vendored with license/provenance.
- [x] Furniture renders as real model geometry in the 3D scene.
- [x] Furniture shows authentic per-material colours (Kenney Kd palette).
- [ ] Sims render as a real character mesh (needs the rigged pipeline; see below).
- [ ] Textured/lit mesh pipeline (only needed for textured/character art).

## Open questions

- Character source: Kenney "Mini/Blocky Characters" or Quaternius (CC0). Static
  mesh loses the humanoid walk animation — prefer a rigged/animated path later.

## Log

- Sourced Kenney **Furniture Kit** (CC0) from archive.org
  (`kenney_furniturePack.zip`); vendored the OBJs + Kenney `License.txt` under
  `assets/source/models/kenney/furniture_kit/`.
- Engine builder is glTF/glb-only and `nt_shape_renderer_mesh` takes raw
  positions+indices, so chose the low-risk path: `tools/little-lives/obj_to_header.py`
  parses + normalizes the OBJs (center X/Z, sit on floor, scale to footprint) into
  `src/ll_meshes.h` (bed/fridge/shower/toilet/sofa/desk).
- Runtime: `draw_object` CPU-transforms the base mesh (yaw toward room + translate)
  into a scratch buffer and draws via `nt_shape_renderer_mesh`, flat-tinted; one
  batch flush after furniture bounds the vertex buffer. Use-spots unchanged so
  gameplay is intact. Evidence:
  `gamedesign/projects/little-lives/reviews/furniture_meshes.png`.
- UPDATE: converter now splits each model by material (Kenney `Kd` per usemtl);
  `draw_object` draws each sub-mesh with its colour -> authentic multi-colour
  furniture (bed = wood frame + coral blanket + white pillows, etc.). The shape
  renderer face-shades + I modulate by day/night, so meshes look lit. Kenney's
  flat-material art needs no PBR textures — this matches the intended look.
  Evidence: reviews/furniture_meshes.png, reviews/city_meshes_overview.png.
- CHARACTER findings: Kenney `animated-characters-3` (CC0, vendored license) is
  FBX + skin PNG + FBX anim clips — no OBJ/glTF. A usable Sim character needs:
  FBX->glTF conversion + a textured mesh material + skeletal animation (engine
  `extensions/skeletal_animation` / ozz). A *static* T-pose mesh would be a
  downgrade vs. the current animated blocky humanoid for a walking life-sim.
  => Recommend keeping animated humanoid Sims; do the rigged/textured/animated
  character as its own milestone (large) only if the lead wants photoreal Sims.
