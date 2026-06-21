---
type: LeadAcceptancePacket
project: ember-road
task: T0024
surface: town-forge-native-v2
verdict: review
updated: 2026-06-21
---

# T0024 Town Forge Lead Acceptance Packet

Verdict: **REVIEW**

Decision needed: accept the refreshed town-forge direction for Ember Road, or
reject it with the next named visual/UX target. Depth 2 gameplay, economy, and
extra rewards stay frozen until that decision.

## Native Moment

Montage:
`gamedesign/projects/ember-road/reviews/T0024_town_forge_lead_acceptance_moment.png`

Screenshots:

- before click:
  `build/captures/ember-road/state_town_lantern_upgrade.png`
- after click:
  `build/captures/ember-road/state_town_lantern_forged.png`

Player read:

- where: Old Gate Town Forge, with the Mine Lantern upgrade named in the main
  scene;
- action: forge Mine Lantern from the scene workbench/source-derived action
  plaque or the compact rail action;
- result: Mine Lantern becomes ready/equipped and the Depth 2 route is visibly
  lit;
- continuation: the next Old Mine expedition now has an item/depth promise.

## Evidence

- product gate:
  `gamedesign/projects/ember-road/reviews/T0024_town_forge_native_scene_anchor_review.md`
- Y-up layout audit:
  `gamedesign/projects/ember-road/reviews/T0024_town_forge_y_up_layout_audit.md`
- source derivation audit:
  `gamedesign/projects/ember-road/reviews/ember-road-town-forge-v2-source_derivation_audit.md`
- slice9 design policy audit:
  `gamedesign/projects/ember-road/reviews/ember-road-town-forge-v2-slice9_design_policy_audit.md`
- runtime/source manifest:
  `gamedesign/projects/ember-road/data/ember-road-town-forge-v2-asset_manifest.json`

## What This Proves

- The event is no longer only a right-rail form. The forge/worktable/lantern
  objects and source-derived action plaque carry the action in the illustrated
  place.
- The rail is now an item/result summary for the Mine Lantern, not a duplicate
  location panel.
- The screen shows a concrete before/after: cost and action before click,
  equipped lantern and Depth 2 route after click.
- The state title now names the current forge/equipment moment instead of
  falling back to a generic town-square quest-hub label.
- Product gate scores are strict 4/5 across composition, readability,
  controls, action direction, art quality, and audience fit.
- Y-up is authored and audited: larger logical `y` means higher on the game
  screen. Renderer, input, screenshot, and DevAPI conversions are boundary-only.

## Remaining Debt

- Some support pieces still come from the older Old Gate set. The forge,
  worktable, lantern, action plaque, result strip, and badge are source-derived
  town-forge v2 assets.
- This packet does not accept broader Depth 2 content. It only asks whether the
  town-forge visual/UX direction is close enough to resume the next narrow
  slice.
- If rejected, the next step should be another visual-only correction or a
  named replacement target, not new gameplay.

## Decision Options

Accept:

- mark T0024 direction accepted;
- keep Y-up audit as required proof for future visual slices;
- resume only the next narrow native slice, likely Depth 2 visual/UX proof.

Reject:

- name the target mismatch: reference, screen state, UX grammar, or art
  quality issue;
- keep feature/content expansion frozen;
- run another visual-only correction against the named target.
