# T0001 UI Scale Rejection - 2026-06-18

Task: `T0001 Mine Cards Mining v0.01 first slice`

Verdict: rejected by lead for PC usability.

## Problem

The current native Mine Cards UI is authored directly in raw framebuffer pixels
with fixed font sizes and hand-written compact/portrait thresholds. It does not
use the engine reference-resolution scale layer (`nt_ui_scale`), so real PC
window sizes can produce a screen that is visually tiny, unclear, and not
playable.

The issue is not just font size. Layout, text sizes, pointer mapping, and DevAPI
bounds must share one logical coordinate system derived from `nt_ui_scale`.

## Runtime Crash Found During Review

At runtime, a small/stressed window could also produce a negative slice9 target
dimension and hit:

```text
NT_ASSERT failed:
  expr: w >= 0.0F && h >= 0.0F && "slice9 target dimensions must be non-negative"
  at: external/neotolis-engine/engine/renderers/nt_sprite_renderer.c:844
```

Game-side mitigation added: skip UI slice/icon emits when width or height is
non-positive before calling the renderer. This prevents the assert, but it does
not solve the UI scale/readability problem.

## Evidence

- Crash-fix check screenshot:
  `build/captures/mine_cards_crash_fix_640x360.png`
- Zoom montage:
  `build/captures/mine_cards_crash_fix_640x360_uizoom.png`
- DevAPI state responded on the crash-fix check and framebuffer capture was
  written.
- Build passed:
  `cmake --build --preset native-debug --target game_seed`

## Required Next Fix

Freeze mechanic/content expansion. Convert the Mine Cards first screen to the
engine UI scale contract:

- compute `nt_ui_scale_t` from physical framebuffer size each frame;
- run layout and text sizes in logical/reference coordinates;
- map pointer input through `nt_ui_scale_apply_pointer`;
- expose DevAPI UI bounds in the same logical coordinate system;
- keep physical framebuffer dimensions only for GPU target/capture/resolution
  uniforms;
- recapture PC window evidence and zoom-readability evidence.

Acceptance requires the lead to be able to play/read the PC build without
zooming or guessing.

## Fix Pass 1

Implemented in game code, not engine code:

- `src/clean_seed_main.c` now includes `ui/nt_ui_scale.h`;
- the game target links `nt_ui` so the scale helper is used through the engine
  module contract;
- each frame computes `nt_ui_scale_t` from the physical framebuffer using
  `960x540` as the authored logical reference;
- layout, sprite UI, shape UI, text, input hit-testing, and DevAPI bounds now
  use logical dimensions;
- the 3D actor viewport converts its logical stage box back to physical
  framebuffer pixels before `glViewport`/`glScissor`;
- the default native window is now `1280x720`.

New evidence:

- PC window screenshot:
  `build/captures/mine_cards_nt_ui_scale_1280x720_window.png`
- Zoom montage:
  `build/captures/mine_cards_nt_ui_scale_1280x720_window_uizoom.png`
- Build:
  `cmake --build --preset native-debug --target game_seed`

Status after fix pass 1: crash is mitigated and physical PC scale is improved.
Still not accepted: lower-board composition has clipping/overlap risk and needs
one focused UI layout pass before T0001 can return to review.

## Focus Pass 2

Lead then rejected the screen for player orientation and focus: no clear accent,
too many things looked like buttons, and the first-time player could not tell
where they are or where to click.

Implemented in game code:

- top HUD now states the current place as `MINING / SURFACE`;
- the stage headline is `NOW MINING`, with auto-running node text;
- lower board keeps one active lane: `1. MINING NOW`;
- future skills are no longer rendered as pseudo-buttons, only as a muted
  `LATER` text line;
- the bottom `SKILLS` pseudo-tab was removed from the first slice;
- the Copper Pickaxe action renders as a real button only when affordable;
- mouse/keyboard input and DevAPI enabled state now match the visual state:
  unavailable upgrade and locked copper node are not exposed as active actions.

Evidence:

- PC screenshot:
  `build/captures/mine_cards_focus_v002_1280x720.png`
- Zoom montage:
  `build/captures/mine_cards_focus_v002_1280x720_uizoom.png`
- Before/after zoom montage:
  `build/captures/mine_cards_focus_v002_1280x720_uizoom_cmp.png`
- Build:
  `cmake --build --preset native-debug --target game_seed`

Status after focus pass 2: improved, but still needs lead review on the actual
PC window. The screen now has a clearer hierarchy, but this is a first-pass
focus rescue, not final UI art.
