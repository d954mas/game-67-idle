---
name: nt-game-iteration
description: "Change a game and see it work: pick the game's own build/run/test commands, the smallest check, and a screenshot for anything visible. Use for ordinary game edits, not portal releases."
---

# NT Game Iteration

The everyday loop for a game under `games/<id>/` or `games/private/<id>/`:
edit, build, check, look. Each game owns its commands; this skill routes to
them and keeps the loop short. Portal packaging and release gates belong to
`nt-portal-publish`; deep runtime evidence belongs to `nt-runtime-automation`.

## Before the first edit

1. Read the game's `README.md` "work from here" section (or its runbook) and
   `AGENTS.md` when present. Commands, presets and the `dev` loop differ per
   game; never assume another game's.
2. Read the game's accepted decisions in `design/knowledge/` that touch the
   change (latest `playtest_*.md`, rules the lead already settled). A build that
   breaks an agreed rule is the most common rejected result.
3. Check the game's Git state: games are nested repositories, and another agent
   may hold uncommitted work there. Do not mix your change into it.

Run game commands from the game root; run Studio commands from the Studio root.

## Choose the smallest check

| Change | Check |
| --- | --- |
| One native contract | `node tools/game.mjs test --only <ctest-name>` (repeat `--only`) |
| Routine logic, ready to hand over | `node tools/game.mjs test` (the `core` tier) |
| Expensive fixtures, packaging | `node tools/game.mjs test --tier slow` |
| Anything a player sees: UI, layout, art, feel | a run plus a screenshot (below); a unit test does not prove it |
| Browser-only behaviour | `node ai_studio/runtime_automation/preview_game.mjs --game <id> --preset wasm-devapi-debug` from the Studio root; preview output is never release evidence |

Build through `tools/game.mjs`, never raw `cmake --build`: it keeps compiler
output out of the transcript. Do not run Studio `verify --full` or every tier
after each edit; that is the release and CI boundary.

Tests follow the Game Test Policy in `AGENTS.md`: cover saves, schema, economy
math and catalogs; never pin a design knob.

## Look at it

For a visible change, launch the native debug build with DevAPI the way the
game's runbook says, then:

```powershell
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/devapi_cli.py 17890 endpoints
```

Drive the scene with the game's own commands (`command.describe` before an
unfamiliar one), wait a frame, and capture with `capture.frame` or
`capture.region`. Check the image yourself before reporting it, and compare
with the previous accepted screenshot when there is one. Use
`pixel_health.py` and `ui_readability.py` from `ai_studio/runtime_automation/`
when a blank frame or unreadable text is plausible. Layout work also loads
`nt-game-ui-layout`.

Keep scratch captures under the game's ignored `build/` or `tmp/`; anything
cited later as evidence goes to `design/knowledge/` or `.ai_studio/evidence/`.

## Finish

- Commit inside the game repository, not the Studio one; the Studio pre-commit
  hook rejects private game names in Studio files.
- Report what was checked and how: the test selection, the screenshot path,
  and anything not verified.
- A suspected engine defect follows the engine boundary in `AGENTS.md`: prove
  where the root cause lives before any workaround.
