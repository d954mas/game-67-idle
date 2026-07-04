---
type: Project Source Notes
title: Combat Stats Reference Packet
description: Source notes for rb-dark-rpg first autobattle stats and complexity limits.
tags: [project, references, combat, stats, autobattle]
timestamp: 2026-07-04T00:00:00Z
game_id: rb-dark-rpg
status: draft-source
source_quality: mixed
checked: 2026-07-04
---

# Combat Stats Reference Packet

Scope: choose a simple first-autobattle stat model for `Дракон не вернулся`.

This packet supports first-slice combat design. It is not a full balance pass:
no gameplay video timing, telemetry, or playable build capture was analyzed.

## Source Matrix

| Source | Link/Path | Quality | Checked | Proves | Does Not Prove |
|---|---|---|---|---|---|
| V0 concept/GDD draft | `games/rb-dark-rpg/design/knowledge/sources/v0_concept_gdd_draft.md` | user-provided | 2026-07-04 | First combat should be pure autobattle with HP, Damage, Armor, Attack Speed, Crit Chance. | Final numbers, pacing, full balance curve. |
| V0 concept idea | `games/rb-dark-rpg/design/knowledge/v0_concept_idea.md` | user-provided distilled source | 2026-07-04 | Player choice lives in preparation, gear, route, quest, and risk, not active combat input. | Exact UI layout or enemy roster. |
| Stone Story RPG Steam page | `https://store.steampowered.com/app/603390/Stone_Story_RPG/` | official store | 2026-07-04 | Auto-RPG can keep player agency through gear, potions, timing, and item swaps while AI handles combat. | It does not justify adding scripting or deep automation to this jam act. |
| Loop Hero Steam page | `https://store.steampowered.com/app/1282730/Loop_Hero/` | official store | 2026-07-04 | Automated expedition combat can be made strategic through equipment, danger placement, loot, and camp upgrades. | It does not justify deckbuilding or roguelike loop structure here. |
| DragonFable official About page | `https://www.dragonfable.com/About` | official | 2026-07-04 | Browser RPG appeal can sit on character building, gear, classes, fast battles, allies, and foes. | Its active skills/classes are too much for v1 autobattle. |
| Darkest Dungeon Steam page | `https://store.steampowered.com/app/262060/Darkest_Dungeon/` | official store | 2026-07-04 | Dark RPG combat readability benefits from classes, flawed heroes, recovery town, turn-based combat, and stress/danger identity. | Stress, permadeath, camping, diseases, and class depth are too complex for this first slice. |
| Battle Brothers Steam page | `https://store.steampowered.com/app/365360/Battle_Brothers/` | official store | 2026-07-04 | Gear, fatigue, morale, injuries, hit chances, contracts, and enemy type matching create deep tactical RPG decisions. | These systems are intentionally out of scope for a simple first autobattle. |
| Legend: Legacy of the Dragons official Character Attributes | `https://warofdragons.com/info/library/index.php?obj=cat&id=429` | official library | 2026-07-04 | `Strength` increases physical damage; 10 Strength increases average physical damage by 1. `Vitality` increases life by 1 per point. `Agility/Dexterity` increases physical dodge chance. `Intuition` increases physical critical chance. `Protection` increases physical block chance. Critical hits triggered by Intuition cannot be blocked. | Exact server formulas for chance curves, block resolution, damage ranges, and equipment itemization are not fully exposed. |
| Legend: Legacy of the Dragons official Red Sets | `https://warofdragons.com/info/library/index.php?obj=cat&id=404` | official library | 2026-07-04 | Equipment sets are class-shaped: Dodger-style bonuses use Strength + Agility; Heavyweight-style bonuses use Protection + Vitality; Bonecrusher-style bonuses use Strength + Intuition. | Set bonuses alone do not explain full combat math. |
| Legend: Legacy of the Dragons official Super-Blows | `https://warofdragons.com/info/library/index.php?obj=cat&id=377` | official library | 2026-07-04 | Combat depth includes discovered/available sequences of blows, basic blow damage, bonus super-blow damage, and effects such as damage absorption or loss of life. | Too active and complex for pure v1 autobattle unless converted into passive traits or enemy tags. |
| Legend: Legacy of the Dragons official Talents | `https://warofdragons.com/info/library/index.php?obj=cat&id=378` | official library | 2026-07-04 | Later progression includes many specialized knobs: Protection, Intuition, Agility, trauma/antitrauma, initiative, critical damage, critical defense, poison, magical protection, block damage reduction, vampirism, and regular/lasting damage protection. | This confirms depth direction, not a first-slice stat list. |
| Path of Exile Armour wiki | `https://www.poewiki.net/wiki/Armour` | community mechanics wiki | 2026-07-04 | Armour can be a rating compared against incoming hit size: it is strong against small hits and weaker against large hits. | Full PoE mitigation order, damage types, resistances, and endgame scaling are too complex for Act I. |
| Path of Exile Strength wiki | `https://www.poewiki.net/wiki/Strength` | community mechanics wiki | 2026-07-04 | Strength can be a permanent character stat that contributes to life and melee physical scaling while weapons still carry base damage. | PoE's percent scaling and passive tree do not fit the first browser-RPG slice. |
| League of Legends Armor wiki | `https://leagueoflegends.fandom.com/wiki/Armor` | community mechanics wiki | 2026-07-04 | Armour can be modeled as effective health with `damage / (1 + armor / 100)`. | This curve needs larger armor numbers before early low-damage fights feel different. |
| Battle Brothers Damage wiki | `https://battlebrothers.fandom.com/wiki/Damage` | community mechanics wiki | 2026-07-04 | Weapon damage, armor damage, direct damage, skill multipliers, injuries, and traits create a deep combat model. | It is too detailed for a single-character first autobattle. |
| Loop Hero Equipment wiki | `https://loophero.fandom.com/wiki/Equipment` | community mechanics wiki | 2026-07-04 | Equipment slots can have clear main stats: weapons give damage, armor gives HP, shields give defense. | It does not provide enough direct formula detail for our combat math. |

## Observations

- `user-provided` V0 already chooses the right first stat cluster: HP, Damage,
  Armor, Attack Speed, Crit Chance.
- `observed` auto-RPG refs put agency before/during automation through gear and
  small timing choices; they do not require direct movement or skill bars.
- `observed` old browser RPG refs reward gear, XP, gold, and quest completion;
  that maps cleanly to a first contract loop.
- `observed` deeper tactical/dark RPG refs add many interesting stats, but the
  cost is high UI load: hit chance, dodge, stress, morale, fatigue, injuries,
  classes, resistances, and status effects.
- `observed` PoE-style armour is not a flat subtraction. It compares armour
  against the incoming hit, so small hits are reduced more than large hits.
- `observed` LoL-style armour is easier to explain as effective HP, but with
  small early numbers it can feel inert unless armor values are inflated.
- `observed` Battle Brothers shows why not to copy tactical formulas wholesale:
  weapon damage, armor damage, direct damage, hit locations, perks, injuries,
  and traits become a full simulation.
- `observed` Legend: Legacy of the Dragons does not present armour as one
  visible "minus damage" formula. Its exposed physical attributes are split into
  damage, life, dodge, crit, and block:
  - `Strength`: physical damage, with 10 Strength adding 1 average physical
    damage;
  - `Vitality`: life, 1 point per 1 HP;
  - `Agility/Dexterity`: chance to dodge physical attacks;
  - `Intuition`: chance to land physical critical blows;
  - `Protection`: chance to block physical attacks;
  - Intuition crits cannot be blocked.
- `observed` Legend's equipment reinforces archetypes through stat pairs:
  Dodger = Strength + Agility, Heavyweight = Protection + Vitality,
  Bonecrusher = Strength + Intuition.
- `observed` Legend's combat depth also comes from super-blow sequences and
  talent blocks, not just the base hit formula.

## Application To RB Dark RPG

### Borrow

- Keep combat automatic, but make preparation obvious.
- Use gear as the player's first meaningful combat choice.
- Show why a fight is easy, fair, risky, or deadly before the player starts it.
- Let rewards improve one visible stat at a time in the first minutes.

### Avoid

- Accuracy, dodge, resistances, elemental damage, stamina, mana, morale,
  injuries, stress, status effects, pets, allies, formations, and active skills
  in the first playable.
- Percent-heavy UI. Use concrete numbers and labels first.
- Hiding the reason for losing. If the player loses, show the stat mismatch.

### Recommended First Stat Model

Player-facing primary stats:

1. `HP` / `Здоровье` - how much damage the seeker can survive.
2. `Damage` / `Урон` - base damage per hit.
3. `Armor` / `Броня` - armor rating compared against the incoming hit size, so
   early armor changes small hits without making the unit immune.

Secondary stats, visible in details or later gear:

4. `Attack Interval` / `Скорость` - seconds between attacks. Show as `удар раз
   в 2.0 с`, not as an abstract percentage.
5. `Crit Chance` / `Крит` - chance to double one hit. Keep low and optional in
   the first slice.

Internal derived values:

- `attack_power = strength + weapon_damage + bonus_attack_power`
- `armor_reduction = armor / (armor + 5 * attack_power)`
- `damage_per_hit = max(1, ceil(attack_power * (1 - armor_reduction)))`
- `crit_damage = damage_per_hit * 2`
- `dps = expected_damage_per_hit / attack_interval`
- `time_to_kill = defender.hp / attacker_dps`
- `threat_label = easy | fair | risky | deadly`

### Strength vs Weapon Damage

Adopt:

- `Strength` is the character stat that levels and rare story rewards improve.
- `Weapon damage` is the item stat that swords/axes/etc. provide.
- `Damage` / `Урон` in the UI is derived: `Strength + weapon damage + bonuses`.

This keeps progression readable: the player can understand both "I got
stronger" and "this sword hits harder" without maintaining two unrelated damage
stats.

### Armor Formula Choice

Adopt the hit-size curve:

```text
armor_reduction = armor / (armor + 5 * attack_power)
damage_per_hit = max(1, ceil(attack_power * (1 - armor_reduction)))
```

Reject for v1:

- pure flat subtraction: too brittle at low numbers and can make small enemies
  deal only 1 forever;
- generic effective-health curve `damage / (1 + armor / 100)`: clean, but early
  armor feels too weak unless the UI shows inflated armor values;
- Battle Brothers-style layered armor/direct-damage simulation: too much for the
  first browser-RPG screen.

### Legend: Legacy Of The Dragons Takeaway

If the goal is to lean toward `Legend: Legacy of the Dragons`, do not treat
`Protection` as armor absorption. Treat it as a block-chance stat, then keep
physical durability as a combination of:

- `Vitality` / HP;
- `Protection` / block chance;
- armor equipment as the main source of `Protection` and `Vitality`;
- class identity through stat pairs:
  - Dodger: Strength + Agility;
  - Heavyweight: Protection + Vitality;
  - Bonecrusher: Strength + Intuition.

For `rb-dark-rpg` first slice, the full Legend model is too complex as-is. The
best borrow is the role split: Strength for damage growth, Vitality for HP,
Protection for blocking or guarding, Intuition for crit. Defer dodge, class
sets, active super-blows, and talent blocks.

## Complexity Gate

First build should expose only:

- three large stats: HP, Damage, Armor;
- two small detail stats: Speed, Crit;
- a pre-fight verdict: `Легко`, `Ровно`, `Риск`, `Смертельно`;
- one reason line: `броня врага гасит твой урон`, `враг бьет быстрее`, or
  `у тебя мало HP`.

Add new stats only after the player has completed `Хлеб для Поста`.
