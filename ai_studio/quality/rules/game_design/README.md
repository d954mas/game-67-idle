# Game Design Rules

Use this group when changed work affects core loop, player motivation, economy,
progression, feature fit, rewards, challenge, playable-slice design, or design
data that changes what the player does and why it matters.

## Not For

- GDD/design-source clarity by itself: use [GDD](../gdd/README.md);
- player-facing clarity by itself: use
  [Player Clarity](../player_clarity/README.md);
- art direction or asset readiness: use [Art](../art/README.md) or
  [Assets](../assets/README.md);
- runtime/build behavior: use [Technical](../technical/README.md).

## Checks

### [QDES_001 - Playable Loop](checks/QDES_001_playable_loop.md)

Checks: gameplay model has player action, response, visible progress/reward,
repeat reason, next hook, and no dead/confusing loop state.

Use when: gameplay/progression/economy/reward/challenge docs, data, prototype,
or runtime work changes the player's action loop.

Record applied checks in the task log using the outcome format from the Quality
README.
