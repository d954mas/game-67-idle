# Combat Spec: First Encounter

## Goal

Prove that danger, damage, loot, and camp recovery work in the first playable slice without building a full RPG combat system.

## First Enemy

`Ruin Wolf`

- Role: fast low-threat tutorial enemy.
- HP: 12.
- Attack: `Bite` for 4 Health damage.
- Pattern: attacks every turn unless the player used `Defend`.
- Reward: `Rusty Blade`, 4 Gold, location progress.

## Player Baseline

- Health: 30/30.
- Resolve: 10/10.
- Weapon: `Old Knife`, 5 damage.
- Consumable: optional `Minor Healing Draught`, heals 10 before upgrade and 15 after `Trail Herbalist I`.

## Player Actions

### Attack

- Input: tap/click `Attack`.
- Effect: deal 5 damage to enemy.
- Feedback: enemy HP bar drops, small hit number.

### Defend

- Input: tap/click `Defend`.
- Effect: next incoming damage reduced by 50%, rounded down.
- Feedback: shield stance icon, turn log says `Guarded`.

### Use Draught

- Input: tap/click `Use Item`.
- Effect: restore Health by potion amount, capped at max Health.
- Blocked state: no potion available.
- Feedback: Health bar rises, item count decreases.

### Skill Check: Calm The Beast

- Input: tap/click `Skill`.
- Cost: Resolve -2.
- Success: 60% base chance; ends encounter without loot gold but grants location progress.
- Failure: enemy immediately uses `Bite`.
- Feedback: result toast with success/fail and Resolve delta.

## Turn Order

1. Player chooses action.
2. Player action resolves.
3. If enemy HP is 0, combat ends with victory.
4. If combat is not over and player did not bypass, enemy action resolves.
5. If player Health is 0, combat ends in forced retreat.
6. New turn begins.

## Expected First Win

Default route:

1. Attack: wolf 12 -> 7, player 30 -> 26.
2. Attack: wolf 7 -> 2, player 26 -> 22.
3. Attack: wolf 2 -> 0, victory.

Expected duration: 3 player turns, under 45 seconds.

## Loss And Retreat

If player Health reaches 0:

- combat ends as `forced_retreat`;
- player returns to last safe camp with Health 5/30;
- `Supplies -1` if available;
- if no Supplies, player returns with Health 5/30 and receives `Exhausted` warning, but the first slice must not hard-lock.

Manual retreat:

- available after turn 2;
- returns to encounter view or camp if safe path is open;
- no combat loot.

## Camp Recovery Link

After the first fight, the player should be damaged enough to understand camp:

- expected Health after normal win: 22/30;
- camp rest costs Supplies -1;
- camp rest restores Health +12;
- crafted/healing upgrade improves future recovery, not the first fight requirement.

## Tuning Knobs

- wolf HP;
- player weapon damage;
- bite damage;
- defend reduction;
- skill success chance;
- forced retreat penalty;
- rest heal amount.

## Validation

- A reducer/test can simulate the expected 3-turn win.
- UI can show Health, enemy HP, action log, victory result, and retreat result.
- Camp rest visibly repairs the damage from combat.
