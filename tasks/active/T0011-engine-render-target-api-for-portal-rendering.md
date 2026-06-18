---
id: T0011
title: Engine render-target API for portal rendering
status: idea
epic: E001
priority: P1
tags: [engine, neotolis-engine, rendering, render-target, portal-rendering]
created: 2026-06-18
updated: 2026-06-18
---

## What

Engine-facing issue: `Backrooms Liminal` needs a public render-target /
framebuffer API in `nt_gfx` before the portal renderer can become a fast,
general multi-pass system for arbitrary impossible rooms.

Current evidence from `external/neotolis-engine`:

- Public `engine/graphics/nt_gfx.h` exposes default-frame pass lifecycle
  (`nt_gfx_begin_frame`, `nt_gfx_begin_pass`, `nt_gfx_end_pass`) but no
  render-target/framebuffer handle, attachment descriptor, or pass target.
- `nt_pass_desc_t` only carries `clear_color` and `clear_depth`.
- `nt_texture_desc_t` creates sampleable textures, but has no renderable
  attachment usage flag or color/depth attachment role.
- Search of engine graphics/backend code found viewport/scissor/depth/stencil
  state, but no public `nt_gfx_*render_target*`, no public framebuffer bind, and
  no `glBindFramebuffer`/FBO path exposed through `nt_gfx`.

Why it matters:

- A universal portal renderer should render a target room from a transformed
  portal camera into an offscreen color/depth target, then composite or clip it
  into the source room's portal aperture.
- Recursive or cached impossible spaces need multiple target views per frame,
  with predictable resize, depth/stencil, and texture binding.
- Without this API, the game can only do one-pass fullscreen shader traversal
  or hardcoded illusions, which is too limiting for "any levels and spaces".

## Done when

- [ ] Engine has a public `nt_gfx` render-target handle API, or an accepted
      equivalent design, without game repo patching the submodule.
- [ ] API supports native and web/WebGL2 backends.
- [ ] A pass can render into a color texture and optional depth/stencil
      attachment, then bind the color texture in a later draw.
- [ ] Resize/lifetime rules are explicit for window-size and fixed-size portal
      targets.
- [ ] Viewport/scissor state is well-defined when rendering into a target and
      returning to the default framebuffer.
- [ ] Minimal test/example proves: render offscreen scene -> sample texture on
      visible quad -> depth/stencil attachment does not corrupt default pass.
- [ ] Backrooms portal renderer can replace its one-pass hardcoded portal proof
      with at least one offscreen target-room view.

## Open questions

- Should the public API be a direct `nt_render_target_t`, or should
  `nt_pass_desc_t` grow optional color/depth attachments?
- Should target textures be created by `nt_gfx_make_texture` with usage flags,
  or only through `nt_gfx_make_render_target`?
- Should stencil be a first-class attachment for portal aperture clipping, or
  should the first version rely on scissor/geometry masks only?
- What is the intended WebGL2 fallback for depth textures and multisampling?

## Log

- 2026-06-18: Created from Backrooms Liminal T0010 portal-renderer work. The
  current game can build a data-driven portal-scene contract, but the engine
  lacks the public render-target/framebuffer API needed for fast, beautiful,
  reusable multi-pass portal rendering.
