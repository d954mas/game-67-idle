---
id: T0415
title: Split shared Studio configuration ownership
status: done
project: P001
epic: E018
priority: P1
tags: [config, ownership, python, canvas, assets]
created: 2026-07-13
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"QTECH_001=Neutral loader and domain-owned config interpretation: 18 focused owner tests; prior independent review gap fixed; final independent review PASS with 69 tests plus CLI smoke; no live old loader or compatibility shim references."}]}
---

## What

Remove dishonest Core Harness ownership of Studio-wide and domain-specific
configuration without adding a configuration framework. Keep one neutral
loader and let real interpretation live with the module that owns it.

## Done when

- [x] Generic Studio config loading has one neutral owner outside
      `core_harness/tool_lib`.
- [x] Prefer one physical `ai_studio/config.mjs`; add a domain helper only when
      it contains real domain logic, not one wrapper file per accessor.
- [x] Python executable/environment interpretation is owned by
      `dev_environment`.
- [x] Canvas project/history/cache interpretation is owned by Canvas; video and
      CorridorKey interpretation is owned by the corresponding asset tools.
- [x] Existing ignored/local overrides continue to work without retaining the
      old domain-specific Core Harness accessor API.
- [x] No extra user-facing config layer, framework, or WSL-specific environment
      is introduced.

## Open questions

- None blocking; prefer the fewest files after callers are inventoried and do
  not create another schema/registry/abstraction layer.

## Log

- 2026-07-13: Accepted direction: keep config physically simple while making
  ownership honest; settings and resource-panel remain feature packs.
- 2026-07-14: Added the neutral `ai_studio/config.mjs` loader; moved Python and
  Canvas interpretation into their owning modules; kept video/CorridorKey
  interpretation in the video tool; migrated callers and removed the old Core
  Harness accessor API.
- 2026-07-14: Verified owner contracts and focused callers (119 tests), Studio
  routing (23 tests), ignored/local override behavior, and absence of live
  imports from `core_harness/tool_lib/studio_config.mjs`. Architecture Map
  metadata is intentionally left to T0413 and the obsolete Canvas full
  reference to T0416.
- 2026-07-14: Independent review found one surviving duplicate loader in asset
  `slice_pack.py`. It was removed: the script now runs through the Python
  owner, reuses `sys.executable` for child tools, and has a permanent contract
  regression. Final independent re-review PASS; 69 focused tests plus CLI help
  smoke pass, no live duplicate loader/old imports remain, diff check clean.
  Awaiting T0413/T0416 deletion of the last historical map/full-reference text
  before structured close.
- 2026-07-14: Final close after T0413/T0416 sync: old map/full-reference text is gone, 18 current config-owner tests pass, no live old loader or compatibility shim remains, and the independent re-review is PASS.
- 2026-07-14: Quality: QTECH_001=pass; evidence: QTECH_001=Neutral loader and domain-owned config interpretation: 18 focused owner tests; prior independent review gap fixed; final independent review PASS with 69 tests plus CLI smoke; no live old loader or compatibility shim references.
