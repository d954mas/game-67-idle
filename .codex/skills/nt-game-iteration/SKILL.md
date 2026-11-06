---
name: nt-game-iteration
description: "Change a game and see it work: pick the game's own build/run/test commands, the smallest check, and a screenshot for anything visible. Use for ordinary game edits, not portal releases."
---

# NT Game Iteration

Use the game's own edit/build/check loop. Portal packaging belongs to
`nt-portal-publish`; advanced runtime proof belongs to `nt-runtime-automation`.

## Before the first edit

- Use the game's runbook and applicable `AGENTS.md` for commands and presets.
- Consult accepted decisions in `design/knowledge/` relevant to the change;
  reuse context already established in the session.
- Identify the owning repository and check its Git state. Public games may be
  tracked by Studio; private games use nested repositories. Preserve unrelated
  work and keep private game identifiers and evidence out of tracked Studio files.

Run game commands from the game root; run Studio commands from the Studio root.

## Choose the smallest check

| Change | Check |
| --- | --- |
| One native contract | `node tools/game.mjs test --only <ctest-name>` (repeat `--only`) |
| Routine logic, ready to hand over | `node tools/game.mjs test` (the `core` tier) |
| Expensive fixtures, packaging | `node tools/game.mjs test --tier slow` |
| Anything a player sees: UI, layout, art, feel | a run plus a screenshot (below); a unit test does not prove it |
| Browser-only behaviour | `node ai_studio/runtime_automation/preview_game.mjs --game <id> --preset wasm-devapi-debug` from the Studio root; preview output is never release evidence |

Build through `tools/game.mjs`, never raw `cmake --build`. Read a failed check's
log instead of rerunning with `--verbose`. Reserve Studio `verify --full` and
all-tier checks for their CI/release boundary or an explicit request.

Tests follow the Game Test Policy in `AGENTS.md`: cover saves, schema, economy
math and catalogs; never pin a design knob.

## Look at it

For a visible change, launch or reuse the runbook's DevAPI build, discover
`endpoints`, and use `command.describe` for unfamiliar commands. Drive the
affected scene, wait a frame, and capture with `capture.frame` or
`capture.region`. Inspect the image and compare against accepted evidence
when available. Use
`pixel_health.py` and `ui_readability.py` from `ai_studio/runtime_automation/`
when a blank frame or unreadable text is plausible. Use `nt-game-ui-layout`
for layout changes and its targeted desktop/phone proof.

Keep scratch captures under the game's ignored `build/` or `tmp/`; anything
cited later as evidence goes to `design/knowledge/` or `.ai_studio/evidence/`.

## Finish

- When committing, use the owning repository identified above.
- Report what was checked and how: the test selection, the screenshot path,
  and anything not verified.
- A suspected engine defect follows the engine boundary in `AGENTS.md`: prove
  where the root cause lives before any workaround.
