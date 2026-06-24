# Blockside Heat GDD

## One-Line Concept

A low-poly Roblox-like open-world crime action prototype about small street jobs, vehicles, NPCs, and readable toy-like chaos in one city district.

## Current Definition Of Done

The first slice is reviewable when a native PC build shows one compact city
block with a readable job objective, a visible player, a nearby enterable car,
NPC presence, simple conflict pressure, HUD state, and a first screenshot/probe
packet compared against the visual target.

## Audience

Casual players. Progression should be clear; controls and moment-to-moment play should stay simple.

## Core Loop

1. Spawn on a city block with one active job and one obvious vehicle.
2. Move on foot or enter the car.
3. Reach the package marker while NPCs/traffic make the street feel alive.
4. Pick up the package, gain heat/wanted pressure, and escape to the drop zone.
5. Receive cash/reputation feedback and a clear next job lock.

## First Playable Slice

- Scene: one native PC city intersection with sidewalk, road, alley pickup, one
  compact car, player, two pedestrians, and one hostile/pursuer NPC.
- Player verbs: walk, steer camera, enter/exit car, drive, pick up package,
  aim/fire a simple toy blaster or baton-hit debug combat verb, finish escape.
- Mission: "Pickup Run" starts active. The player grabs a package, wanted level
  rises from 0 to 1, one pursuer reacts, and the player reaches the drop zone.
- Physics: arcade movement, car acceleration/braking/turning, simple
  collision/knockback. No realistic vehicle simulation in the first slice.
- Weapons: one non-branded simple sidearm or baton action with cooldown and
  clear hit feedback. It is a mechanic proof, not combat depth.
- NPCs: pedestrians wander on short paths; a pursuer starts idle, then chases
  after package pickup.
- UI: job, cash, wanted, vehicle prompt, package/drop marker, feedback toast,
  blocked/locked hint for a future job.
- Proof: native screenshot and a compact smoke/probe showing `job_stage`,
  `in_vehicle`, `cash`, `wanted_level`, and NPC reaction changed.
- Stop condition: if the first screen cannot answer "where am I, what can I do,
  what changed, why continue, why does it look like a game", no new systems or
  districts are added.

## Art Direction Stub

Bright, saturated, friendly, readable at a glance. Avoid realistic, muddy, or
low-contrast presentation. Target composition:
`visual/targets/blockside-heat-first-slice-target.png`. Borrow its blocky city
readability, over-shoulder car framing, mission marker, and HUD hierarchy; do
not copy its generated storefront text or pseudo-brands.

## Story Seed

The player is a new courier for a neighborhood contact called Mina. The first
job is intentionally small: pick up a box behind a pawn shop and deliver it
before a local guard catches up. The story role is to motivate mechanics, not
to add cutscenes before the first loop is playable.
