# Melvor Idle Reference Intake - 2026-06-17

Status: `source pack / early synthesis`, not final balance or UI copy target.

Study mode: `central deconstruction` for core loop and progression. This packet
is enough to choose the first project direction: a tiny-start block mining idle
RPG. It is not enough to implement final pacing, exact numbers, or a full skill
matrix.

## Reference Question

If the old `Mine Cards` card mechanic is removed, what Melvor-like game can use
the archived blocky mine art direction without starting too large?

## Source Packet

| Source | Quality | Checked | What it proves | What it does not prove |
|---|---|---:|---|---|
| Melvor Idle Steam page: https://store.steampowered.com/app/1267910/Melvor_Idle/ | marketplace/store | 2026-06-17 | Current positioning: RuneScape-inspired idle/incremental RPG, 20+ skills, interlocking skills, offline progress, bank/inventory, combat. | Early prototype scope or exact balance. |
| Melvor Idle site: https://melvoridle.com/ | primary/live app | 2026-06-17 | Current live app shell/version and dense app UI shape. | Early development history. |
| Melvor Idle Wiki Skills: https://wiki.melvoridle.com/w/Skills | official/community wiki | 2026-06-17 | Current skill taxonomy and how skills feed each other. | First-slice onboarding and v0.01 scope. |
| Melvor Idle Wiki v0.01-v0.08 changelogs | official/community wiki | 2026-06-17 | Melvor started with one skill, then deepened and expanded incrementally. | Exact implementation architecture. |
| Mine Cards old GDD import: `../sources/old_gdd_import_2026-06-17.md` | user-provided | 2026-06-17 | Original portrait/blocky art intent and old card screens. | Current Melvor-like product direction. |

## Observed / Source-Backed Facts

- Steam positions Melvor as a RuneScape-inspired idle/incremental RPG with a
  feature-rich long-term skill set and 20+ skills.
- Steam highlights interlocking skills: work put into one skill benefits other
  skills.
- Steam also names offline progression, bank/inventory, combat, and expansions
  as current-scale features.
- The wiki's current skill list is large, but early changelogs show that the
  game began much smaller.
- v0.01 was Woodcutting-only: trees, axes, GP, auto-cutting milestones, random
  events, autosave.
- v0.02 deepened Woodcutting before adding real new skills: prestige, bird
  nests, statistics, random-event UX, and balance changes.
- v0.03 added Fishing and Firemaking, plus Bank/Shop UI revisions.
- v0.04 was a ground-up redesign and added Cooking, Mining, and Smithing.
- v0.05 added Combat after the resource/skill spine existed.

## Translation To Mine Cards

The useful pattern is not "copy Melvor's current huge UI". The useful pattern is:

```text
start one skill -> make the reward/upgrade loop clear -> deepen it ->
add a second skill that consumes the first skill's resources -> add combat later
```

For this project, translate Woodcutting-first into Mining-first:

```text
choose mining node -> progress timer fills -> ore/xp/mastery enter inventory ->
rare geode/rich vein can appear -> upgrade pickaxe -> unlock deeper node
```

The old Mine Cards archive can support future surfaces:

- character/equipment art becomes later gear screen input;
- skill screen art becomes later progression UI input;
- chest/victory/defeat art becomes later reward/combat input;
- old card battle screens are historical and not v0.01 direction.

## Recommended Game Direction

Working title: keep `Mine Cards` temporarily, but define the product as
`Block Mine Idle` internally until the name is revisited.

Genre: blocky idle skilling RPG.

Core fantasy: run a voxel miner who trains Mining, improves tools, discovers
rare finds, and eventually crafts gear for dangerous lower layers.

First-session promise:

- One hero.
- One skill: `Mining`.
- One resource chain: `Stone/Copper -> Copper Pickaxe`.
- One rare moment: `Geode`.
- One visible growth: mining becomes faster or unlocks the next node.

## Borrow / Avoid / Copy Risk

Borrow:

- skill-first progression;
- visible progress timers;
- mastery/level labels;
- resource chains;
- bank/inventory as later structure;
- the idea that each added skill should support another system.

Avoid:

- current Melvor-scale skill matrix in v0.01;
- exact Melvor UI layout, names as a complete set, and balancing;
- combat before the resource spine matters;
- old card mechanics as first loop;
- Minecraft-specific characters, enemies, block silhouettes, logo treatment, or
  Steve proportions.

## Current Mine Cards Mismatch

Current project state has archived card-run docs and PSD comps, but no current
native build or gameplay screenshot. Mismatch audit is therefore against docs
and art inventory:

- Old concept was card-run-first; new direction is skill/activity-first.
- Old UI showed many currencies/modals; v0.01 needs only ore, coins, XP, and
  mastery.
- Old hero is too close to Minecraft/Steve for public art.
- Old battle-card screens are deferred.
- Old equipment/skills screens are useful only after Mining proves the spine.

## Next Proof

Generate or select one fake shot:

`Mining v0.01 Activity Screen`: blocky miner, active Copper/Stone node, progress
bar, reward log, pickaxe upgrade, and one locked/deeper node.

Do not generate card/combat fake shots until Mining v0.01 direction is accepted.
