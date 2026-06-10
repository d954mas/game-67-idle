---
name: primary-gdd-pipeline
description: Use when starting a new game concept, first GDD, visual GDD site, reference pack, fake shots, art bible, runtime asset checklist, or implementation handoff. Triggers include requests to create the initial GDD, gather refs, make a visual design website, define gameplay/core loop/currencies/UI, convert references into game-ready art direction, or prepare a next-chat implementation plan.
---

# Primary GDD Pipeline

Use this skill to turn a loose game idea into a scoped, implementation-ready
primary GDD with visual proof. Optimize for speed, clarity, and a small vertical
slice instead of a large document set.

## Non-Negotiables

- Start with a Definition of Done before creating files.
- Separate `reference`, `fake shot`, `runtime asset`, and `implementation plan`.
- Keep temporary generation, scripts, rejected images, screenshots, and audit logs in `tmp/`.
- Put only durable final outputs in the design folder, usually `gamedesing/` in this repo.
- Do not create more than 5 durable docs before producing a visual proof.
- Do not call a visual board “game-ready art” unless separate runtime assets and a composed screen proof exist.
- Validators prove consistency, not product quality. Require visual/runtime evidence when possible.

## Start Checklist

1. Read `AGENTS.md`.
2. Locate the design root: prefer existing `gamedesing/`, `gamedesign/`, `docs/design/`, or `GDD.md`.
3. Check `git status --short --ignore-submodules=all`.
4. Write the task DoD in one short block:
   - what must exist;
   - what is out of scope;
   - what proof will be accepted.
5. If the user asks for visuals or art, decide up front:
   - visual reference only;
   - fake gameplay screenshot;
   - runtime-ready asset pack.

## Fast Primary GDD Workflow

### 1. Pin The Concept

Create or update one concise concept file before expanding:

- player fantasy;
- one-sentence hook;
- genre and platform;
- target session;
- core verbs;
- primary progression metric;
- tone/safety constraints;
- no-go list.

For this project pattern, keep the central identity visible:

```text
67/67 at the top -> betrayal/downfall -> 1/67 start -> climb back through life sim systems
```

### 2. Define The First Playable Slice

Do this before market research or broad content matrices.

Write:

- first 30 seconds;
- first 5 minutes;
- core loop;
- currencies/stats;
- first upgrade;
- first job/activity;
- first visual change;
- save/reset expectation;
- acceptance criteria.

Cut scope aggressively. The first slice should prove one loop, not the whole game.

### 3. Make Visual Proof

If the user needs to “see the game”, produce visual proof in this order:

1. One gameplay fake shot that shows actual UI, currencies, action, and player goal.
2. One progression image that shows how life/status changes.
3. If implementation is next, a runtime asset pack:
   - separate character PNGs;
   - separate UI PNGs;
   - backgrounds;
   - manifest;
   - composed screen built from those separate PNGs.

Use the `imagegen` skill for generated raster art. Move final generated images
into the project; do not leave project-referenced images only under the default
generated-images folder.

### 4. Create Machine-Readable Contracts

Only after the concept and visual proof are stable, create minimal JSON contracts:

- `data/balance.json` - numbers, effects, unlocks.
- `data/ui_flow.json` - screens, actions, tabs, UI states.
- `data/asset_manifest.json` - required visual assets and fallbacks.
- `data/analytics_events.json` - only if playtest analytics is in scope.

Keep ids stable and implementation-oriented.

### 5. Write The Handoff

Create one implementation entrypoint, e.g. `game_implementation_plan.md`.

It must include:

- what already exists;
- exact source-of-truth order;
- first playable slice scope;
- files to read first;
- implementation phases;
- build/test commands;
- Definition of Done;
- prompt for the next implementation chat.

## Minimum Artifact Set

Prefer this small set first:

- `concept.md`
- `gdd.md`
- `data/balance.json`
- `data/ui_flow.json`
- `art_bible.html` or visual page/section
- `game_implementation_plan.md`

Add more docs only when they remove implementation ambiguity.

## Visual Art Done Criteria

A visual/art task is done only when the correct tier is satisfied:

- Reference tier: mood image or board, clearly labeled as non-runtime.
- Fake-shot tier: gameplay screen mock shows what the player does and sees.
- Runtime tier:
  - separate PNGs exist;
  - transparency is validated for sprites/UI;
  - backgrounds are separate;
  - manifest lists ids, files, sizes, usage;
  - a composed screen proves the assets can recreate the target look;
  - raw generation/source sheets are ignored or kept out of final commits.

## Validation

Run the narrowest useful checks first:

- GDD/data changes: `node gamedesing/tools/validate_all.mjs` if available.
- Generated art pack: validate files, manifest paths, PNG dimensions, transparency where expected.
- Website/server changes: verify HTTP `200` on the relevant page.
- Implementation handoff: confirm `game_implementation_plan.md` is linked from README/site/editor whitelist if those exist.

Do not stop at validators if the user asked for something visual or playable.
Capture or create visual evidence.

## Git Hygiene

Before commit:

1. Check `git status --short --ignored --ignore-submodules=all`.
2. Confirm no `tmp/`, generated source sheets, raw rejected images, build outputs, or screenshots evidence are staged.
3. Commit durable docs/data/final assets only.
4. Use a commit message that names the deliverable, not the activity.

## Stop Conditions

Stop and reframe before continuing when:

- the user says the result is “not game art”, “not gameplay”, or “not clear”;
- you are adding broad docs without a vertical-slice proof;
- you cannot state the current DoD in one paragraph;
- visual output is only a poster/reference but the user asked for game-ready assets;
- implementation is being started before the first playable slice is defined.
