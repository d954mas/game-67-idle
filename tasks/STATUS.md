# Project Status

## Current Goal

Active concept: `blockfell-runes`, native PC 3D block-fantasy action RPG.
Primary task: T0013 Blockfell Runes first playable vertical slice.

## Blocking Work

- No blocking work for the current vertical slice.

## Non-blocking Debt

- T0010 is a deferred post-prototype asset consistency idea; do not start it
  before Blockfell Runes has accepted art direction and batch content.
- T0013 still uses procedural shape models/materials; replace with legal
  project-local assets before claiming final visual quality.

## Current Gate

Keep the native slice playable and evidence-backed: movement, combat, loot,
rune progress, gate opening, screenshot proof.

## Required Validation

```powershell
node tools/taskboard/cli.mjs validate
cmake --build --preset native-debug --target game_seed
py -3.12 tools/devapi/smoke.py 9123
```

Do not use context budget or `validate --review` as a normal development gate;
run it only on explicit request or during review.

## Last Known Good Evidence

- 2026-06-20: build and `py -3.12 tools/devapi/smoke.py 9123` passed.
- 2026-06-20: latest visual proof:
  `build/captures/blockfell_authored_asset_pass.png`; pixel health passed.
- Detailed evidence is in T0013 and `gamedesign/projects/blockfell-runes/`.

## Next Priorities

1. Replace procedural geometry/materials with legal project-local assets.
2. Add readable engine-font labels/tooltips for controls and objectives.
3. Expand the first route only after the current combat/loot/rune loop remains
   validated by native smoke and screenshot evidence.
