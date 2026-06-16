# Reference Digest — Idle/Incremental RPG (Voxelheim)

> Required by the AGENTS.md Game/core-loop gate: ground the loop in named refs,
> not invented thin. Mode: **central deconstruction** (the idle loop IS the
> gameplay driver). Sources: genre study + web (see links) and direct knowledge
> of the titles below.

## Sources checked

- **Tap Titans 2** — tap/auto DPS, gold from kills, hero/skill upgrades, **stage
  progression with bosses on a timer**, prestige ("Prestige") for relics →
  permanent artifacts.
- **Idle Slayer** — auto-run + auto-attack stream of enemies, gold + combo,
  upgrades, **prestige (Reborn)** for a multiplier currency.
- **NGU Idle** — nested resets; each prestige tier boosts core stats; deep
  meta-progression.
- **Melvor Idle** — skill trees + global multipliers; idle while away; strong
  retention via many parallel goals.
- Genre anatomy: "produce → upgrade → prestige → repeat" with **offline
  earnings** and exponential number-go-up.

## Observed loop facts (what makes them work)

1. **Always-on auto-progress.** The hero fights on its own; the player's job is
   to make decisions (what to upgrade), not to execute combat.
2. **A near-affordable upgrade at all times.** Gold ticks up; there is always
   something you can *almost* buy → the "one more upgrade" pull.
3. **Exponential walls + soft resets.** Enemy HP/cost scale exponentially; when
   progress stalls, **prestige** trades your run for a permanent multiplier so
   the next run blasts past the old wall.
4. **Bosses are the pacing gate.** Every N stages a timed boss check-points
   progress and creates a "get a bit stronger, then retry" beat.
5. **Offline earnings are the retention hook.** Coming back to "+X gold while
   you were away" + a collect button is the daily re-engagement.
6. **Few currencies, clear roles.** A soft currency (spent every run) + a meta
   currency (permanent). More than ~2-3 early currencies muddies the loop.

## Borrow / Avoid / Copy-risk

- **Borrow:** auto-combat; gold→4 upgrades; timed bosses every 10 stages;
  prestige→meta multiplier; offline earnings + collect popup; one near-affordable
  upgrade always visible.
- **Avoid:** deep skill webs (Melvor) and dozens of currencies — too heavy for a
  first slice; pay-to-win/energy gates; punishing death (idle should never feel
  like a loss).
- **Copy-risk:** none — these are genre-standard mechanics, not a single game's
  signature. Keep the Voxelheim fiction (snowy path to the Frost Keep) as the
  skin so it is not a Tap Titans clone.

## Current-build mismatch (what must change)

The shipped build is a **real-time tap-to-move action-RPG** (you walk the hero,
fight 3 fixed goblins once, win). For idle it must become: hero is **stationary
and auto-fights an endless stream** of monsters that walk down the path; kills
drop **gold**; the hotbar becomes an **upgrade panel**; add **stage counter,
bosses, prestige, and offline earnings**. The art (hero, goblin, keep, path,
backdrop, UI frames) is **reused as-is**.

## Next proof (native screenshot/scenario)

A native idle screen showing: gold counter ticking from auto-kills, an upgrade
panel where buying "Sword +1" visibly raises damage/kill-speed, the stage
counter advancing, and a boss timer — i.e. the **core-loop gate**, not just a
pretty scene.
