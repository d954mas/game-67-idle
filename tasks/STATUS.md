# Project Status

## Current Goal

No active game concept. The repo root is the shared AI game-dev pipeline +
`template/`. Start a game by copying `template/` to a game folder:
`node tools/bootstrap/new_game.mjs --id <id>`.

## Current Gate

None (no active game). Each game folder runs its own gates.

## Required Validation

`node ai_studio/taskboard/cli.mjs validate`
`node tools/pipeline_validate.mjs`

## Notes

- Closed prototypes are git tags (latest: `blockside-heat-snapshot-2026-06-24`).
- `template/` is the runnable reference: settings UI on nt_ui widgets, coloured +
  textured mesh paths, a movement system, screenshot capture (`--capture`).
- Asset rule: paid/licensed binaries never enter git — they live under each game's
  gitignored `<game>/assets/restricted/`, enforced by the restricted-asset guard.
