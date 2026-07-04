# T0256 — Text-to-Animation, PROCEDURAL/RIG angle (research)

Date: 2026-07-04. Sibling agent covers generative-video models; this note covers the
deterministic spec + procedural-transform route. Recommendation first, evidence as
`file:line`, then the v1 pipeline.

## TL;DR recommendation

Build a **tiny custom deterministic animation spec** (`ai_studio.canvas.animation.v1`) of
**affine transform tracks + opacity**, authored/edited by **codex** (vision + RU text ->
JSON, or minimal patch), played by a **new continuous-rAF preview mode** that injects
per-frame transform deltas into the existing `paintElement`, and **baked to flipbook PNG
frames** by re-driving the existing `render_group.py` PIL path N times. Do **not** adopt
Lottie/Rive/Spine as the authoring format, and do **not** try to "export to an engine
animation runtime" — the engine deliberately has none.

## 1. Engine reality — what actually plays animation (load-bearing)

There is **no animation subsystem in the engine**, by design:
- `docs/neotolis_engine_spec_1.md:3057` lists "sprite animation system" as a NOT-yet-built
  module; `:3318` — "the animation clock is passed in by the game — there is no engine
  [animation]"; `:1009`/`:1019` — the game drives a flipbook itself by reading back the
  cached atlas region index and swapping it per frame.
- `nt_ui_anim.h:57` (`nt_ui_anim`) is an **exponential-smoothing easing cache toward a
  target** (card-flip / HP-bar slide), keyed per widget id — NOT a keyframe timeline, and
  UI-only. Not a sprite animation player.
- `nt_sprite_comp.h` — sprites bind an atlas region (by hash/index), flip, slice9, origin.
  **No sequence / frame_time / flipbook fields.** `nt_sprite_renderer_emit_region`
  (`nt_sprite_renderer.h:119`) takes a **per-sprite column-major mat4** — the game is free
  to drive position/rotation/scale procedurally each frame, but must write that loop itself.
- `deps/cglm/include/cglm/ease.h` exists — full easing library is available to *game* code
  if a future game-side transform-track sampler is ever written.

**Consequence:** the animation spec is primarily an **authoring + preview artifact on the
canvas**. The realistic engine handoff is **baked flipbook frames** (atlas region swap — the
`:1009` path, matches the engine's "prebuilt assets" philosophy), NOT a live spec the engine
interprets. A transform-track runtime inside the engine (game samples tracks -> `emit_region`
mat4) is a possible *future*, explicitly out of v1 scope.

## 2. What a single alpha-cut sprite supports without rigging

`paintElement` (`site/workspace.js:289-355`) already reads `x/y/w/h`, `rotation`, `flipH`,
`flipV`, `slice9` and applies `ctx.save/translate(center)/rotate/scale(flip)/translate`.
So the affine surface is **already there** — a preview only injects deltas into that block:
- **Whole-sprite transform oscillators** (no slicing needed): bob = `off_y` sine, pulse =
  `scale` sine, pendulum = `rot` sine, shake = `off_x/off_y` noise, spin = `rot` linear
  ramp, throb/fade = `opacity` sine. Pivot defaults to the box center (matches paintElement).
- **Multi-part** (the killer demo): the canvas already slices a sprite into part-elements
  via region-detect + `sliceRegions` (`ops.mjs:10`, `:934`) and nests them with `parentId`
  groups (`createGroup` `ops.mjs:1344`). Wings become sub-elements; each part gets its own
  transform channels (L/R wing = `rot` oscillators in opposite phase) animated relative to
  the parent group. This is exactly "крылья должны махать".
- **Mesh/warp (wave a flag):** a per-column `drawImage` source-slice loop with a sine
  y-offset is feasible on 2D canvas *without WebGL* for simple banner/wave (same shape as
  the slice9 patch loop at `:319-326`); a true mesh grid needs WebGL. **Defer to v1.1** —
  PIL parity is extra work and it is not the core ask.
- **Particle overlays (sparkles/fire):** canned parametric emitters (type/rate/lifetime/
  gravity/spread/color), drawn by the preview loop and baked identically. Additive, no
  lottery. **Defer to v1.1** (or a thin 3-preset version) — not needed to make a sprite
  "feel alive".

**v1 = affine transform oscillators + keyframe tracks + opacity, per element/part.** Covers
pendulum/bob/pulse/shake/spin/fade/throb — the bulk of the value.

## 3. Spec format — custom subset, not Lottie/Rive/Spine

| Format | Fit | Verdict |
|---|---|---|
| **Lottie** JSON | Mature, huge ecosystem; per-layer AORPS (anchor/opacity/rotation/position/scale) keyframes on any property. But AE-centric, abbreviated keys, **LLM cannot reliably emit/patch raw**, and **no player in his engine**. | Reject as authoring format. Our primitive set is a strict subset of Lottie AORPS, so a Lottie *export* (web sharing) is a cheap future add. |
| **Rive** | Binary + interactive state machines, editor-first. | Reject — binary, editor-centric, over-scoped. |
| **Spine** | **Skeletal rig** — explicitly the thing we are avoiding. | Reject. |
| **CSS/WAAPI** | DOM-only, not canvas. | Reject. |

Recommend a **tiny custom schema** `ai_studio.canvas.animation.v1` — matches his laws
(additive, minimal, loud validation) and maps 1:1 onto `paintElement`'s existing transform:

```
element.animation = {                    // absent => static (mirror rotation===0-dropped law, store.mjs:280)
  duration: 2.0, loop: true,
  channels: [                            // NAMED channels => stable LLM diffs
    { id:"wingL", target:"<partElementId>", prop:"rot",
      kind:"osc", wave:"sine", amp:18, freq:1.5, phase:0, pivot:[0.1,0.5], ease:"inout" },
    { id:"body",  prop:"scale", kind:"osc", wave:"sine", amp:0.04, freq:1.0 }
  ]
  // kind:"keys" alt form: track:[{t,value,ease}, ...] for bespoke one-shot choreography
}
```
`prop ∈ {off_x, off_y, rot, scale, scale_x, scale_y, opacity}`. Both `osc` and `keys`
compile to one evaluator `sample(t) -> {off_x,off_y,rot,scale_x,scale_y,opacity}` applied on
top of the element's authored transform. Unknown prop/wave => **throw** (mirror the store's
`Number()` validation + the ops loud-error law, `ops.mjs` header). **One new field to add to
both renderers: opacity** (JS `globalAlpha`, PIL layer alpha) — everything else is already
honored by both paint paths.

## 4. LLM-as-animator (codex) — contract + iteration

Codex is already wired for text + vision via `tools/prompt_assist.mjs` (`buildExpandInstruction`
text, `buildExtractInstruction` vision; loud JSON parse via `stripCodeFence` + required-keys,
`:187`/`:217`/`:243`). Add a sibling **`buildAnimateInstruction`**:
- **Inputs:** the sprite image (vision — so codex picks sensible pivots and, for a grouped
  multi-part sprite, binds channels to the right part-element ids/labels) + the RU text +
  (for edits) the **current spec JSON**.
- **Output:** either the full `v1` JSON, or — for edits — a **minimal patch over named
  channels** (JSON merge on `channel.id`). Named (not positional) channels make diffs stable.
- **Iteration loop:** "крылья должны махать медленнее" -> send current spec + ask for the
  minimal patch (e.g. `{channels:[{id:"wingL",freq:0.8},{id:"wingR",freq:0.8}]}`), not a
  full regen -> apply -> instant re-preview. **No re-roll lottery**; a bad number is one
  slider nudge away.
- **Determinism/taste fit:** every channel is an explicit, human-legible parameter the
  inspector exposes as amplitude/frequency/phase sliders — the "NUDGE, don't re-roll" delight.
- **Tests never spawn codex** — reuse the injected-fake-assistant contract already used for
  expand/extract (`prompt_assist.mjs:5-6`).

Note: codex vision sees ONE flattened image. Multi-part requires the parts to already be
sliced (existing op) before "махать крыльями"; an un-sliced sprite still gets whole-sprite
motion (bob/pulse/spin) with zero slicing.

## 5. Preview runtime

The canvas `render()` is **on-demand** — coalesced to one repaint per frame on state change
(`site/workspace.js:119-128`), **not a continuous loop**; grep finds `requestAnimationFrame`
only in that scheduler. So add a **self-scheduling rAF "animation preview mode"** (view-state
only, like the T0207 cleanup preview `:300` and the Alt clip-ghost peek — never journaled/
persisted). It runs *only* while preview is active for a selected element/group, computes
`sample(t)` per animated channel, injects `off/rot/scale/opacity` into `paintElement`, and
**stops when no channel is active** (idle cost zero). Cost is one repaint/frame — identical
to a drag — so ~10 animated part-elements at 60fps is the same order as an active drag
(verify with `tests/bench.mjs`). Minimal v1 UX: play/pause toggle + a scrub slider bound to
`t`; no timeline editor.

## 6. Bake path

`render_group.py` already composites the element tree with the SAME node schema
(`paint_element` reads `x/y/w/h/rotation/flipH/flipV/slice9`, `tools/render_group.py:58-118`,
`--spec ai_studio.canvas.render_group_spec.v1`). Bake = for k in 0..N-1: evaluate the spec at
`t = k/N * duration`, write a render-group spec whose animated nodes carry the sampled
`x/y/w/h/rotation/opacity`, call `render_group.py` -> N PNGs. Because **preview and bake read
the identical node schema and share one sampler**, baked frames match the preview by
construction — this extends the existing byte-for-byte JS<->PIL parity contract
(`site/workspace.js:332`, `render_group.py:69`). N frames then feed the existing
sheet/slice/atlas/export machinery; APNG/GIF is a thin encode over the frame list for sharing.

## 7. Integration shape

- **Model:** additive `element.animation` blob (absent = static), same "absent means default"
  discipline as `rotation`/`flipH`/`clip` (`store.mjs:257-281`).
- **Ops + parity:** a `setAnimation`/`patchAnimation` op in `ops.mjs` that `commitMutation`s
  one journal entry (`ops.mjs:321`), routed in `api.mjs` (PUT `.../elements/<eid>/animation`,
  same shape as `slice9` `api.mjs:838`) and mirrored in `cli.mjs` — **one op, two equal
  clients** (agent CLI + site page), per the tool-parity invariant.
- **Journal semantics:** spec edits (codex apply, slider nudge) are journaled one-per-change;
  the live preview and scrub `t` are **view-state, never journaled** (matches clip-ghost /
  cleanup-preview precedent).
- **Inspector:** a new "Animation" section (same `field/numberInput/appendChild` pattern as
  the slice9 / alpha sections, `inspector.js:182`/`:466-505`): per-channel prop/wave selects
  + amp/freq/phase number inputs + a "Describe animation…" text box that calls codex, and
  play/pause + scrub.

## 8. Honest limits — where the generative route genuinely wins

Text-to-procedural (affine tracks) **cannot** do: organic **secondary motion** (fur/cloth/
hair follow-through), **soft-body/jiggle**, **morphs / shape interpolation** (mouth shapes,
squash that changes silhouette), true **volumetric/3D turnaround**, or any motion the single
flat sprite has no geometry for. Mesh-warp gets simple cloth-wave only. **Split of territory:**
procedural owns *deterministic, editable, loopable transform life* (idle bob, wing flap,
pulse, sway, sparkle) — the daily "make this sprite feel alive" case; the generative-video
route owns *organic, one-shot, physically-rich* motion where a spec cannot express the intent.

## 9. Recommended v1 pipeline (increments + risks)

Increments, roughly ordered/sized:
1. **Spec + shared sampler** (`animation.v1` schema, `sample(t)` evaluator, loud validation)
   + add `opacity` to both `paintElement` and `render_group.paint_element`. (S/M)
2. **Preview mode** — self-scheduling rAF, delta injection into `paintElement`, play/pause +
   scrub, view-state only. (M)
3. **Codex `buildAnimateInstruction`** (create + minimal-patch edit) + loud JSON parse; op
   `setAnimation`/`patchAnimation` with journal; tests inject fake assistant. (M)
4. **Inspector section** — per-channel sliders + describe box + transport. (M)
5. **Bake** — N-frame driver over `render_group.py`, hand off to existing sheet/export; GIF/
   APNG encode. (M)
   v1.1: canned particle emitters; per-column mesh-warp (wave).

Three riskiest assumptions + cheap verifications:
- **A. Codex emits/patches valid `v1` JSON with sensible pivots from RU text + vision.**
  Verify: a 10-prompt harness over 2-3 sample sprites, measure valid-JSON rate and
  "looks-right on preview"; reuse the fake-assistant test seam (no codex in CI).
- **B. JS preview and PIL bake stay frame-identical.** Verify: make the sampler ONE shared
  pure module (or mirror + a golden test comparing JS `sample(t)` vs Python `sample(t)` at N
  times); extend the existing byte-parity contract; confirm the new `opacity` field agrees.
- **C. Continuous-rAF cost with N animated part-elements.** Verify: `tests/bench.mjs` at
  ~10 part-elements/60fps; confirm the loop stops (zero idle cost) when no channel is active.
