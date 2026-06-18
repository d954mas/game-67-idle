# T0001 Lead Review Packet

Date: 2026-06-18
Task: `T0001 Mine Cards Mining v0.01 first slice`
Status: ready for lead review, not accepted/done yet.

## Review Sheet

Single overview image:

`gamedesign/projects/mine-cards/reviews/t0001_lead_review_sheet_2026-06-18.png`

It includes:

- landscape idle;
- landscape geode/reward;
- portrait idle;
- portrait geode/reward;
- core mining motion sheet;
- 720x480 stress layout.

## What To Judge

Judge the first playable slice as a game screen, not a final art pass:

- Is the first screen recognizably Mine Cards Mining, not a debug tool?
- Is the fixed top actor/action stage understandable: miner -> rock -> progress/reward?
- Does the lower board read as future Melvor-like mechanics without overwhelming the first action?
- Can a new player understand within about 10 seconds what is running, what grows, and what the next goal is?
- Are landscape and portrait both acceptable as authored compositions?
- Is the KayKit/Ozz placeholder miner acceptable for this slice, with custom Mine Cards character art deferred?

## Evidence

Primary screenshots:

- `build/captures/mine_cards_compact_ui_v003_landscape_surface.png`
- `build/captures/mine_cards_compact_ui_v003_landscape_geode.png`
- `build/captures/mine_cards_compact_ui_v003_portrait_surface.png`
- `build/captures/mine_cards_compact_ui_v003_portrait_geode.png`

Motion proof:

- `build/captures/mine_cards_core_moment_v004.gif`
- `build/captures/mine_cards_core_moment_v004_sheet.png`

Live-state proof:

- `build/captures/mine_cards_live_state_v003_state.json`
- `build/captures/mine_cards_live_state_v003_copper_unlocked.png`
- `build/captures/mine_cards_live_state_v003_upgrade_affordable.png`
- `build/captures/mine_cards_live_state_v003_upgrade_purchased.png`
- `build/captures/mine_cards_live_state_v003_small_window_stress.png`

Gates:

- Responsive/product closeout: `gamedesign/projects/mine-cards/reviews/product_gate_t0001_closeout_v001_2026-06-18.md`
- Compact UI gate: `gamedesign/projects/mine-cards/reviews/product_gate_compact_ui_v003_2026-06-18.md`
- Core motion gate: `gamedesign/projects/mine-cards/reviews/core_moment_mining_v004_2026-06-18.md`
- Live-state matrix: `gamedesign/projects/mine-cards/visual/live_state_acceptance_matrix.json`
- Pre-review hygiene: `build/captures/mine_cards_t0001_slice_hygiene.md`
- Snapshot hygiene: `build/captures/mine_cards_t0001_slice_hygiene_snapshot.md`
- Acceptance audit: `gamedesign/projects/mine-cards/reviews/t0001_acceptance_audit_2026-06-18.md`

## Current Verdict

Agent verdict: acceptable for lead review as the first Mining slice.

Why:

- Product/game-loop gate passed with all required live states covered.
- Core moment is proven in motion, not only a static screen.
- Landscape and portrait were captured as separate compositions.
- The screen uses generated/runtime assets and the KayKit/Ozz character path rather than debug-only rectangles.

Known debt:

- KayKit/Ozz miner is still kit placeholder art, not final custom Mine Cards character art.
- Future activity chips are visible placeholders.
- Slice hygiene is now an explicit end-of-experiment snapshot warning, not a
  strict fail:
  `build/captures/mine_cards_t0001_slice_hygiene_snapshot.md`.
- Blank UI kit slice9 runtime crops/previews and decor overlays are still pending.
- Equipment sheet with shadow problems is reference/probe only; T0008 tracks production-safe equipment source art.

## Accept / Reject Criteria

Accept T0001 if:

- the first screen direction is good enough to become the baseline;
- the placeholder miner quality is acceptable as temporary;
- the next work can move to custom character art, reusable renderer path, or production equipment/source prep.

Reject or request one more pass if:

- the first screen still reads as repeated boxes/tooling rather than a game;
- actor/action stage is still not visually tied to Mining;
- the lower board still hides the next goal or upgrade reason;
- portrait or 720x480 stress is not acceptable;
- the lead wants custom Mine Cards character art before calling the first slice accepted.

## After Acceptance

If accepted:

1. Move `T0001` to done with this packet as lead-review evidence.
2. Choose the next slice:
   - custom Mine Cards voxel miner art;
   - `T0008` production equipment source sheet;
   - deeper Mining mechanics only after the visual baseline remains accepted.

If rejected:

1. Keep feature expansion frozen.
2. Record the exact rejected axis in T0001.
3. Fix only the cited visual/product-read issue.
4. Recapture the smallest matching screenshot/motion proof and rerun the relevant product gate.
