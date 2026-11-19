---
name: nt-blender-production
description: "Create, render, audit, or hand off Blender scenes and models; shared bpy/GLB library, one-command review cards, mesh/export/contact/clip checks."
---

# NT Blender Production

Ready tools live in `ai_studio/assets/tools/blender/` (its README lists every
command). They exist to save time: use what helps, write your own when it does not.

## Ready Tools

Look at and check any GLB in one call, from the Studio root or a game repo (paths resolve
against the folder you call from; folders and `*.glb` globs expand):

```powershell
node ai_studio/dev_environment/python_run.mjs ai_studio/assets/tools/blender/review.py assets/models/crate.glb
```

Then open `tmp/review/<stem>/card.png` with Read: views, triangle counts, textures and
findings on one image. The terminal already lists every finding with a fix hint.

- Rendering starts Blender, which the agent sandbox blocks: run it outside the sandbox, or add
  `--no-render` to get the checks alone in milliseconds.
- `--game-camera '{"height":1.8,"behind":2,"pitch":16,"fov_y":55}'` adds the game's view,
  `--game-crop shot.png` puts the in-game screenshot on the card, `--turntable 8`,
  `--animation Walk --time 0.4`, `--pivot floor`, `--floating`, `--contacts c.json`.
- Single checks (`mesh_check.py`, `glb_check.py`, `contact_check.py`, `clip_check.py`) take
  the same paths; they advise by default and `--strict` turns any of them into a gate.
- `run_blender.py <script.py> -- <args>` runs your own build script in headless Blender 5.2
  with `from ai_studio.assets.tools.blender.lib import bpy_scene, bpy_io, glb, color`.
- `lib/glb.py` reads, poses and writes GLB without Blender; `lib/color.py` `PaletteAtlas`
  allocates palette rows and refuses to overflow.

## What Has Helped Before

Suggestions from `ai_studio/assets/tools/blender/production_lessons.md`; use what fits the asset:

- Show a cheap fake shot from the game camera before building; style misses cost more
  than any technical defect.
- Judge an asset next to shipped neighbours and the last approved round, at the game
  camera, with the in-game crop on the card.
- For characters, start from a rigged, professionally made base and adapt it; swap a
  base rather than sculpting a pack mesh locally.
- Keep shape recipes in the game and import the shared library instead of copying
  scripts between games.
- Contact and clip checks catch floating parts, foot slide and broken seated poses before the
  lead sees them.

## Full Gate (When Wanted)

For hero scenes, named-reference work, propagation or handoff where fresh-hash evidence
matters, the README's Stages 0–6 and `gate.py` give a fail-closed review loop with
independent visual and technical lanes. Character proof guidance:
[character guidance](references/characters.md). Named-reference style work uses
`nt-design-knowledge`.

## Completion

Report what was built (paths), the review card, check results that matter, and what
remains open. Keep render success, technical checks and style approval as separate
claims: a clean check does not mean the lead will like the look.
