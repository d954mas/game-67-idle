---
id: T0017
title: Fake shot art request for Mech Builder Battler
status: done
epic: ""
priority: P1
tags: [gamedesign, art-direction, fake-shot, mobile, web, mechs]
created: 2026-06-19
updated: 2026-06-20
---

## What

Create a draft art request packet for the three first-slice fake shots:
hangar first screen, battle moment, and reward/upgrade return. The packet should
make generation controllable by defining accepted-target status, candidate
policy, composition, prompt drafts, negative prompt, must-not-bake list,
runtime composition expectations, QA rejection rules, and lead review checklist.

Scope boundaries:

- In scope: fake-shot art request packet and review checklist.
- Out of scope: image generation, final art, runtime integration, pipeline/tools
  changes, engine changes, and reusable UI-kit slicing.

## Done when

- [x] `gamedesign/projects/mech-builder-battler/design/fake_shot_art_request_2026-06-19.md`
      exists with frontmatter and links to current visual/GDD/spec docs.
- [x] The packet defines the three fake shots, prompt drafts, negative prompt,
      must-not-bake list, QA rejection rules, and handoff fields for future
      generation.
- [x] The packet clearly states that it is not an accepted visual target until
      the lead accepts or edits it.
- [x] `node tools/taskboard/cli.mjs validate` passes or any failure is logged.

## Open questions

- Fake shots are accepted and should be generated; stronger source screenshots
  are only required before exact UI/economy copying or final polish.

## Log

- 2026-06-19: Added draft fake-shot art request packet at
  `gamedesign/projects/mech-builder-battler/design/fake_shot_art_request_2026-06-19.md`.
  Kept task in `review` because the packet needs lead acceptance before image
  generation or runtime visual work.
- 2026-06-20: Post-prototype cleanup: archived as historical Mech Builder Battler work after the user stopped the game.
