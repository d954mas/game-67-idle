---
type: Reference Deconstruction
title: Loop Sort-Puzzle — Central Deconstruction (for Critter Corral)
description: Gameplay reference digest for the loop sort-puzzle driver behind Critter Corral.
tags: [reference, sort-puzzle, critter-corral]
checked: 2026-06-15
---

# Loop Sort-Puzzle — Reference Digest

- **Mode:** central deconstruction (this is the gameplay driver, not a small UI
  detail). Not a release-critical deep dive yet.
- **Sources checked:** AppMagic "VOODOO's New Big Three" (detailed play
  descriptions of Marble Sort! and Sand Loop) -> `gamedesign/sources/voodoo_new_big_three_hybrid_casual_2026-06-15.md`;
  distilled rules in `gamedesign/knowledge/hybrid_casual_patterns.md`. Evidence
  is article-described play, not first-hand frame capture (acceptable for a
  primitive prototype; upgrade to captured frames before final-art claims).

## Observed facts (genre)

1. Items arrive continuously; the player routes each by COLOR into a matching
   container before space/time runs out. Learn-in-seconds; depth comes later
   (tighter space, new mechanics, escalating chaos).
2. The praised quality is the SATISFYING / ASMR physics of the core action
   (marbles roll/click/tumble; sand melts and distorts). "Satisfying sells."
3. Difficulty and monetization come from HONEST mistakes (Sand Loop's
   inconsistent grain counts; chaotic layouts), not punishing precision.
4. The standout titles add ONE physics twist that changes the decision (Marble
   Sort!: marbles are thrown so they bounce/mix), not a new genre.

## Screen grammar

One readable play-field; obvious color->container mapping; immediate per-sort
feedback; escalating spawn/pressure. One dominant goal on screen.

## Borrow / Avoid / Copy-risk

- **Borrow:** a physically SATISFYING core moment; learn-in-seconds; exactly one
  twist; calm-but-deepening difficulty; one readable goal per screen.
- **Avoid:** cloning a title's theme/levels/identity; ad-spam monetization;
  reinventing the genre; punishing controls.
- **Copy-risk:** low — our twist (the sorted things are ALIVE and you only
  influence them, never place them; open-pasture herding) is distinct from any
  named title. Keep theme/skin our own (decided later, by Codex).

## Current-build mismatch

No build yet (Stage 0). Nothing to compare. The genre's satisfying-core lesson
maps directly to our pillar "satisfying chain": the herd-into-pen pop + same-
color chain must be the felt moment.

## Next native proof

A native screenshot (and short capture) of the PRIMITIVE core moment — luring a
clump of critters so a matching color pops into its pen with a chain — judged
qualitatively against the concept fake-shot DIRECTION (`../concept.md`). Code is
unlocked for that core-moment slice; broad content stays gated on it feeling good.
