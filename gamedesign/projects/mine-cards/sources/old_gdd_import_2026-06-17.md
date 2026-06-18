# Old GDD Import - 2026-06-17

## Imported Sources

- Old GDD:
  https://docs.google.com/document/d/1FsBYGJq1GqKB3WvEpS-tgRnRXwHv38WZAuhwngkHf14/edit?usp=sharing
- Combat notes:
  https://docs.google.com/document/d/1eU41-qgxjZ_-p6FSksIz4jhqjb7HJinpfpqBTs_o_4s/edit?usp=sharing
- Figma:
  https://www.figma.com/file/pg1jmXq724TLG1Ln7earlA/Idle-Craft-2?node-id=74%3A671
- Old web prototype:
  https://544727.selcdn.ru/mine-cards/index.html
- Local art folder:
  `C:\Users\ROG\YandexDisk\gamedev\assets\my\Mine Cards`
- Temporary contact sheet made for this import:
  `tmp/mine_cards_contact_sheet.png`

## Extracted Facts

- Format: portrait mobile layout, target composition `1080x1920`; desktop uses
  a wide background with the portrait interface centered.
- Visual direction: voxel/blocky Minecraft-adjacent style, not literal pixel
  art; own casual art style with Minecraft-like readability.
- Old screen set: location selection, location/card gameplay, equipment
  upgrades, skill upgrades, chest open, victory, defeat, exit location, level up,
  not enough food.
- Old bottom nav/icon needs: location select, exit location, skills, equipment,
  sound toggle, music toggle.
- Old card types: monster, trap, resources, event.
- Old combat direction: dungeon exploration is represented as a descending card
  grid/deck; player chooses one of three cards to step onto, then moves downward.
- Old tactical rule: monsters show attack directions; after the player's move,
  monsters check adjacent attack directions and attack the hero.
- Old failed attack rule: if the hero steps onto a monster but does not kill it,
  the hero returns to the previous cell.
- Old scroll rule: when the hero reaches the third row, the board scrolls upward
  by one row and the top row is discarded.
- Old boss rule: the boss can shuffle the deck, move the player to the top, and
  spawn enemies and items.
- Progression: level-ups add HP; skills and equipment improve combat/economy.
- Equipment intent:
  - armor: much more HP;
  - helmet: small HP and bonus to shields;
  - boots: small HP and dodge chance;
  - gloves: small HP and chance for extra reward;
  - pickaxe: more currency;
  - sword: damage.
- Removed old design points: no energy system; no HP bars needed on equipment;
  location list should use vertical scroll instead of left/right arrows.

## Reference Backlog

These are names from the old docs. They are not yet a valid implementation
reference base until each driving reference has a durable deconstruction.

- `Forward`
- `Quest Cards`
- `Crossroads`
- `Dead Shell`
- `Minimal Dungeon RPG`
- `Munchkin`
- `Exploding Kittens`
- Minecraft
- Minecraft Dungeons
- Minemob Clicker menu/title treatment
- https://dribbble.com/twotinydice

## Import Limits

- The linked Figma and old prototype were captured as source links, not deeply
  audited in this import.
- The PSD art folder contains several files Pillow could not open directly.
  That is an asset pipeline task, not a concept blocker.
- The old docs contain mechanics but not final balance numbers. All numeric
  values in current JSON contracts are first-slice draft values.

