---
id: T0323
title: "template web delivery parity: package_web.sh + web-load-smoke + scenarios skeleton"
status: backlog
project: P001
epic: ""
priority: P1
tags: [template, web, release, smoke, vibejam-retro]
created: 2026-07-06
updated: 2026-07-06
---

## What

Lead directive 2026-07-06: every jam-proven delivery fix must live in the
template so a new game gets it out of the box. Parity audit vs rb-dark-rpg:
template has NO tools/ dir at all (no package_web.sh, dev_rebuild.sh,
run_tests.sh) and devapi/ lacks scenarios.py. Wasm link flags + glad/stb
native-only guard were ported to template CMakeLists 2026-07-06 (unverified
under emscripten — no wasm build dir for template yet).

Web-load-smoke (lead: "нужен"): jam submission worked only on 2nd-3rd try;
load errors were visible in the browser console — catchable automatically.

## Done when

- [ ] templates/template/tools/: package_web.sh (build wasm-release, copy pack,
      zip) + dev_rebuild.sh + run_tests.sh ported and genericized from
      rb-dark-rpg.
- [ ] Web-load-smoke wired into package_web.sh: serve zip contents via
      http.server, headless Chrome (--headless=new, SwiftShader per
      web-wasm-headless-verify), FAIL on console errors; first-frame
      pixel-health check (not black).
- [ ] templates/template/devapi/scenarios.py skeleton (scenario hooks pattern
      from rb-dark-rpg) so arc-smoke gates have a place to live in new games.
- [ ] Template wasm-release configure+build passes with the ported CMake flags.

## Open questions

- Smoke cadence: every package_web run (confirmed needed by lead); arc-smoke
  gate placement (per-task done vs release-only) still undecided.

## Log

- 2026-07-06: created from VibeJam retro walkthrough (plan item 1 + template
  parity directive). CMake wasm flags + glad/stb guard already ported; native
  template build verified, emscripten build NOT yet verified.
