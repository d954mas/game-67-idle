---
id: T0011
title: Engine render-target API for portal rendering
status: review
epic: E001
priority: P1
tags: [engine, neotolis-engine, rendering, render-target, portal-rendering]
created: 2026-06-18
updated: 2026-06-19
---

## What

Engine-facing issue: `Backrooms Liminal` needs a public render-target /
framebuffer API in `nt_gfx` before the portal renderer can become a fast,
general multi-pass system for arbitrary impossible rooms.

Engine tracker: https://github.com/d954mas/neotolis-engine/issues/238

Current evidence from `external/neotolis-engine`:

- Public `engine/graphics/nt_gfx.h` exposes default-frame pass lifecycle
  (`nt_gfx_begin_frame`, `nt_gfx_begin_pass`, `nt_gfx_end_pass`) but no
  render-target/framebuffer handle, attachment descriptor, or pass target.
- `engine/graphics/nt_gfx.h:251-253`: `nt_pass_desc_t` only carries
  `clear_color` and `clear_depth`.
- `nt_texture_desc_t` creates sampleable textures, but has no renderable
  attachment usage flag or color/depth attachment role.
- `engine/graphics/nt_gfx.h:317-318`: pass lifecycle is only
  `nt_gfx_begin_pass(const nt_pass_desc_t *desc)` / `nt_gfx_end_pass(void)`.
- `engine/graphics/nt_gfx_internal.h:24-25`: backend pass lifecycle also only
  accepts `const nt_pass_desc_t *desc`; no backend target handle is present.
- `engine/graphics/nt_gfx.c:286-298` forwards `nt_gfx_begin_pass(desc)`
  directly to the backend begin-pass call; there is no public target selection
  point around the pass lifecycle.
- `engine/graphics/gl/nt_gfx_gl.c:650-654` begins a pass by setting the
  viewport to `g_nt_window.fb_width/fb_height` and clearing color/depth on the
  current/default framebuffer; no `glBindFramebuffer` path is used there.
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

## Minimal requested API shape

The game does not require raw OpenGL access. It needs a backend-agnostic public
contract roughly equivalent to:

- Create/destroy a render target with width, height, color format, optional
  depth/stencil attachment, and resize policy.
- Begin a pass against either the default framebuffer or a render-target handle.
- Expose the color attachment as an `nt_texture_t` or sampleable texture handle
  for later composition.
- Define viewport/scissor defaults when entering and leaving a render target.
- Keep native GL and WebGL2 behavior aligned; if WebGL2 has constraints around
  depth textures or multisample resolve, document them in the descriptor.

## Engine issue body

Title: Add public `nt_gfx` render-target/framebuffer API for offscreen portal rendering

Problem:

`nt_gfx` currently exposes textures and default framebuffer passes, but it does
not expose a backend-agnostic way to create, bind, render into, and later sample
an offscreen color/depth target. This blocks Backrooms Liminal portal rendering:
the game needs to render an impossible target room from a portal camera, then
composite that texture into the source room aperture. The current game-side
fallback is a one-pass/proxy portal shader, which cannot scale to arbitrary
rooms, recursive spaces, or reusable portal scenes.

Evidence:

- `engine/graphics/nt_gfx.h:251-253`: `nt_pass_desc_t` only contains clear
  color/depth.
- `engine/graphics/nt_gfx.h:317-318`: public pass API has no target parameter.
- `engine/graphics/nt_gfx_internal.h:24-25`: backend pass API also has no target
  parameter.
- `engine/graphics/nt_gfx.c:286-298`: `nt_gfx_begin_pass(desc)` forwards
  directly to backend begin-pass.
- `engine/graphics/gl/nt_gfx_gl.c:650-654`: GL begin-pass clears the window
  framebuffer size and does not bind an FBO.
- Scoped search under `engine/graphics` found no public render target handle and
  no `glBindFramebuffer` path.

Requested contract:

- Create/destroy a render target with width, height, color format, optional
  depth/stencil, and explicit resize policy.
- Begin a pass against either the default framebuffer or a render target.
- Expose the render target color attachment as a sampleable `nt_texture_t` or
  equivalent public texture handle for later composition.
- Define viewport/scissor defaults for target pass entry and return to default
  framebuffer.
- Support native GL and WebGL2, documenting any WebGL2 depth/stencil/MSAA
  constraints.

Minimal acceptance test:

1. Render a colored/depth-tested offscreen scene into a render target.
2. Bind the target color texture in a later default-framebuffer pass.
3. Draw it on a visible quad.
4. Verify the offscreen depth/stencil attachment does not corrupt default pass
   depth, viewport, or scissor state.

## Open questions

- Should the public API be a direct `nt_render_target_t`, or should
  `nt_pass_desc_t` grow optional color/depth attachments?
- Should target textures be created by `nt_gfx_make_texture` with usage flags,
  or only through `nt_gfx_make_render_target`?
- Should stencil be a first-class attachment for portal aperture clipping, or
  should the first version rely on scissor/geometry masks only?
- What is the intended WebGL2 fallback for depth textures and multisampling?

## Log

- 2026-06-19: Created the external engine issue:
  https://github.com/d954mas/neotolis-engine/issues/238.
- 2026-06-19: User asked to verify whether render target/framebuffer binding is
  currently exported. Rechecked public `nt_gfx`, backend boundary, shared
  implementation, and GL backend line-level evidence. It is still not exported:
  no render target handle, no pass target, no attachment descriptor, no texture
  usage flag for renderable attachments, and no `glBindFramebuffer` path under
  `engine/graphics`. Added a portable engine issue body and minimal acceptance
  test to this task so it can be copied into the engine tracker without
  touching the submodule from the game repo.
- 2026-06-19: Re-audited the current `external/neotolis-engine` public graphics
  surface. `nt_gfx.h` still only exposes `nt_texture_t`, `nt_pass_desc_t` with
  `clear_color`/`clear_depth`, `nt_gfx_begin_pass(const nt_pass_desc_t *)`,
  texture binding, and viewport/scissor calls; no render target handle,
  attachment descriptor, or framebuffer binding is exported. The shared backend
  boundary still only accepts `nt_gfx_backend_begin_pass(const nt_pass_desc_t *)`,
  while the GL backend begins a pass by setting viewport to
  `g_nt_window.fb_width/fb_height` and clearing the default framebuffer. Scoped
  engine search found no `glBindFramebuffer`/FBO path under `engine/`, only
  framebuffer-size/window/input/UI references. Moved this issue from raw idea to
  backlog because the needed API, evidence, done criteria, and open design
  questions are now explicit.
- 2026-06-18: Rechecked current `nt_gfx` public surface for
  render-target/framebuffer binding. The API still does not export any target
  handle, pass attachment descriptor, or framebuffer bind path; existing
  Backrooms portal work must remain a one-pass/proxy implementation until the
  engine issue is resolved.
- 2026-06-18: Created from Backrooms Liminal T0010 portal-renderer work. The
  current game can build a data-driven portal-scene contract, but the engine
  lacks the public render-target/framebuffer API needed for fast, beautiful,
  reusable multi-pass portal rendering.
- 2026-06-19: Backrooms is closed as a finished experiment; keep the render-target engine issue as review evidence for future reopened portal work, not current actionable game work.
