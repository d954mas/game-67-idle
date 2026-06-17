# Voxelheim - Rescue Implementation Plan

Status: **design-ready, implementation not synced**. The old native idle build
is useful evidence but is not the target. Implement against `gdd.md`,
`data/rescue_loop.json`, and `visual/ui_ux_rescue_spec.md`.

## Goal

Ship one native proof screen for **Voxelheim: Frost Keep Rebuilder**:

- auto-combat against one clear enemy;
- Gold + Frost Blocks + Keep Rank top bar;
- visible Gate -> Forge -> Campfire repair track;
- one dominant next action;
- 3-card loot/rune choice after the first combat packet;
- no minimap, no old four-equal-button shop, no black debug plates.

## Guardrails

- Native PC first. No web detour.
- Visual-first freeze remains active until the rescue screen is readable.
- Real assets through `nt_sprite_renderer`/atlas only.
- Do not add late-game systems before the first screenshot answers: what is
  fighting, what was earned, what to press next, and what will change.
- Old `balance.json` v4 contains useful math/history but has systems not
  implemented in code. Use `data/rescue_loop.json` as the new loop contract.

## Phases

### R0 - Sync source of truth

- Mark old idle constants as legacy in code comments or split rescue constants.
- Add state fields for Frost Blocks, Keep Rank, repaired rooms, pending card
  choice, and selected temporary run cards.
- Keep migration narrow; do not delete old state without a migration plan.

Proof: DevAPI state dump exposes Gold, Frost Blocks, Keep Rank, rooms, pending
choice.

### R1 - Static rescue screen composition

- Remove minimap/brand-heavy plate from first slice.
- Recompose screen around combat + keep repair panel + one next-action area.
- Use current assets as placeholders only if the screenshot reads.

Proof: native screenshot `build/captures/rescue_first_screen.png` plus
readability zoom.

### R2 - First packet loop

- Auto-combat kills 5 enemies in a short packet.
- Drop Gold and Frost Blocks with reward fly-to-counter.
- After packet, pause into 3-card choice.

Proof: DevAPI scenario reaches pending choice; screenshot shows 3 cards.

### R3 - Keep repair loop

- Repair Gate, Forge, Campfire with Frost Blocks.
- Each repair visibly changes the keep panel/scene and unlocks one next system.

Proof: scripted input repairs all 3 rooms; before/after screenshots show visible
change.

### R4 - Compact training

- Add only Power, Speed, Loot as compact secondary training.
- Training unlocks after Forge so first session does not start with a shop wall.

Proof: purchase changes derived stat and visible feedback; UI remains readable.

### R5 - Product gate

- Run native build/probe.
- Run UI readability zoom.
- Run visual/product gate with blockers recorded.
- Only after pass, plan final UI asset generation or deeper systems.

## Art Needs

Immediate:

- frosted top resource bar;
- next-action button;
- 3 card bases with rarity/state overlays;
- room icons/states for Gate, Forge, Campfire;
- Frost Block icon;
- cleaner enemy/hero combat presentation.

Later:

- companion helper;
- repaired keep room variants;
- Avalanche Reset / Frost Shard art;
- event/season visuals.

## Top Risks

1. **Still too generic.** Mitigation: keep repair must be visible in the first
   screenshot.
2. **UI stays unreadable.** Mitigation: readability zoom before any feature
   expansion.
3. **Scope creep.** Mitigation: only Gate/Forge/Campfire, one enemy, one 3-card
   choice pool, three training stats.

## Next Implementation Prompt

"Implement R1/R2 of Voxelheim rescue: recompose the native screen around
combat + Frost Keep repair + one next action, then add the first combat packet
ending in a 3-card choice. Use `data/rescue_loop.json` and
`visual/ui_ux_rescue_spec.md`; do not add late-game systems."
