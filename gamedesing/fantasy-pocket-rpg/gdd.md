# Fantasy Pocket RPG GDD Slice

## Current Gate

- Concept gate: partial, direction accepted for camp-as-preparation.
- Reference gate: first compact pass complete.
- Visual gate: not started; next needed proof is a gameplay fake shot.
- Slice gate: first playable slice defined below.
- Handoff gate: out of scope until visual direction is accepted.

## Game Summary

A compact fantasy RPG for mobile and PC. The player explores a dangerous province through map locations and first-person/illustrated encounters, gains loot and reputation, then returns to safe camp points to recover, craft, speak with companions, and choose the next upgrade.

## First 30 Seconds

1. Player sees a province map with one available route: `Old Road -> Moss-Covered Ruins`.
2. Top bar shows `Gold`, `Supplies`, `Health`, and `Resolve`.
3. Quest card says: `Find the missing scout near the old stones`.
4. Player taps `Travel`.
5. First encounter opens in a first-person illustrated scene: broken road, standing stones, distant ruin.
6. Player chooses `Search the stones` or `Move toward the ruins`.
7. A small reward/pressure appears: `Found herbs +1`, `Heard wings above: Dragon Omen +1`.

## First 5 Minutes

1. Travel to first location.
2. Resolve one event choice.
3. Fight one simple enemy or avoid it through a stat check.
4. Loot `Rusty Blade` and `Dragon-Marked Shard`.
5. Health or Resolve drops enough to justify resting.
6. Safe campfire unlocks after the encounter.
7. In camp, player:
   - rests;
   - crafts `Minor Healing Draught` from herbs;
   - equips or compares loot;
   - talks to first companion about the dragon mark;
   - chooses one perk.
8. Returning to map reveals the next node: `Hunter's Ford`.

## Core Loop

```text
choose destination -> encounter/event -> fight/check/choice -> loot/resources/status change -> safe camp preparation -> upgrade/perk/story beat -> unlock next destination
```

## Camp Rule

Camp is a preparation/narrative screen, not a base builder.

Access:

- safe map nodes;
- cleared locations;
- campfires/shrines/inns;
- world-map travel pauses.

Not allowed:

- during combat;
- in hostile rooms;
- as a universal escape button from danger.

Camp actions:

- rest and recover;
- craft simple consumables;
- repair/upgrade selected gear;
- level up/perk choice;
- talk to companions;
- inspect dragon relic/egg/mark;
- manage inventory before next expedition.

## First-Slice Stats

- Health: combat survival.
- Resolve: mental stamina for fear, magic, persuasion, and resisting curses.
- Supplies: travel/rest/camp pressure.
- Gold: shops, repairs, services.
- Dragon Omen: mystery/progression flag, not a spendable currency in first slice.

## First-Slice Activities

### Travel

- Input: choose map node.
- Cost: 1 Supplies for dangerous route, 0 for safe route.
- Reward: enters encounter, may reveal danger.
- Failure/blocked state: low supplies warns but does not hard-lock first slice.

### Search

- Input: choose search action in encounter.
- Cost: small Resolve risk or trap chance.
- Reward: herbs, gold, lore clue, hidden route.
- Blocked state: needs light/tools/perk for some searches later.

### Fight

- Input: attack, defend, use item, attempt skill.
- Cost: Health/Resolve risk.
- Reward: loot, XP, location progress.
- Blocked state: low Health suggests retreat to safe camp if available.

### Camp Rest

- Input: enter camp from safe state.
- Cost: Supplies.
- Reward: Health recovery, companion scene, craft access.
- Blocked state: unsafe location or no supplies for full rest.

## First Upgrade

`Trail Herbalist I`

- Unlock condition: collect herbs in the first ruins.
- Cost: 2 Herbs + camp access.
- Effect: healing draught restores more Health.
- Why player wants it: first tangible camp payoff.
- Visible before/after: potion card changes from `Heal 10` to `Heal 15`.
- Affected activity: Camp Crafting.

## Companion First Beat

First companion role: a scout, mercenary, or apprentice mage who knows local legends but does not understand the dragon mark.

First camp conversation:

- confirms the relic is old and dangerous;
- offers a practical next step;
- introduces relationship tone;
- unlocks the next map clue.

## Dragon Progression

Do not start with a dragon pet. Start with mystery:

1. strange mark/relic;
2. dreams/omen;
3. egg/shard awakening;
4. small companion or spirit;
5. combat/travel utility later.

The dragon system should support exploration and hero identity, not become a collection game.

## Visual Direction For First Fake Shot

Required screen elements:

- first-person fantasy ruin or province map;
- top bar: Health, Resolve, Supplies, Gold;
- quest objective;
- primary actions: Travel/Search/Fight/Camp depending state;
- loot/event result;
- visible camp availability state;
- dragon-mark mystery hint;
- modern readable mobile-first UI.

## Risks

### Camp Steals Focus

- Type: design/UX.
- Test: fake shot and first slice must show camp as return/prep after adventure, not main screen.
- Stop rule: if the most interesting screen is camp management, reduce camp scope.

### Too Much Survival

- Type: fun risk.
- Test: first 5 minutes should feel like adventure with pressure, not resource punishment.
- Stop rule: if Supplies/Health dominate every action, simplify.

### Visual Identity Too Generic

- Type: art/product risk.
- Test: fake shot must include a memorable dragon-mark/fantasy province signal.
- Stop rule: if it looks like generic RPG UI, define stronger art motifs before implementation.

## Next Gate

Visual gate: create one gameplay fake shot for the first 5 minutes and review it before expanding content or implementation.

