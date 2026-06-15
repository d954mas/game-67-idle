---
id: T0050
title: Require runtime art to meet fake-shot quality not rough crops
status: backlog
epic: E003
priority: P1
tags: [assets, quality, visual]
created: 2026-06-15
updated: 2026-06-15
---

## What

"Generated/free assets allowed" was interpreted as permission to integrate rough
crops, not as a requirement to produce a polished runtime set (rune :157).
Pixel audits pass on edge/transparency hygiene of individual crops but never
check the assembled screen against the art bible - clean crops of the wrong art
still make a bad screen. Add the rule plus a check: runtime art must reach
fake-shot quality before integration, and the visual gate judges the composed
screen versus the bible, not just crop edges.

## Done when

- [ ] Asset/visual skills state: "generated allowed" != "rough crops allowed"; the runtime set must meet the fake-shot quality bar before integration.
- [ ] The visual gate (T0045) checks composed-screen-vs-bible/fake-shot, explicitly distinct from per-crop pixel hygiene.
- [ ] A failing example (rough-crop screen) is rejected by the gate; a passing example is accepted.
- [ ] `node tools/skills_eval.mjs` + relevant gate tests + `node tools/taskboard/cli.mjs validate` pass.

## Open questions

## Log

- 2026-06-15: Created from full pipeline review. Evidence: late pixel audits PASS while the assembled screen still looks nothing like the fake shot.
