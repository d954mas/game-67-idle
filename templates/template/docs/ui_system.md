---
type: Template Engineering Note
title: Template UI System
description: How the template consumes the ui-kit feature, and the sizing defect the feature's canvas rule replaces.
tags: [template, ui, responsive, design-system]
status: accepted
---

# Template UI System

The interface system itself lives in `features/ui-kit`: the canvas rule, the
token sheet, the art generator, the theme and the widgets. Read
`features/ui-kit/README.md` for the contract and `INSTALL.md` for the seams.
This note is the template's own half — what it binds, what it owns, and the
evidence that the old sizing rule was broken.

## The defect the feature fixes

`ui_runtime_begin` used to fit a 1280x720 reference rectangle inside the window
(`NT_UI_SCALE_EXPAND`). That question — how much of a landscape reference fits —
has no useful answer on a phone. A 360x640 window fitted 1280 units across 360
pixels, so one UI unit became 0.28 device pixels and every widget arrived at a
quarter of its intended size. The type ceilings meant to protect readability
were themselves written in device pixels, which is not a unit that means
anything physical: the same number is a legible line on a monitor and an
unreadable one on a dense phone.

Measured on the settings panel, in device pixels (`dpr` 1 in these captures, so
device pixels and CSS pixels coincide):

| window   | UI canvas before | title before | body before | touch target before |
|----------|------------------|--------------|-------------|---------------------|
| 360x640  | 1280 x 2276      | 8.4 px       | 5.6 px      | 13.5 px             |
| 390x844  | 1280 x 2770      | 9.1 px       | 6.1 px      | 14.6 px             |
| 640x360  | 1280 x 720       | 15.0 px      | 10.0 px     | 24.0 px             |
| 844x390  | 1558 x 720       | 16.3 px      | 10.8 px     | 26.0 px             |

| window   | UI canvas after  | title after | body after | touch target after |
|----------|------------------|-------------|------------|--------------------|
| 360x640  | 480 x 853        | 32 px       | 20 px      | 44 px              |
| 390x844  | 480 x 1039       | 32 px       | 20 px      | 44 px              |
| 640x360  | 853 x 480        | 32 px       | 20 px      | 44 px              |
| 844x390  | 1039 x 480       | 32 px       | 20 px      | 44 px              |

Desktop is unchanged: at 1920x1080 neither the density floor nor the cap binds,
so the canvas is still 1280x720.

Evidence: `tmp/ui_template_pass/before/`, `tmp/ui_template_pass/after/`
(settings open), `tmp/ui_template_pass/before_default/`,
`tmp/ui_template_pass/after_default/` (first screen), each with a
`contact_sheet.png` and a `summary.json` carrying the runtime `ui.tree` bounds.

## What the template owns

The feature never includes a generated header and never writes to a pack, so
three things stay on this side:

| File | Owns |
|------|------|
| `src/ui/theme.c` | resolves this game's atlas regions and calls `ui_theme_init` with the studio default tokens |
| `src/ui/ui_runtime.c` | the engine UI context, materials, atlas binding; opens each frame with `ui_frame_begin` |
| `src/ui/loc_widgets.c` | the bridge from `LocStr` to the kit's `const char *` text entry points — still the ONLY place a `LocStr` becomes a raw pointer |
| `src/build_packs.c` | packs the nine kit regions with the slice9 borders from the token sheet |
| `assets/ui/` + `assets/packs/template-ui-kit/` | the generated art and its licence, provenance, integrity and origin records |

A screen composes `ui_metrics()` and the `ui_kit_*` widgets and states no size of
its own. The layer constants come from `ui_kit.h` (`UI_LAYER_*`) because the
walker sorts globally within one Clay zIndex band: a surface with its own layer
scale cannot be stacked predictably against one that uses the kit's.

## Repainting this game

The template ships the studio default look on purpose — a prototype should not
have to design an interface before it has a game. A game that wants its own face
edits nothing in the feature: it copies `features/ui-kit/tokens/studio_default.json`,
passes its own `ui_tokens_t` to `ui_theme_init` in `src/ui/theme.c`, and
regenerates the art from the same sheet. See
`features/ui-kit/README.md` "Extension points".

## Assets

The nine PNGs in `assets/ui/` are generated in-repo by the feature's tool:

```
node ai_studio/dev_environment/python_run.mjs features/ui-kit/tools/gen_ui_kit.py --out templates/template/assets/ui
```

They are project-original (CC0), recorded per file with licence, provenance,
origin and `sha256` in `assets/packs/template-ui-kit/`, and listed in
`ai_studio/assets/manifests/tracked_binary_inventory.json` under `template:ui`.
The Kenney UI pack they replace was removed with its manifest.

Because the studio default theme is shared, this art is byte-identical to the
same nine files in the studio's private reference game. That is the point of a
default theme rather than a leak, and the privacy gate carries a narrow waiver
for exactly those nine digests
(`ai_studio/workspace/shared_private_binaries.json`).

The kit's origin is that reference game's UI. Everything else in its `assets/ui/`
was audited and left behind: fourteen icons from a paid vendor pack whose licence
forbids redistribution, one Apache-2.0 control glyph and one CC0 cursor glyph
that no template surface uses, and one game-specific loading render.

The slice9 borders in `src/build_packs.c` are stated in design units times the
token sheet's export scale; art and packer must move together.

## The platform SDK probe panel

The panel is a developer instrument and defaults OFF in every build a player can
see, debug included. It used to default ON outside Release and sat over the game
in the bottom-right corner of the first screen.

The state it reported is available without taking the screen:
`game.platform_sdk.state` over DevAPI returns target, backend, boot status and
capabilities (`src/platform_sdk_devapi.c`). The panel itself still builds behind
`-DGAME_PLATFORM_SDK_DEBUG_UI=ON` for clicking the mock ad flows by hand; the
mock ad modal is not behind that flag, because a game that calls for an
interstitial locally still has to show one.
