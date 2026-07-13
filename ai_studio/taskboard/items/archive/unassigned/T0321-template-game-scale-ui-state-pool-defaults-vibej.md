---
id: T0321
title: "template: game-scale UI state pool defaults (VibeJam lesson - ui states exhausted)"
status: done
project: P001
epic: ""
priority: P1
tags: [template, ui, vibejam-retro]
created: 2026-07-06
updated: 2026-07-10
---

## What

During VibeJam #1 (rb-dark-rpg) the non-evicting `nt_ui_state` pool overflowed
several times (many elements across scenes sharing one UI context): fail-fast
`NT_ASSERT "nt_ui_state: pool overflow"`. Mid-jam fixes: game raised
`max_elements=4096 / state_slots=1024 / state_probe_max=16` (c8819ec9), engine
issue+PR filed from chat 019f31fe, and `nt-game-ui-layout` skill got state-hygiene
rules (clear on close via `nt_ui_state_clear/_clear_all`, raise slots only after
lifetime audit, `nt_ui_state_used_slots` open/close/reopen verification).

Remaining gap: the template still shipped engine sample defaults (256/4), so every
next game would hit the same assert. Fix: template starts at game-proven scale.

## Done when

- [x] `templates/template/src/ui/ui_runtime.c` sets `max_elements=4096`,
      `state_slots=1024`, `state_probe_max=16` with a comment pointing to
      state-clear hygiene, and `UI_ARENA_SIZE` raised 4MB -> 8MB to fit.
- [x] Template `game` target compiles with the new descriptor.
- [x] Layout skill covers state cleanup (already present since c8819ec9 —
      verified, no edit needed).

## Open questions

## Log

- 2026-07-06 lead retro follow-up: "ui states заканчивались несколько раз; в
  шаблоне стартовые значения больше + скилл вёрстки научить очищать стейты".
  Evidence trail: rb-dark-rpg `ui_runtime.c:70` (1024/16 since c8819ec9 07-05
  16:54), engine `nt_ui_state.h` defaults 256/4 with fail-fast assert, chat
  019f31fe 07-05 16:17 "NT_ASSERT failed: nt_ui_state: pool overflow ... сделай
  issue и pr в движок". Template edited (desc override + 8MB arena), compiled
  native-debug `game` OK. Skill `nt-game-ui-layout` already teaches clearing
  states on close + `used_slots` reopen-baseline verification — confirmed
  present, added in the same c8819ec9 commit.
- 2026-07-11: T0375 storage reconciliation: card was already status done with all criteria checked and evidence logged; moved from active storage into archive through the canonical CLI.
