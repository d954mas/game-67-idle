# Voxelheim — Gameplay Depth Plan (toward top-ref level)

From `reviews/idle_vs_refs_review.md`: the loop is correct but THIN vs Clicker
Heroes / Tap Titans 2 — too few decisions, no active layer, shallow meta. These
are the depth additions (grounded in the refs) for the next gameplay pass, after
the visual pass lands. Numbers go in `data/balance.json` (the core-loop gate).

## 1. Active layer — a tappable ability (CH click-damage / TT2 skills)
Pure-idle is the least sticky end. Add ONE active ability so there's a
moment-to-moment action:
- **"Frost Fury"**: tap a big button -> a burst (e.g. instant 10x current DPS as
  one hit, or +200% damage for 5s), then a ~20s cooldown shown as a radial/bar.
- Optional later: a 2nd skill (gold rush: +100% gold 8s). Keep to 1-2.
- Borrow: CH/TT2 reward active play with a power spike but never REQUIRE it (idle
  must still progress hands-off).

## 2. Decision-richness — more upgrades + a prestige TREE
Refs hold players with a LONG upgrade list + a meta tree:
- **Upgrades:** add 2-3 beyond the current 4 (Sword/Boots/Armor/Luck): e.g.
  **Crit** (crit chance + crit x), **Multi-strike** (chance to hit twice),
  **Greed** (gold x). 6-7 upgrades = more "what next" decisions; keep them
  multiplicative where they touch damage/gold.
- **Prestige tree (Frost Shards):** expand the 4 flat shard upgrades into a small
  TREE (~6-8 nodes), some GATED behind earlier nodes / a prestige count, e.g.
  Sharper Steel -> Frostbite (crit synergy); Rich Veins -> Treasure Maps; Head
  Start -> Expedition (start further). Gating creates the CH-Ancients / TT2-
  artifact "what to invest in" depth.

## 3. Companions (optional, bigger — CH heroes)
The CH core is a LIST of heroes each adding DPS, leveled with gold. A small
version: 2-3 unlockable companions (a wolf, an archer) that add auto-DPS, each
leveled with gold. High stickiness but a bigger build — do AFTER 1 & 2 if the
slice still feels thin.

## Pacing target (refs)
First session ~10-20 min to first prestige (we have ~that). Each prestige +50-
200% (Pecorella) — verify after the tree lands. Keep offline + the "one near-
affordable upgrade always" pull.

## Order for the next pass
1) Active ability (biggest feel gain, smallest build).
2) +2-3 upgrades + the prestige tree (decision-richness).
3) Companions only if still thin.
Each: numbers in balance.json, headless probe asserts, screenshot, game-design
critic on "is there more to DO / decide".
