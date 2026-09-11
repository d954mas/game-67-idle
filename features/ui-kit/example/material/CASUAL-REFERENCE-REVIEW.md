# Game UI reference review

The user accepted Material as a direction for the system, but rejected the current browser example as insufficiently game-like. The new primary visual reference is the user-provided LAYERLAB GUI Pro - Minimal Game Blue. The existing Material browser preview remains an interaction/theme experiment, not the accepted game-facing style.

## Sources actually viewed

### Official store

https://layerlab.io/products/gui-pro-minimal-game-blue-psd

Viewed the public cover and gallery slides 4, 5, 6 in Chromium, including component sheets, counters, item slots, perk cards, rewards, rarity borders and purchase rows. The page describes common white/sliced sprites, base prefabs, themed prefabs and reference scenes. It explicitly says its demo layouts do not include code or animation. Its public demo is https://layerlab.itch.io/gui-pro-minimal-game-blue-demo.

The useful neutral part is the vocabulary of game controls and their hierarchy: bold compact type, large meaningful icons, edged surfaces, clear price/action areas, badges, and content-specific slots. Blue is a replaceable theme. Character art, equipment and currency icons are example content, not mandatory starter features.

### Local Yandex Disk collection

Root: C:/Users/ROG/YandexDisk/gamedev/assets/buy/ui/

Viewed original preview files through Chromium. No Unity archives were extracted and no vendor source assets were copied into the shared feature.

| Pack | Viewed material | Finding |
| --- | --- | --- |
| Layer Lab/GUI PRO Kit - Simple Casual | Preview/004_Component_1.png, 100_Settings.png, 070_Play_Type_01.png, 082_Popup_Reward_Get.png | White surfaces, shallow button depth, colorful icon sockets, overlaid bars and floating unit cards. A softer, more neutral reference. Some palette combinations have weak text contrast; do not adopt those values blindly. |
| Layer Lab/GUI Pro-SuperCasual | Preview/14_Settings.png, 4_Play_UI_Idle.png, 4_Play_UI_ChoiceSkill.png | Strong dark contour, bold outlined labels, compact resource strips, ability slots, badges and skill cards over a dimmed game. Strongest local reference for recognizable casual game UI. |
| Layer Lab/GUI Pro-CasualGame | Preview/Popup_Setting.png | Dark blue panels, icon-first rows, shallow button lips and bold labels. Useful surface hierarchy; exact theme need not become the studio default. |
| Casual and Fancy GUI Pack | Preview/preview01.png | Crowns, ribbons and ornamental frames add a strong genre motif. Too themed as the shared default. |

Additional folders inventoried: Casual Game GUI, Casual GUI Pack, Casual Ultimate GUI Pack, Cartoony Simulator UI Pack, GUI Pro Simple Casual and GUI Pro SuperCasual. Those inventory entries are not claims of pixel-level review of every asset.

The shared catalog searches earlier in this conversation did not discover these pack-level resources. Add pack names and preview entry points to the catalog as separate future maintenance; do not make every loose sprite an eagerly loaded template asset.

## Existing game lessons

A studio game's runtime screenshot was viewed: scene dominates, level/ammo are compact central counters, icon-plus-label utilities sit at the upper right, no full-page application shell. The design/visual/implementation-b8.md contract preserves that hierarchy.

A studio game's design/references/ui_kit/world-desktop.jpg was viewed as a backdrop/reference frame. Its design/prototype.md and source review establish a boss strip, wallet, corner upgrade action, upgrade cards with effect/level/price, and a result panel over the same world. HTML design references and source review are not a substitute for current runtime screenshots.

## Revised kit scope

Prioritize these game-facing components before expanding generic form controls:

1. Resource counter: icon socket, number, optional add action or change feedback.
2. Status strip: optional portrait, name, health/progress and phase marker.
3. Ability button: large icon, short label, cooldown, amount, lock and selected state.
4. Upgrade/perk card: icon, effect, current-to-next level, price, affordable/locked/max state.
5. Reward item and result composition: reward contents, amount and primary/secondary actions.
6. Game overlay layout: edge HUD, free world center, safe areas, modal scrim and adaptive phone/desktop arrangement.

Neutral defaults should carry readable bold typography, controlled shallow depth, a modest contour and clear icon hierarchy. World art, lore, economy, inventory content and the meaning of progress/cooldown are owned by the game. The system must not impose all store screens or mechanics on new games.

Material remains useful for semantic tokens, control states, spacing consistency and keyboard/accessibility behavior. The commercial casual packs and the actual Toy/Robot composition are the visual and game-flow reference.

## Reuse boundary

The local folder name buy is not license evidence. A ReadMe adjacent to one standalone Simple Casual archive describes third-party learning/test use; it is not a commercial license from the author. A separate Casual GUI Pack license allows game use but disallows redistributing raw art. Review direction now; establish a valid license before incorporating vendor art into a distributable starter or game. The linked LAYERLAB product also states restrictions on generative-AI use. No vendor imagery was passed to image generation during this review.

Capture evidence: tmp/ui-kit-material-qa/ref-*.jpg, layerlab-sample-4.jpg through layerlab-sample-6.jpg, layerlab-samples.json. These are reference-view screenshots, not newly authored shipping assets.
