---
type: Source Notes
title: Game UI UX Design Guidelines Research
description: Reusable notes on game UI/UX, HUD hierarchy, runtime UI composition, slice9-safe art, atlases, and reusable generated UI asset systems.
tags: [sources, game-ui, ux, hud, accessibility, slice9, atlas, generated-ui]
checked: 2026-06-14
---

# Game UI UX Design Guidelines Research

## Research question

How should game UI be designed and produced so it is readable, beautiful,
reusable, atlas-efficient, and safe for runtime resizing?

The immediate failure to prevent: attractive generated panels/buttons that look
good as a static composed image, but produce bad slice9 results because unique
ornaments, caps, lighting, labels, and state decorations are baked into
stretchable regions.

## Source matrix

| Source | Observed facts | Take | Do not take |
| --- | --- | --- | --- |
| Game Accessibility Guidelines full list: https://gameaccessibilityguidelines.com/full-list/ | Recommends large/well-spaced interactive elements on small or touch screens, simple clear language, readable default font size, high contrast between text/UI/background, objectives/reminders, clear indication that elements are interactive, resizable/rearrangeable interfaces, and portrait/landscape support where relevant. | Treat readability, contrast, touch target size, objective clarity, and player-controlled pacing as baseline game UX gates, not late polish. | Do not judge UI quality only by ornamental style or source image beauty. |
| Unity 9-slicing manual: https://docs.unity3d.com/Manual/9SliceSprites.html | 9-slicing preserves corners while horizontal borders stretch only horizontally, vertical borders stretch only vertically, and the center stretches/tile-fills. Without 9-slicing, the whole sprite stretches. | Author resizable panels/buttons as corners, straight/repeatable edges, and boring center fills. Preview at minimum, target, and oversized sizes. | Do not bake unique center gems, side medallions, labels, or asymmetric caps into areas that will stretch. |
| Godot NinePatchRect: https://docs.godotengine.org/en/stable/classes/class_ninepatchrect.html | NinePatchRect splits a texture into a 3x3 grid, keeps corners unchanged, tiles/stretch edges and center, supports per-side patch margins, region rects for atlas use, optional center drawing, and stretch/tile modes. Tile mode avoids distortion only when the texture is seamless. | Store patch margins, atlas region, center draw policy, and stretch/tile mode in runtime metadata. Use tile only for seamless textures. | Do not assume one uniform margin works for all sides or all sizes. |
| Godot UI containers: https://docs.godotengine.org/en/stable/tutorials/ui/gui_containers.html | Containers own child positioning and resizing. Box, grid, margin, panel, scroll, aspect-ratio, flow, and center containers each solve different layout jobs. | Build UI as layout containers plus reusable visual parts, not absolute-positioned art screenshots. Separate desktop and portrait compositions with container rules. | Do not manually place every element in one bitmap-like composition if the screen must respond to content and aspect ratio. |
| Godot multiple resolutions: https://docs.godotengine.org/en/stable/tutorials/rendering/multiple_resolutions.html | Treat base size as design size; multiple resolutions require explicit stretch/aspect choices. Fractional scaling can cause uneven pixel/line widths; scale should be exposed when useful for accessibility. | Define base design size, target aspect families, safe areas, and screenshot proofs. Keep UI line widths and text readable under scale. | Do not validate only one desktop aspect ratio and assume mobile/portrait will work. |
| TexturePacker docs: https://www.codeandweb.com/texturepacker/documentation | Runtime data files carry sprite positions, names, trimming, pivots, 9-patch data, and other metadata. | Atlas metadata is part of the asset, not an optional build artifact. | Do not keep crop/slice/pivot decisions only in chat or filenames. |
| TexturePacker texture settings: https://www.codeandweb.com/texturepacker/documentation/texture-settings | Supports alpha bleeding/premultiply to reduce transparent-border artifacts; max/fixed sizes, scaling variants, algorithms, trim modes, trim threshold, extrude, border padding, shape padding, common divisors, identical layout, and duplicate sprite aliasing. Shape padding of at least 2 avoids neighboring pixels being sampled in OpenGL rendering. | Add trim/extrude/bleed/padding/common-divisor policy to generated UI assets and atlas packs. Use variants and shared/aliased sprites for size savings. | Do not crop to alpha tightly without padding/bleed; it causes halos and neighbor bleeding. |
| Aseprite slices docs: https://www.aseprite.org/docs/slices/ | Slices can name rectangular regions, store 9-slice internal centers, define pivots, export each slice, and export slice data to JSON. | Treat slice definitions as authored metadata: bounds, center, pivot, and semantic name. | Do not treat crop coordinates as disposable temporary script constants. |
| Aseprite sprite sheet docs: https://www.aseprite.org/docs/sprite-sheet/ | Sprite sheets/atlases are the normal production format; import supports offset, sprite size, padding, and sheet orientation; texture atlas is one loaded image containing game graphics. | Build named source families and packed atlases with padding/metadata. | Do not ship many loose ad hoc PNGs without an atlas/manifest plan once runtime integration starts. |
| AutoGameUI paper: https://arxiv.org/abs/2411.03709 | High-fidelity game UI generation suffers from mismatched UI and UX design; the proposed system uses structured representations/protocols to maintain coherence across constructed UI. | Use structured intermediate specs for screen purpose, hierarchy, component roles, visual states, and interaction. | Do not rely on a pretty generated image to encode UX, hierarchy, and runtime behavior implicitly. |
| GameUIAgent paper: https://arxiv.org/abs/2603.14724 | Uses Design Spec JSON, deterministic post-processing, and VLM reflection. Reports visual-domain failure modes such as rarity degradation and visual emptiness; partial rendering improvements can amplify structural defects in evaluation. | Keep generated UI in editable specs/manifests, run deterministic postprocess, then visual review. Evaluate structure before surface polish. | Do not polish rendering before structure is correct; a nicer broken layout can hide or amplify defects. |

## UI/UX rules for games

1. Start from player intent, not the panel art.
   Every screen needs an answer to: where am I, what is important, what can I
   do, what changed, and what reward/state matters now. If this cannot be read
   in 3-5 seconds from a screenshot, the screen is not ready for asset polish.

2. Separate persistent HUD, contextual HUD, modal UI, and decoration.
   Persistent HUD should show only always-needed state. Contextual UI appears
   only when useful. Modal UI interrupts for a decision. Decoration supports
   theme but cannot compete with the action hierarchy.

3. Use visual hierarchy before ornament.
   Primary action, current objective, resource changes, blocked/locked state,
   and reward feedback need stronger contrast and position than decorative
   frames. Rich frames are acceptable only if they do not reduce text contrast
   or confuse click/touch affordance.

4. Prefer one dominant action in FTUE.
   Casual game first screens should not expose every system. Use one primary
   action, 1-2 secondary actions, a current objective, and obvious reward
   feedback.

5. Make interactivity visible.
   Buttons need shape, depth/state, hover/press/disabled/locked/selected
   variants, and spacing. Static labels, values, and non-interactive chips
   should not use the same treatment as clickable controls.

6. Text belongs to runtime, not source art.
   Generated text, fake glyphs, baked labels, baked numbers, and baked quest
   names destroy localization, balance iteration, accessibility, and reuse.

7. Mobile is a separate composition.
   Portrait should reduce simultaneous values, use one full-width primary
   action row, place secondary actions below, and avoid desktop stat strings.
   Do not squeeze a desktop HUD into a phone.

## Runtime UI element taxonomy

For generated game UI, request and build these as separate families:

- **Screen backgrounds:** map parchment, dark vignette, inventory paper, shop
  backdrop. Usually not slice9; often full-bleed or tiled.
- **Resizable bases:** blank panel base, modal base, button base, input slot,
  reward chip, tooltip, status bar, tab. These must be slice9-safe.
- **Decor overlays:** corner caps, top plaques, side gems, medallions, locks,
  rarity crests, dividers, screws, glow strips. These anchor onto bases.
- **Icons:** currency, health, mana, skill, quest, location, warning, reward,
  item rarity. These need semantic ids, padding, and consistent silhouette.
- **State overlays:** selected ring, disabled cover, locked chain, affordable
  shine, claimable pulse, cooldown mask, progress fill.
- **Runtime text:** labels, values, timers, quest text, button captions,
  tooltips, accessibility names.
- **Hit targets/layout:** invisible or debug-drawn rectangles that may be
  larger than the art, especially on touch.

## Slice9-safe art rules

1. Slice9 bases must be structurally boring.
   Corners can be beautiful. Long edges must be straight/repeatable. Center
   must be plain or seamless. Unique center symbols, caps, banners, locks, and
   medallions are separate overlays.

2. Split beauty from stretch.
   If an element is the reason the panel looks expensive, it probably should
   not stretch. Export it as an overlay with an anchor:
   `top_center`, `bottom_center`, `left_mid`, `right_mid`, `corner_tl`, etc.

3. Author edge strips intentionally.
   Long fantasy planks, sci-fi rails, metal seams, and glowing strips should be
   produced as repeatable/tileable edge materials or as fixed left/mid/right
   segments. A generated full edge with unique asymmetry usually fails when
   resized.

4. Content safe area is first-class.
   A panel needs a content rectangle where text/icons cannot overlap borders,
   caps, gems, screws, or grime. Button labels need safe x/y padding independent
   of the art crop.

5. Minimum size is part of the asset.
   A button that works at 240x64 may fail at 128x48. Record min size and
   disallowed uses.

6. Preview hostile sizes.
   Test minimum, normal, oversized wide, oversized tall, and portrait target.
   If the asset only looks good at source size, it is a static mockup asset,
   not a reusable UI base.

## Atlas and reuse rules

1. Pack by runtime lifetime and screen family.
   Example: `ui_common`, `ui_fantasy_panel`, `ui_icons_core`,
   `ui_map_markers`, `ui_fx`. Avoid one giant atlas if many screens load only
   a small subset.

2. Store metadata with the atlas.
   Every entry needs id, rect, trim, original size, pivot/anchor, slice9
   margins, content safe area, state role, and source family.
   For generated-art review, the proof atlas should also draw readable asset
   ids in padding/free space so the reviewer can say exactly which named asset
   is accepted or rejected. That label overlay is review evidence, not runtime
   texture data.

3. Use trim plus padding/bleed/extrude.
   Tight crops save space but cause sampling artifacts. Use transparent
   padding, alpha bleed, and border/shape padding; validate at target scale.

4. Alias identical art.
   Shared borders, repeated button centers, disabled overlays, and generic
   chips should not be duplicated in the atlas. Store multiple ids pointing to
   the same region when the runtime semantics differ. Review manifests should
   report both semantic entry count and physical bitmap count.

5. Prefer overlays over duplicated full buttons.
   A primary/secondary/locked/selected button family can often be:
   base button + color/glow overlay + lock overlay + selected frame + runtime
   text. Full separate PNGs are justified only when silhouette/material changes
   substantially.

6. Preserve scale variants deliberately.
   Desktop, high-DPI, and mobile can use variants, but the layout coordinates
   must stay divisible/stable where possible to avoid fractional artifacts.

## Reading the provided failed examples

The sci-fi and fantasy screenshots show good style direction but poor runtime
asset decomposition:

- The assembled images say "cropped PNGs", but the panels still contain baked
  long-edge decorations, glowing strips, center plaques, side ornaments, and
  button state text.
- Green/key fringe is visible in places; extraction cleanup is still part of
  the problem, but the bigger issue is that the art is being treated as whole
  composed panels instead of a UI kit.
- The right-side fantasy panel has fixed top/bottom/corner metal blocks and
  side brackets that should be overlays or fixed edge segments, not stretchable
  frame pixels.
- Buttons are visually rich but too state-specific as whole PNGs. Better:
  one blank base, separate hover/selected/locked overlays, separate icon/lock,
  runtime label.
- The progress bar should be a system: track slice9, fill tile/slice, cap(s),
  optional marker/handle, runtime label. It should not be one cropped strip
  with text and decorative center medallion baked in.

## Prompt implications for generated UI

Generate source families with explicit separation:

- "blank resizable bases only: no text, no icons, no center medallions, no
  unique symbols in stretch zones";
- "separate decor overlay sheet: corner caps, top plaques, side gems, screws,
  locks, selected rings, glow strips, each isolated with gutters";
- "isolated icon sheet: square icons, consistent silhouette, no frames unless
  frames are a separate family";
- "bar system sheet: track base, fill strip, left cap, right cap, marker,
  optional overlay glow";
- "include visible safe content rectangle in the design spec, but do not bake
  guide lines into final source art";
- "source art must be generated at 1x or 2x target runtime scale with generous
  gutters and flat/dual-plate extraction background".

## Pipeline changes this implies

1. Add a UI kit decomposition checklist before generation.
2. Require per-family prompts: bases, overlays, icons, bars, backgrounds.
3. Require a slice9 hostile-size preview, not only contact sheet.
4. Add an overlay composition preview: base + decor + text + state overlays.
5. Add atlas policy fields: `pack_group`, `trim`, `bleed`, `extrude`,
   `shape_padding`, `alias_of`, `variant_scale`, `anchor`.
6. Separate review atlas proof from game runtime packing. The review atlas is
   for visible validation, labels, alias proof, extrusion proof, and handoff
   names; the game can repack the accepted assets later using its own packer.
7. Add a design rejection gate: fail source sheets with unique non-corner
   ornament inside stretch zones even if pixel audits pass.
8. Keep performance fast by running quick gates during iteration and full
   portable validation only before commit/release.
