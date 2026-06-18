# Melvor Idle History Deconstruction - 2026-06-17

Status: `early history deconstruction`.

Purpose: understand how Melvor Idle began, so this project does not attempt to
clone the current huge game in one step.

## Sources Checked

- Steam product page: https://store.steampowered.com/app/1267910/Melvor_Idle/
- Live site: https://melvoridle.com/
- Current skill wiki: https://wiki.melvoridle.com/w/Skills
- v0.01 changelog: https://wiki.melvoridle.com/w/V0.01
- v0.02 changelog: https://wiki.melvoridle.com/w/V0.02
- v0.03 changelog: https://wiki.melvoridle.com/w/V0.03
- v0.04 changelog: https://wiki.melvoridle.com/w/V0.04
- v0.05 changelog: https://wiki.melvoridle.com/w/V0.05
- v0.06 changelog: https://wiki.melvoridle.com/w/V0.06
- v0.08 changelog: https://wiki.melvoridle.com/w/V0.08
- v1.0 changelog: https://wiki.melvoridle.com/w/V1.0

## Timeline

| Version | Date | What changed | Design lesson |
|---|---:|---|---|
| v0.01 | 2018-11-23 | Initial release. One skill: Woodcutting. 9 tree species, 9 axes, GP, auto-cutting milestones, random events, autosave. | Start tiny, but include progression, automation, and reward variance. |
| v0.01.1-v0.01.2 | 2018-11-23/24 | Feedback fixes: bank capacity values, clearer axe upgrades, progress bars. | UI readability and idle tuning were fixed immediately. |
| v0.02 | 2018-11-30 | Woodcutting Prestige, bird nests, stats, random-event UX, balance, preview of Fishing/Firemaking. | Deepen the first skill before adding a large matrix. |
| v0.03 | 2018-12-07 | Added Fishing and Firemaking. Firemaking consumes Woodcutting logs. New Bank and Shop UI. | Add skills that consume or support existing resources. |
| v0.04 | 2019-09-11 | Ground-up redesign. Added Cooking, Mining, Smithing, official domain, mastery alpha, settings, wider shop/bank changes. | A larger idle game may need an early structural redesign before scaling. |
| v0.05 | 2019-09-21 | Combat update: Attack, Strength, Defence, Hitpoints, automated attacks, equipment/food setup, combat areas. Mining gained gem rocks and mastery benefits. | Combat arrived after resource/crafting structure, not as the first loop. |
| v0.06 | 2019-09-28 | Added Thieving and Farming. Farming was described as the first offline skill. More mastery and bank improvements. | Offline/passive systems can come after the active loop and resource base. |
| v0.08 | 2019-10-29 | Added Ranged combat, Fletching, Crafting, ammo, rings/necklaces, more equipment slots, smithing redesign. | Combat breadth follows production-chain needs such as bows, arrows, armour, and jewelry. |
| v1.0 | 2021-11 | Full release/endgame-scale content and high-level combat balancing. | Current Melvor scale is multi-year accumulation, not a first milestone. |

## Central Deconstruction

Melvor's early shape is:

```text
single skilling action -> reward -> tool/automation upgrade ->
resource consumer skill -> bank/shop UI -> combat after gear/resources matter
```

What matters for us:

- The first skill was useful alone, but pointed toward future skill interaction.
- Progress bars, autosave, bank capacity, and upgrade clarity were not polish;
  they were early product requirements.
- Random events gave idle actions a small active beat without changing the
  genre.
- The first expansion did not immediately add combat. It added skills that
  made existing resources meaningful.
- Combat arrived once equipment, food, and resource production could support it.

## Borrow

- One-skill start.
- Progress timer plus reward log.
- Tool upgrade as first visible power increase.
- Mastery/node-specific progress.
- Rare optional event to break pure waiting.
- Bank/inventory as later scale support.
- Skills that feed each other rather than unrelated minigames.

## Avoid

- Copying current Melvor's full tab density into the first screen.
- Adding combat before resources and upgrades have a reason.
- Adding many skills at once.
- Using the old card board because the project name says cards.
- Treating offline progress as v0.01 scope before the live screen is fun and
  readable.

## Translation To This Project

Melvor's Woodcutting-first start becomes our Mining-first start.

`Mine Cards v0.01` should include:

- one skill: Mining;
- 2 visible nodes plus 1 locked node;
- one tool upgrade: Copper Pickaxe;
- one rare event: Geode/Rich Vein;
- level and mastery progress;
- clear reward log and upgrade before/after.

Defer:

- Smithing until Mining has enough resources to consume;
- equipment until Smithing has a reason;
- combat until equipment has a reason;
- cards until combat needs a visual mode;
- offline progress until the active loop and save model are stable.

## Current-Build Mismatch

There is no current native gameplay screen yet. Against the imported docs/art,
the mismatch is:

- old docs are card-run-first;
- new direction is one-skill idle-first;
- old art shows combat/card surfaces before production surfaces;
- old UI includes multiple currencies and modals before the core Mining loop;
- old hero likely needs public-safe redesign.

## Next Native/Fake-Shot Proof

Create and review a Mining v0.01 fake shot before implementation:

```text
Blocky miner + active Copper/Stone node + progress timer + reward log +
pickaxe upgrade + one locked/deeper node + optional geode callout.
```

The proof passes only if a new player can identify what is running, what grows,
what to upgrade, and why to keep mining.
