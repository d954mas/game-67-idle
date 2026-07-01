---
type: Art Pipeline Knowledge
title: AI Art Iteration Pipeline
description: Reusable workflow for moving generated art into runtime assets.
tags: [art, pipeline, ai, assets]
timestamp: 2026-06-13T00:00:00Z
---

# AI Art Iteration Pipeline

Reusable workflow for turning generated art into game-ready runtime assets
without slow manual loops.

## Goal

AI art should move through a short, inspectable loop:

```text
visual target -> art request packet -> generated sheet -> crop/slice manifest
-> explicit pack build -> native runtime screenshot -> review notes
```

The agent should not rely on chat-only crop coordinates, ad hoc temporary
files, or one-piece screenshot art when the game needs reusable components.

For any non-trivial generated art pass, treat this as a game-owned request
packet, not a loose conversation. The game owns the source paths, candidate
policy, crop/slice9 data, runtime manifest, pack command, and native evidence
path. AI Studio no longer provides a shared generated-art scaffold; write
the packet next to the current game's design/assets metadata when the game
needs one.

## What Other Pipelines Suggest

- UI systems treat resizable art as structured components. Unity 9-slicing keeps
  corners fixed while edges and centers stretch or tile; Godot NinePatchRect
  uses the same 3x3 idea and exposes patch margins as data.
- Runtime asset systems separate content builds from player builds. Unity
  Addressables documents content builds and content-only update builds so asset
  changes can be processed without treating every change as a full app rebuild.
- Sprite atlases exist to pack many textures into fewer runtime textures and
  draw calls. This argues for an explicit atlas/pack step, not direct scratch
  PNG references.
- AI art production platforms such as Scenario emphasize custom/reference
  models, art bibles, side-by-side comparison, batch generation, APIs, MCP, and
  reusable workflow templates. The reusable pattern is: stable style source,
  repeatable generation packet, batch candidates, then automated integration.
- Agent harness best practice is to turn repeated failures into source-of-truth
  docs, validators, tools, or evals. For art, the missing invariant is usually
  not another prompt sentence; it is a manifest, builder, screenshot check, or
  reusable UI contract.
- Recent game UI agent research converges on a structured intermediate
  representation: JSON/YAML design specs between natural language, visual
  reflection, deterministic post-processing, and engine/runtime assets. For
  this repo, the local intermediate representation is the game-owned request
  packet plus crop and runtime manifests.
- Sprite processing research supports a hybrid split: use visual/AI methods for
  style-sensitive candidate generation and masks, then deterministic scripts
  for trim boxes, pivots, slice9 sanity, pack building, and screenshot health.

Supporting research notes: `ai_studio/game_design/knowledge_base/knowledge/ai_art_pipeline_research_2026.md`.

## Art Request Packet

Before generating more than one game asset, create a small request packet in a
durable or temp project path. It can be JSON or Markdown, but it must be
structured enough for another agent to reuse.

Include:

- `visual_target`: accepted fake shot, lineup, art bible, or reference image.
- `asset_family`: characters, reusable UI kit, board parts, icons,
  backgrounds, effects, or full mockup.
- `reusable_kind`: `sprite`, `slice9`, `icon`, `tile`, `border`,
  `background`, `effect`, or `full_mockup`.
- `must_not_bake`: labels, counters, icons, player-specific values, debug text,
  or gameplay state that runtime must compose dynamically.
- `crop_ids`: stable ids expected from the generated sheet.
- `slice9_insets`: left/top/right/bottom guide values for resizable UI.
- `runtime_composition`: how the game will assemble the parts.
- `qa_rejects`: unreadable text, random letters, watermarks, wrong subject,
  fused icons, weak silhouette, style drift, non-transparent background.

## Fast Local Loop

1. **Generate source art.** Save raw outputs into durable visual/source folders,
   not only the generator's default output path.
2. **Inspect before slicing.** Reject obvious failures immediately; do not
   integrate unreadable or incorrectly structured sheets.
3. **Slice from a manifest.** Keep crop rectangles, trim rules, chroma-key
   rules, pivots, and slice9 insets in a file, not in chat.
4. **Build the explicit pack.** If the project has a pack/material pipeline,
   inspect and measure the smallest pack build before choosing any direct PNG
   shortcut.
5. **Compose in runtime.** Buttons are slice9 background + runtime text/icon;
   boards are border/tile/highlight/slot pieces; characters are separate
   sprites with metadata.
6. **Validate in the primary harness.** For this repo template that usually
   means native PC screenshot/DevAPI evidence unless the current task
   explicitly targets web.
7. **Audit the evidence source.** Check that the screenshot is from the
   expected framebuffer/runtime path, has the expected dimensions, and did not
   silently fall back to a window or browser capture.
8. **Capture the lesson.** If the loop is slow, add a missing manifest field,
   builder task, validator, or skill rule.

## Batch Candidate Loop

For generated sprites, UI sheets, fake shots, or icons:

- Generate a small batch of candidates, usually 3-6, against the same
  game-owned request packet.
- Reject obvious failures before any runtime work.
- Select one or two source sheets for slicing; do not integrate every
  generated candidate.
- Keep rejected candidates in `tmp/` or another ignored source folder with a
  short reason when the failure teaches the next prompt.
- Update the game-owned request packet with the accepted source paths before
  crop/slice work.

## Parallel Work Split

When the user asks to parallelize art work, split by artifact ownership:

- research/reference packet: owns prompt, references, candidate criteria;
- asset worker: owns source sheets, crop/slice manifests, alpha/trim checks;
- runtime worker: owns pack ids, engine integration, DevAPI screenshot;
- verifier: owns visual QA notes and evidence-source checks.

All lanes must write back to the same game-owned request packet or manifest. Do
not let separate agents keep private crop coordinates, selected candidates, or
pack ids only in chat.

## Reusable UI Rules

- Generate blank button and panel states separately from labels and icons.
- Store slice9 margins as data alongside the asset.
- Do not generate a board as one baked image when the board changes at runtime.
- Generate repeated parts as tileable centers, edges, corners, highlights, and
  overlays.
- Keep icons independent so a single icon can be reused in buttons, counters,
  cards, and tutorials.

## Agent Speed Rules

- Do not hand-crop new coordinates in conversation when a manifest can hold
  them.
- Do not assume pack builds are slow. Measure the smallest pack build and use
  cache behavior when it exists.
- Do not create a game-local direct loader as the default answer to asset
  friction. Use it only when the pack path is missing, broken, or measured as
  blocking for the current iteration.
- Keep source sheets, sliced runtime assets, generated packs, screenshots, and
  rejected images in separate directories.
- Add cheap validators first: required asset ids exist, alpha/background pass,
  slice9 margins are sane, atlas/pack rebuilt, and runtime screenshot is
  nonblank/readable.
- Prefer one art-job scaffold plus repeatable script output over repeated
  manual explanation of where files should go.
- For chroma-key assets, verify alpha numerically in the runtime PNG, not only
  by looking at an image preview.
- Choose the chroma-key color per asset family. Do not use green keying for
  green field tiles, green buttons, grass, leaves, or glow effects unless the
  manifest marks those assets as `chroma_mode: none` or a later validator proves
  the alpha survived. Prefer magenta keying for green-heavy source sheets.
- Treat chroma color inside clean cutout holes as repairable background: the
  extractor should have a deliberate `remove_key_holes`/soft-matte mode and the
  visual proof must show the hole on dark, light, and warm backgrounds. Treat
  chroma color baked into object material, outlines, contact shadows, or
  semitransparent shadow ramps as risky source contamination. Normalizing a
  non-flat background can only fix border-connected background pixels; it
  cannot recover art pixels already mixed with green-screen spill. Do not
  answer contaminated shadows with only wider key tolerance or aggressive edge
  deletion, because that removes soft shadows and material shading. Try a
  benchmarked repair pass first; if it damages form/shading, regenerate with a
  safer key and gutters, request true alpha, or switch to dual-plate alpha.
- Keep cast/contact shadows separate from isolated item source sheets when
  possible. The item source should contain object shading; runtime composition
  can add a separate shadow sprite or effect. Baked shadows on a chroma plate
  are fragile because the key color leaks into the shadow's partially
  transparent pixels.
- Before building an atlas/pack, run a cheap alpha-bbox check for every asset
  id the pack builder will include. A single fully transparent runtime PNG
  should fail before the native pack tool reaches atlas packing.
- Treat screenshot size/source mismatches as failed visual evidence. A fallback
  window capture can hide framebuffer bugs or capture the wrong surface.

## Validation

An art iteration is ready for implementation when:

- the accepted visual target is linked;
- the art request packet exists;
- raw generated sheet paths and runtime asset paths are listed;
- crop/slice9 manifest is reproducible;
- pack/material build command is known and measured or explicitly blocked;
- native/runtime screenshot evidence exists for playable screens;
- any remaining visual gaps are written in the relevant task log.

For chroma-keyed source sheets, readiness also requires:

- source-sheet intake plus visual benchmark classifies key-color-like pixels
  inside component bounds as either repairable holes/background or unsafe
  material/shadow contamination;
- key/halo hue conflicts are either zero/accepted by policy or routed to a
  safer key, true alpha, split layers, or dual-plate alpha;
- runtime PNG audit reports no visible green spill and no hidden RGB under
  fully transparent alpha;
- a background proof or contact sheet is inspected on dark, warm, and light
  plates so soft edge contamination is visible before pack integration.

## References

- Unity Manual, [9-slicing](https://docs.unity3d.com/Manual/sprite/9-slice/9-slicing.html)
- Unity Manual, [Sprite Atlas](https://docs.unity3d.com/Manual/sprite/atlas/atlas-landing.html)
- Unity Addressables, [Build content overview](https://docs.unity3d.com/Packages/com.unity.addressables@2.3/manual/Builds.html)
- Unity Addressables, [Content update builds overview](https://docs.unity3d.com/Packages/com.unity.addressables@2.3/manual/content-update-builds-overview.html)
- Godot Docs, [NinePatchRect](https://docs.godotengine.org/en/stable/classes/class_ninepatchrect.html)
- Scenario, [Creative AI infrastructure](https://www.scenario.com/)
- Project knowledge, [Agent Legibility](agent_legibility.md)
- Project research, [AI Art Pipeline Research 2026](ai_art_pipeline_research_2026.md)
