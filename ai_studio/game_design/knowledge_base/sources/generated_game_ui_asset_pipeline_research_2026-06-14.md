---
type: Source Notes
title: Generated Game UI Asset Pipeline Research
description: External research notes for AI-generated game UI art, slice9/NinePatch metadata, atlas workflows, and runtime-ready asset production.
tags: [sources, ai-art, game-ui, ui-ux, slice9, atlas, pipeline]
checked: 2026-06-14
---

# Generated Game UI Asset Pipeline Research

## Research question

How do other projects and engines turn generated or drawn UI/game art into
runtime-ready assets instead of stopping at a pretty screenshot?

The specific failure to prevent is: generate a visually attractive screen, then
fail to cut it into reusable panels, icons, state variants, slice9 parts,
atlases, and native runtime proof.

## Source matrix

| Source | Observed facts | Take | Do not take |
| --- | --- | --- | --- |
| Unity 9-slicing manual: https://docs.unity3d.com/Manual/9SliceSprites.html | 9-slicing exists to resize one sprite instead of authoring many sizes. Corners stay fixed; borders stretch or tile on one axis; center stretches or tiles on both axes. | Require explicit slice margins and stretch/tile mode for every resizable panel/button. Preview at multiple sizes. | Do not stretch a whole generated panel uniformly and call it UI. |
| Android Draw 9-patch: https://developer.android.com/studio/write/draw9patch and NinePatch drawables: https://developer.android.com/guide/topics/graphics/drawables#nine-patch | Android stores stretch regions and content/padding regions in a 1-pixel border. The editor previews stretched output and warns about risky/bad patches. Stretchable regions should be at least 2x2 pixels, with 1px safe space to avoid interpolation artifacts. | Add content-safe rects, min stretch size checks, and preview/audit for bad patch artifacts. | Do not store only crop rectangles. Slice9 without content padding is incomplete for buttons and panels. |
| Phaser Nine Slice: https://docs.phaser.io/phaser/concepts/gameobjects/nine-slice | Nine Slice is explicitly for UI/buttons that expand to content while keeping corners fixed. Minimum width/height are constrained by side margins. | Validate min target size: width >= left + right and height >= top + bottom. | Do not allow slice margins that make requested button sizes impossible. |
| PixiJS NineSliceSprite: https://pixijs.com/8.x/guides/components/scene-objects/nine-slice-sprite | NineSliceSprite preserves corners/edges. Slice values can come from texture default borders. Resizing uses width/height geometry, not uniform scale. | Store slice borders in runtime manifests; render with native 9-slice geometry, not generic sprite scale. | Do not bake every button size as a separate image unless the visual style requires it. |
| TexturePacker docs: https://www.codeandweb.com/texturepacker/documentation and texture settings: https://www.codeandweb.com/texturepacker/documentation/texture-settings | Engine data files carry sprite positions, names, trimming, pivot points, 9-patch, and other metadata. Alpha bleeding reduces halo artifacts around transparent borders. Texture max/fixed sizes matter. | Runtime assets need metadata: name, rect, trim, pivot, slice9, alpha handling, texture limits. | Do not keep atlas/crop decisions only in chat or filenames. |
| Aseprite slices/sprite sheet docs: https://www.aseprite.org/docs/slices/ and https://www.aseprite.org/docs/sprite-sheet/ | Slices can name regions, define bounds, 9-slice internal centers, and pivots. Sprite sheet JSON can export slice data. Atlases are a standard runtime format. | Treat source sheets as sliceable production files with named regions and pivots. | Do not rely on eyeballed manual crop coordinates as the only source of truth. |
| ShoeBox by renderhjs: https://renderhjs.net/shoebox/ | Older production utility pattern: extract sprites from sheets, create sprite sheets, generate bitmap fonts, and author slice9 data as a practical asset-prep toolbox. Its value is not model generation; it assumes art must be cleaned, split, trimmed, packed, and previewed before runtime. | Copy the workflow shape: transparent/chroma extraction, component-based sprite isolation, trim/padding, sprite-sheet metadata, and separate slice9 authoring. | Do not copy the old desktop tool as a dependency unless explicitly needed; reproduce the deterministic steps in project scripts. |
| LibGDX TexturePacker: https://libgdx.com/wiki/tools/texture-packer | Texture packing includes whitespace stripping, padding, duplicate padding, edge padding, rotation controls, aliases, and metadata for how packed sprites map back to originals. | Generated runtime assets need trim, padding/extrusion, and metadata, not just image crops. | Do not accept zero-gutter generated sheets where adjacent antialias/shadow pixels bleed into another asset. |
| ybuild-ai AI game art pipeline skill: https://github.com/ybuild-ai/ai-game-art-pipeline-skill | The central rule is pipeline by runtime job, not model hype. It emphasizes post-processing scripts, target-device preview, and rejecting fully automated whole-game art sets without curation. | Adopt runtime-job-first planning, deterministic postprocess, small vertical slice, and QA at target size/device. | It is not enough for UI/slice9 by itself; it covers broad game art but has little dedicated game UI kit structure. |
| ComfyUI: https://github.com/Comfy-Org/ComfyUI | Node graph workflows can be saved/loaded as JSON and generated media can carry workflow/seed metadata. It supports complex repeatable workflows and production API use. | Record generator, seed, prompt, negative prompt, workflow JSON/path, and source refs for reproducibility. | Do not replace our runtime asset contract with a giant generation graph. |
| AUTOMATIC1111 Stable Diffusion WebUI: https://github.com/AUTOMATIC1111/stable-diffusion-webui | Common mature generation features include img2img, inpainting, outpainting, upscalers, prompt attention, correct seeds, and saved generation parameters. | Use inpaint/outpaint/upscale surgically; store generation parameters with artifacts. | A general image UI is not a game asset pipeline. Good images still require cleanup, metadata, and runtime proof. |
| mattwilliamson/comfyui-ai-gamedev: https://github.com/mattwilliamson/comfyui-ai-gamedev | Game asset nodes are modeled as multi-stage pipelines: background removal, mesh generation, texture painting, remeshing/optimization, examples, workflows, and tests. | Borrow the idea of explicit stages, example workflows, and tests around nodes/tools. | This repo is mostly 3D-heavy and should not define our 2D UI path. |
| quinteroac/ComfyUI-GameAssetsMaker: https://github.com/quinteroac/ComfyUI-GameAssetsMaker | Provides Aseprite-compatible atlas JSON, sprite previews, animation tags, VLM audit prompts, and VLM correction of generated spritesheet rectangles. | Add an audit/correction pass for crop rectangles and slice boxes. Build contact sheets/previews for human or VLM review. | Uniform frame assumptions do not solve slice9 UI panels; adapt the idea, not the exact schema. |
| NanoAlpha by MarkoUnity: https://github.com/MarkoUnity/NanoAlpha | Extracts transparent sprites from two pixel-aligned AI renders on light and dark backgrounds. It uses difference matting, configurable background colors, foreground guard, connected-component cleanup, alpha hardening, and local browser-only PNG export. | Add a future `dual_plate_alpha` path for difficult generated sprites/UI where chroma key spill is hard to remove. Require aligned light/dark plates, sampled background colors, blob cleanup, and alpha hardening evidence. | Do not assume this solves prompt inconsistency. The two plates must be the same image except background, so it needs img2img/control workflow discipline. |
| SPRITE paper/project: https://arxiv.org/abs/2604.18591 and https://baiyunshu.github.io/sprite.github.io/ | Game UI screenshot-to-runtime work uses VLMs plus a structured YAML intermediate representation to capture hierarchy, bounding boxes, assets, and interaction logic. | Store UI as a semantic component tree and asset manifest, not just a screenshot. | The project code is not available as a dependency; use the concept, not the tool. |
| GameUIAgent: https://arxiv.org/abs/2603.14724 | Uses a Design Spec JSON intermediate representation, deterministic post-processing, and VLM-guided reflection. Reports failure modes such as visual emptiness and rarity-dependent degradation. | Use structured design specs and reflection/audit checkpoints; define failure taxonomy before generation. | Figma-focused output is not enough for native game UI integration. |

## What seems to work

1. Runtime job first.
   Successful workflows classify the asset by how it is used in-game: resizable
   button, panel, icon, sprite frame, map tile, background layer, FX, character
   pose, or full-screen illustration. The generation method follows that job.

2. Structured intermediate data.
   Engines and UI reconstruction papers converge on the same idea: store named
   components, bounds, pivots, slice centers/borders, content padding, hierarchy,
   and target sizes. The source image is not enough.

3. Slice9 as metadata, not visual hope.
   Unity, Android, Phaser, PixiJS, TexturePacker, and Aseprite all treat 9-slice
   behavior as explicit metadata. Corners, one-axis edges, center behavior,
   content rect, and minimum size must be known before runtime use.

4. Preview before integration.
   Android's 9-patch editor, Aseprite/TexturePacker workflows, and AI pipeline
   skills all rely on previews/contact sheets. Generated art has to be viewed at
   target sizes, not just at source-image resolution.

5. Deterministic cleanup.
   Chroma removal, alpha bleeding, trimming, atlas packing, frame extraction,
   and compression are deterministic steps. They should be scripts/tools, not
   repeated manual improvisation.

5a. Sprite/icon extraction is a solved tool problem.
   ShoeBox, TexturePacker, LibGDX TexturePacker, and Aseprite workflows all
   assume the source art is processed into components before runtime use:
   remove transparent/chroma background, find the intended sprite/component,
   trim empty pixels, add padding/extrusion to protect edges, and emit metadata.
   Manual crop rectangles can seed this process, but cannot be the only gate.

6. Reproducible generation.
   Mature AI art tools preserve seeds and parameters. ComfyUI-style workflows
   preserve node graphs. For game art, this should be attached to the
   game-owned request packet or asset manifest.

7. Audit and correction loop.
   ComfyUI-GameAssetsMaker's VLM audit/correction pattern and GameUIAgent's
   reflection controller both suggest that generated UI assets need a separate
   verification pass for geometry and semantic correctness.

8. Dual-plate alpha extraction for hard mattes.
   NanoAlpha shows a practical alternative to chroma screens: ask the generator
   for the same subject on a light and dark plate, then derive alpha from the
   per-pixel difference. This avoids baking a saturated key hue into antialias
   edges, but only works if the two plates are aligned and visually identical
   except for the background. It should be a pipeline mode for difficult icons,
   character sprites, ornate UI overlays, and any source family that repeatedly
   fails chroma fringe audits.

## What does not work

1. Pretty screenshot as final asset.
   It hides all production problems: baked text, fused icons, inconsistent
   states, uncuttable panels, unusable stretch zones, and wrong target scale.

2. One prompt for the whole UI kit.
   The model tends to blend panels, icons, ornaments, labels, and state colors
   into one image. The result is visually rich but hard to reuse.

3. Manual crops without a manifest.
   Coordinates in chat or temporary notes cannot be validated, regenerated, or
   audited. The crop manifest is part of the asset.

4. Slice9 without content padding.
   A resizable button/panel also needs a text/icon safe area. Otherwise runtime
   text collides with borders or decorative corners.

5. No negative constraints.
   Without explicit negative prompts and rejection checks, generated sheets
   include fake text, unreadable glyphs, fused labels, random decorative symbols,
   and inconsistent icon scales.

6. Over-automation.
   Existing public skills explicitly warn against fully automated whole-game art
   production without curation. The right unit is a small vertical slice and a
   checked reusable kit.

7. Web-only proof for native target.
   Browser previews can help inspect a sheet, but they do not prove the native
   game runtime path. Native screenshot proof is required once implementation
   starts.

## Required workflow for our skill

1. Define target screen and runtime jobs.
   Example: map background layer, location marker icons, rescue quest icon,
   choice button slice9, modal panel slice9, status bar slice9, icon states.

2. Define art bible before generation.
   Include palette, line weight, material vocabulary, corner radius language,
   icon silhouette rules, lighting, border thickness, rarity/state color rules,
   and forbidden motifs.

3. Generate source families, not one giant screenshot.
   Recommended families:
   - UI kit source sheet: blank panels, buttons, tabs, bars, dividers, frames.
   - Icon source sheet: square icons with gutters, consistent silhouette size.
   - World/map source: background layers and landmark stamps.
   - FX/sprite source: isolated animation frames or one-off effects.

4. Require no baked text in generated UI assets.
   Text, numbers, debug values, quest names, prices, and player state belong to
   code. Generated assets can include intentional symbols only when declared as
   icons.

5. Create crop/slice manifest.
   Each asset entry needs: id, kind, source image, rect, output path, pivot if
   relevant, slice9 margins if resizable, content padding, min size, target
   preview sizes, state role, and notes.

6. Build runtime manifest.
   The crop manifest is source-production data; the runtime manifest is what
   code consumes. Runtime entries should not depend on editor-only assumptions.

7. Run validator in draft mode before generation.
   Catch missing target jobs, missing slice9/icon/sprite expectations, and
   prompts that allow baked labels or fused elements.

8. Run strict validator after cutting.
   Catch missing files, invalid rects, missing slice9 margins/content padding,
   impossible min sizes, missing runtime asset outputs, and missing previews.

9. Preview and audit.
   Generate a contact sheet of crops, slice9 stretched previews at minimum,
   normal, and large sizes, and a native screenshot once integrated.

10. Close with evidence.
   Store source art, prompts/params, manifests, contact sheets, previews, native
   screenshots, product gate result, and task log references.

## Validator requirements

The current validator direction is right but incomplete. It should enforce:

- a game-owned request/manifest schema when the game needs one
- explicit `visual_targets`
- `reusable_kinds` includes at least one runtime UI kind when UI is requested:
  `slice9`, `icon`, `sprite`, `map_tile`, `panel`, `button`, `bar`
- `must_not_bake` includes text/state/value categories
- expected `crop_manifest`, `runtime_manifest`, and `runtime_dir`
- for strict mode, existing source images
- crop entries with `id`, `kind`, `source`, `rect`, and `output`
- slice9 entries with `slice9` margins, `content` padding/safe rect, and
  target preview sizes
- icon entries with size class and state/semantic role
- sprite entries with pivot/anchor and frame metadata
- runtime entries with source asset path and metadata consumed by code
- preview evidence path for contact sheets and stretched slice9 checks

## Immediate implications for Rune Marches / next game UI pass

- Do not generate one full fantasy RPG HUD and try to cut it afterward.
- Generate a small UI kit first: one modal panel, one button family with states,
  one status bar, one icon frame, one map marker style, and one parchment/map
  background layer.
- Generate icon sheets with deliberately large gutters. Tight adjacent icon
  shadows or antialiasing cause false component merges and must be rejected at
  source-selection time.
- Keep every button blank. Put labels in code.
- Design for mobile target sizes at source time: touch targets, icon
  silhouettes, and readable contrast must be checked in preview.
- Create source art under the project folder, temporary rejected outputs under
  `tmp/`, durable accepted manifests/assets under the active project folder.
- Native PC screenshot remains the proof for playable game work.

## Follow-up: crop/fringe failure found in Rune Marches V2

The first Rune Marches UI kit pass still had visible extraction failures after
strict validation: the silver icon was partially cropped, some expanded icon
rectangles captured neighboring icon fragments, and magenta/purple fringe was
visible around buttons/icons. The root cause was accepting manual crop
rectangles plus a contact sheet without enough geometric assertions.

The revised extraction rule:

- Chroma removal should be border-connected, so intentional interior purple
  effects are not destroyed.
- Icons should use a wider source capture only when there are safe gutters.
- Icon outputs should isolate the intended component, preferably center-biased
  for centered icon sheets.
- Icon outputs should trim to alpha bounds and add padding after isolation.
- Key-color edge fringe should be removed at transparent borders.
- Strict validation should require icon trim/padding and component isolation
  policy, or an explicit recorded exception.
- Contact sheets must be inspected as a production gate before runtime
  integration. If the sheet shows clipped silhouettes, neighboring fragments,
  or key-color outlines, the game-owned request is not done.

## Follow-up: purple halo after chroma removal

The Rune Marches blank UI bases still showed a faint purple outline even after
visible key-color pixels were removed. The root cause was not only opaque
magenta pixels. Transparent pixels near the edge still carried magenta/purple
RGB from the chroma background, and resize/filtering could sample those RGB
values back into visible edges.

External production patterns match this diagnosis:

- Godot's texture importer has `process/fix_alpha_border`, enabled by default,
  to put surrounding colors into the transparent-to-opaque transition and
  reduce outline artifacts with bilinear filtering. It also documents
  premultiplied alpha as an alternative border fix path.
- LibGDX TexturePacker's `bleed` option sets RGB values for transparent pixels
  from nearest non-transparent pixels specifically to prevent filtering
  artifacts; its docs recommend more bleed iterations such as 4 or 8 when
  downscaling still shows artifacts.
- TexturePacker's `extrude`, border padding, and shape padding settings exist
  to repeat border pixels and prevent neighboring/transparent-border bleeding
  artifacts.

Revised cleanup rule:

- Build the alpha matte from border-connected key/background pixels.
- Remove or decontaminate visible key-colored edge specks.
- Repair visible purple-contaminated pixels in UI families that have no
  intentional purple/magic color.
- Bleed RGB from nearest non-transparent/non-key edge colors into transparent
  pixels while leaving alpha transparent.
- Resize previews and slice9 tiles in premultiplied-alpha space.
- Audit both visible purple halo and dangerous transparent-edge RGB, because a
  PNG with alpha=0 can still produce a purple outline after filtering if its
  transparent RGB remains magenta.

Source intake rule added after the halo fix:

- The key background color itself must be validated against the art palette.
  A saturated magenta background is useful only if the generated components do
  not use exact key-colored pixels internally and do not contain a large
  key/halo-colored hue band.
- Exact key-color holes inside component bounds are source-sheet failures, not
  normal removable background, because a connected-component slicer will treat
  them as holes and a global chroma key can destroy intended art.
- Small amounts of related hue can be legitimate in shadows or material ramps,
  so the gate records ratios instead of banning all purple. Families that truly
  need magic/purple art should use an explicit preserve/masking policy or a
  safer chroma/background color.
- Candidate key colors should be scored against the component palette before
  regeneration. On the current Rune Marches V2 UI bases, green scored safer
  than magenta because the art contains more red/blue/purple material and
  shadow ramps than green.
- A remaining one- or two-pixel dark purple, dark maroon/magenta, or red-blue
  line on the outer alpha contour is still an extraction failure. Bright
  magenta-only audits are too weak; the pixel gate must also catch very dark
  purple halo pixels such as `#400040` and muted edge-spill pixels like
  `(55, 20, 45)` or near-black purple pixels such as `#26022d` when they touch
  transparency.
- Normal contact sheets are not enough evidence for this class of defect. The
  pipeline now needs an edge-proof preview that shows zoomed
  top/right/bottom/left alpha-boundary strips on checkerboard and marks any
  detected bad edge pixels. Accepted proof images should be recorded as durable
  game-owned evidence, not kept only as temporary viewer screenshots.
- The audit must be source-key aware. Switching from magenta to green is not
  enough if the cleanup and pixel audit still only understand magenta/purple
  halos. A green-screen source can leave visible green chroma spill on the
  alpha contour and hidden green RGB in transparent pixels. Both are failures
  and should be counted against the actual `green_screen.key` in the crop
  manifest.
- For source families that keep producing key-color spill after cleanup, create
  a game-owned dual-plate prompt note instead of endlessly retuning a single
  chroma key:
  one light-background plate, one dark-background plate, same dimensions,
  same composition, no baked text, and a deterministic alpha extraction step
  with blob cleanup and alpha hardening.
- The next generation prompt should be compiled from the game-owned request and
  intake findings. A reusable prompt note should carry source family role,
  no-bake rules, slice9 restrictions, gutters, background/chroma choice, and
  acceptance checklist so the next source sheet does not repeat the same
  fused-UI/chroma/stretch-zone mistakes. Accepted source provenance should
  reference that note so traceability runs from request -> prompt -> source ->
  crop -> runtime audit.

## Follow-up: procedural scaffold failure found in Rune Marches V2

After fixing crop and slice9 geometry, the rescue pass briefly replaced the
generated ornate UI bases with procedural/programmer-art panels. This made the
technical slice9 preview cleaner, but the lead correctly rejected it as not
generated art and visually too poor.

The revised rule:

- Procedural UI art may be used only as a debug scaffold to prove geometry,
  min-size behavior, content safe areas, and overlay anchors.
- A procedural scaffold cannot close a generated-art task unless the
  game-owned request records an explicit exception accepted by the lead.
- Final generated UI bases must come from generated or artist-authored source
  families. Code may cut, validate, pack, and compose those assets, but should
  not replace them with two-color programmer art.
- If generated panels are too ornate for slice9, regenerate or edit layered
  source families: blank stretchable bases, repeatable edge strips, corner
  caps, and separate overlay decor. Do not flatten everything into one
  uncuttable sheet, and do not solve it by throwing the art away.
- Each accepted source family needs game-owned provenance: provider/model or
  workflow, workflow file/json when available, seed or no-seed reason, prompt,
  negative prompt, source family role, accepted image path, and rejected
  candidate notes.
- A manifest-level policy is not enough. The Rune Marches builder briefly
  recorded trim/component policy but still called the old direct crop path in
  `main()`. The pixel audit caught the mismatch because runtime PNG alpha
  bounds were still too close to output edges. Future pipelines need both
  contract validation and pixel validation against generated files.

## Process decision

This should become a workflow split across two skills:

- `game-visual-art-direction`: owns art bible, visual target, source generation
  requests, rejection taxonomy, and art review.
- `game-asset-pipeline`: owns crop/slice/runtime manifests, post-processing,
  packing, validator, previews, and runtime proof.

A new dedicated micro-skill could be added later if the split becomes too large:
`generated-game-ui-assets`. For now, updating the two existing project skills is
lower overhead and keeps the responsibilities close to current repo structure.
