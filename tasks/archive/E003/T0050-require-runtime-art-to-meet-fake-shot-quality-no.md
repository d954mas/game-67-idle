---
id: T0050
title: Require runtime art to meet fake-shot quality not rough crops
status: done
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

- [x] The rule is stated in AGENTS.md (canonical) and in `generated-game-ui-assets` Failure Response: "generated allowed" != "rough crops"; runtime art must reach the fake-shot bar before it ships.
- [x] The rule makes the binding check the assembled-screen-vs-bible/fake-shot judgment (the T0045 product-read/visual gate), explicitly distinct from per-crop pixel/edge/transparency audits.
- [~] Failing/passing example: the binding check is the product-read gate (a screenshot judgment), not a new automated "composed-screen vs bible" scorer. Building such a scorer is out of scope; the human/agent visual gate from T0045 is the intended mechanism. Noted honestly rather than faked with a token test.
- [x] `node tools/skills_eval.mjs` 9/9 + `node tools/taskboard/cli.mjs validate` ok.

## Open questions

## Log

- 2026-06-15: Created from full pipeline review. Evidence: late pixel audits PASS while the assembled screen still looks nothing like the fake shot.
- 2026-06-15: Added the rule to AGENTS.md Validation (generated != rough crops; runtime art must hit the fake-shot bar; per-crop audits prove hygiene not art quality; the assembled screen vs bible is the binding check) and to `generated-game-ui-assets` Failure Response. No new automated scorer built - the composed-screen judgment is the existing product-read/visual gate (T0045). skills_eval 9/9, taskboard ok.
