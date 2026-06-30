# GDD Review

Load this before claiming a GDD, visual GDD, reference pack, gameplay data, or
handoff task is done.

## Stance

Review as a product/design director, not as the author. Prioritize blockers,
missing proof, stale source-of-truth, and player-facing confusion.

## Gate Review

Check the current gate:

- Concept: hook, fantasy, audience, platform, pillars, no-go list.
- References: 3-7 refs, source quality, borrow, avoid, copy-risk.
- Visual proof: gameplay fake shot or running-game proof exists and is visible.
- First slice: first 30 seconds, first 5 minutes, loop, economy, UI flow.
- Handoff: source order, phases, commands, acceptance gates, next prompt.

If the claimed gate is not satisfied, report `partial` and name the missing
piece.

## Quality Routing

Use project quality rules instead of inventing a second quality system:

- GDD/source package: `QGDD_001`.
- Gameplay loop/design fit: `QDES_001`.
- Player-facing visual or UI clarity: `QCLR_001`.
- Responsive GDD/site surface: `QCLR_002`.
- Art or fake-shot finish: `QART_001`.
- Asset publish/readiness: `QASSET_001`.
- Technical/runtime proof: `QTECH_001`.

## Common Findings

- GDD names a fantasy but the screen does not show it.
- Art exists, but no fake gameplay UI exists.
- Currencies exist, but sources/sinks are unclear.
- Site exists, but it contradicts JSON/source docs.
- Handoff has phases, but no first clickable action.
- Handoff has UI actions, but no mechanics numbers behind them.
- Temp or raw generation artifacts are staged.
