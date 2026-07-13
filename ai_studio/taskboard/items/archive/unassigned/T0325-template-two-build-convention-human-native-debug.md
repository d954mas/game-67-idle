---
id: T0325
title: "template: two-build convention - human native-debug (no devapi) + agent devapi-debug [AI] title"
status: done
project: P001
epic: ""
priority: P1
tags: [template, devapi, builds, vibejam-retro]
created: 2026-07-06
updated: 2026-07-10
---

## What

Lead directive 2026-07-06: agent and lead run the game in parallel — the agent
needs its own DevAPI build with its own window title; VS Code must build the
lead a devapi-free build for hand-play. Two builds per game: human + AI.

Implemented in template: GAME_DEVAPI_ENABLED now defaults OFF (agents opt in
with -DGAME_DEVAPI_ENABLED=ON in build/devapi-debug); window title compile-def
GAME_WINDOW_TITLE = "<GAME_TITLE>" or "<GAME_TITLE> [AI]" for DevAPI builds;
VS Code Debug configure tasks (game + template) pass GAME_DEVAPI_ENABLED=OFF.

Found and fixed a REAL parallel-build collision while verifying: NT_PRESET_NAME
did not encode the devapi split, so both builds wrote engine libs into the same
build/engine/native-debug — the human build overwrote nt_input.lib without
inject symbols and the agent build failed to link (undefined
nt_input_inject_*). Preset name now branches: native-release / devapi-debug /
native-debug.

## Done when

- [x] Template native-debug builds with DevAPI OFF, plain "Template" title, no
      nt_devapi symbols in exe (verified by grep on binary).
- [x] Template devapi-debug builds with DevAPI ON, "Template [AI]" title marker
      present in exe.
- [x] Both build dirs coexist: rebuilding one does not break the other
      (separate build/engine/<preset> lib dirs; verified by rebuilding
      native-debug after devapi-debug).
- [x] VS Code Debug configure tasks pass -DGAME_DEVAPI_ENABLED=OFF.
- [x] Convention documented in templates/template/devapi/README.md.

## Open questions

- Agent scripts in future games must target build/devapi-debug — worth a lint
  in nt-runtime-automation docs when T0323 ports tools/.

## Log

- 2026-07-06: implemented + verified (grep title/devapi markers in both exes,
  cross-rebuild check green). rb-dark-rpg CMake intentionally untouched (game
  closed); its VS Code task now passes OFF for hand-play rebuilds.
- 2026-07-11: T0375 storage reconciliation: card was already status done with all criteria checked and evidence logged; moved from active storage into archive through the canonical CLI.
