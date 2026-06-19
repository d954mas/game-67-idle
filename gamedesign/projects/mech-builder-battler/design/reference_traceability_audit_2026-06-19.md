---
type: Project Design Audit
title: Reference Traceability Audit
description: Traceability and gap audit from reference evidence to GDD and first-slice decisions for Mech Builder Battler.
tags: [project, audit, traceability, references, gdd, mvp, mobile, web, mechs]
timestamp: 2026-06-19T00:00:00Z
---

# Reference Traceability Audit

Purpose: show which design decisions are supported by current reference
evidence, which are project translations, and which remain gated before fake
shots or implementation.

This audit routes to the project wiki because it is specific to
`mech-builder-battler`. It does not add reusable design knowledge.

## Inputs Audited

- [Mobile mech analogs](../references/mobile_mech_analogs_2026-06-19.md)
- [Mech Arena deconstruction](../references/mech_arena_deconstruction_2026-06-19.md)
- [CATS deconstruction](../references/cats_deconstruction_2026-06-19.md)
- [Mechangelion deconstruction](../references/mechangelion_deconstruction_2026-06-19.md)
- [Visual reference packet](../references/visual_reference_packet_2026-06-19.md)
- [GDD draft](gdd_draft_2026-06-19.md)
- [First slice spec](first_slice_spec_2026-06-19.md)
- [Lead review packet](lead_review_packet_2026-06-19.md)

## Audit Verdict

The concept is ready for lead review and fake-shot planning, not runtime
implementation.

Strong enough for lead review:

- one-mech PvE-first positioning;
- hangar -> battle -> reward -> upgrade loop;
- semi-auto arena recommendation;
- visible part slots and part-driven build identity;
- no PvP/service/monetization clutter in first slice;
- first fake-shot requirements.

Not strong enough for exact implementation:

- final web/mobile export path;
- exact orientation beyond first fake-shot recommendation;
- exact input feel/tuning beyond the accepted floating joystick / WASD model;
- exact UI layout;
- final art assets;
- exact reference-derived economy/combat pacing.

## Traceability Matrix

| Design Decision | Source Evidence | Evidence Strength | Current Doc Target | Status |
|---|---|---|---|---|
| One large owned mech as first-screen focus | Mech Arena hangar/home grammar; CATS central built object; visual packet first-screen contract | Medium: visible/source-supported, no local capture | GDD, first-slice spec, lead review packet | Ready for lead review and fake shot |
| PvE-first first version | War Robots/Mech Arena PvP service risk; GDD guardrails; user requested casual | Medium: inferred from risks and scope | GDD, first-slice spec | Ready for lead review |
| Build -> battle -> reward -> upgrade loop | CATS build-proof loop; Mech Arena hangar/upgrade; mobile analog cross-pattern | Medium-high: cross-reference pattern, source packet incomplete | GDD, first-slice spec | Ready for lead review |
| Semi-auto arena | Mech Arena active combat + CATS low-agency risk + mobile accessibility | Medium: design translation, not directly observed as our exact model | GDD, first-slice spec, lead packet | Recommended, needs input test |
| Auto-target / no precision aim | Mech Arena targeting assist/control support; casual mobile constraint | Medium: observed controls exist, exact translation inferred | GDD, first-slice spec | Ready for fake shot, needs prototype proof |
| Resources/salvage first reward -> hangar purchase/craft | User direction; CATS build-proof loop; GDD economy guardrails against random pressure | Medium-high: accepted project decision, translated from reference risks | GDD, first-slice spec, lead packet | Accepted on 2026-06-19 |
| Shoulder rockets as first purchasable module | Mech Arena weapon fantasy; visual packet part silhouette rules; first-slice proof need | Medium: accepted project decision from visual/gameplay suitability | First-slice spec, lead packet | Accepted on 2026-06-19 |
| Heat mechanics with `Cooling` UI label | Mech/mechanical fantasy and visual effect readability | Medium: accepted project design inference | GDD, first-slice spec, lead packet | Accepted on 2026-06-19 |
| Industrial mini-boss / Foundry Warden | Mechangelion boss spectacle + copy-risk avoidance | Medium: accepted translation, not source-specific copy | GDD, first-slice spec, lead packet | Accepted on 2026-06-19 |
| Landscape-first fake shots/native slice | Visual packet phone-scale need; battle HUD/control space | Medium: practical visual recommendation accepted for first slice | Lead packet | Accepted on 2026-06-19 |
| No offers/ads before first loop | Store/review complaints across analogs; user casual expectation | High as risk guardrail | GDD, first-slice spec | Accepted guardrail |
| No exact UI/layout copying | Reference deconstruction copy-risk | High as legal/design hygiene | All refs and specs | Accepted guardrail |
| Three fake shots before final art | Visual workflow gate and visual packet | High as process gate | Visual packet, first-slice spec, lead packet | Required before final art |

## Reference-To-Decision Notes

### Mech Arena

Supported decisions:

- hangar as home;
- short battle session;
- active mech combat;
- large readable battle HUD zones;
- special ability as build identity;
- avoid PvP/service clutter in first version.

Evidence limitation:

- The packet does not include owned gameplay capture or first-session timing.
- Exact UI hierarchy and economy pacing remain gated.

Implementation implication:

- Use Mech Arena for direction, not exact HUD, modes, economy, pilots, implants,
  event surfaces, or shop flow.

### CATS

Supported decisions:

- central owned object;
- build -> proof -> rebuild;
- parts as readable physical choices;
- short battle proof after equip.

Evidence limitation:

- Current live tuning and first-session visuals are not fully captured.
- Automated battle is a useful pattern but conflicts with mech-pilot fantasy.

Implementation implication:

- Borrow the proof loop, not the no-agency combat model or timer-box economy.

### Mechangelion

Supported decisions:

- few large buttons;
- simple robot combat;
- boss scale/spectacle;
- weapon/defense upgrade clarity.

Evidence limitation:

- Source packet is weakest: mostly store/secondary screenshots and copy.
- Not enough proof for deep buildcraft or exact UI.

Implementation implication:

- Use only as simplicity ceiling and boss readability reference.

### War Robots

Supported decisions:

- market familiarity with large robot combat;
- weapon/module fantasy;
- role variety;
- anti-patterns around service complexity, PvP pressure, and clutter.

Evidence limitation:

- It is heavier and less casual than the target.

Implementation implication:

- Use as scale/weapon-fantasy reference and anti-pattern, not as first-slice
  structure.

## Decision Strength

### Strong Decisions

These can be treated as accepted unless the lead rejects them:

- One player mech first.
- PvE first.
- Hangar as home.
- Battle reward must return to a visible purchase/craft, slot, or upgrade.
- No PvP/service monetization surfaces in first loop.
- No exact copying of reference UI/silhouettes/names.
- Fake shots required before final art.

### Medium Decisions

These are recommended and should be tested by fake shot/prototype:

- Semi-auto arena.
- Auto-targeting.
- Fixed three-quarter/isometric camera.
- Landscape-first fake shots/native slice.
- Three loadout archetypes for MVP.
- Drone swarm + shield/charger + mini-boss first content.

### Weak / Lead-Dependent Decisions

These should not be silently finalized:

- Shoulder rockets as first purchasable module.
- Heat mechanics with `Cooling` UI label.
- Industrial machine / Foundry Warden as first mini-boss.
- Industrial salvage sport tone.
- Exact input tuning and feel for the accepted floating virtual joystick /
  drag movement zone plus WASD native harness.

## Gaps Blocking Implementation

### Export / Input Gap

Problem:

- User target is mobile/web, but development and playtesting use the native PC
  harness.
- The exact mobile-style input scheme and later web/mobile export path are not
  accepted yet.

Needed:

- native PC slice scope that preserves mobile/web controls, UI scale,
  readability, and session constraints;
- explicit approval before any web/mobile export or frontend/runtime-path work.

### Visual Target Gap

Problem:

- Visual reference packet exists, but no accepted fake shot or local screenshot
  board exists.

Needed:

- three fake shots or equivalent accepted visual targets:
  hangar, battle, reward/upgrade.

### Source Evidence Gap

Problem:

- Central reference docs still lack owned gameplay capture/video timestamps and
  local screenshot boards.

Needed:

- either capture stronger evidence for exact UI/economy/combat claims, or keep
  implementation intentionally translated rather than copied.

### Lead Acceptance Gap

Problem:

- GDD/spec/lead packet are in review, not accepted.

Needed:

- lead accepts or edits the decision checklist in
  [Lead review packet](lead_review_packet_2026-06-19.md).

## Next Best Work

Recommended next sequence:

1. Lead accepts or edits the decision packet.
2. Create/accept three fake shots.
3. Scope the native PC slice with mobile-style controls/readability.
4. Create one scoped implementation task from the accepted first-slice spec.
5. Capture screenshot proof against the fake-shot targets.

## Stop Rules

Do not proceed to runtime implementation if:

- native PC slice scope is still unresolved;
- lead decisions are not accepted or edited;
- no fake-shot/visual target exists;
- the implementation would copy exact reference UI/economy/silhouettes;
- first slice starts adding PvP, pilots, implants, ads, offers, battle pass, or
  random chest timers.

## Review Summary

The design direction is coherent and sufficiently decomposed for review:

- references are collected and labeled;
- core loop, gameplay, mechanics, and meta are decomposed;
- first-slice spec exists;
- lead decision packet exists;
- implementation gates are explicit.

The design is not done enough for final implementation because visual target,
native PC slice scope, stronger source evidence, and lead acceptance are still
missing.
