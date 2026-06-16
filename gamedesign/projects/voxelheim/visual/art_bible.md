# Voxelheim Art Bible — "Bright Roblox Adventure"

Anchor: `fake_shot_first_screen.png` (Theme A, locked). The fake shot is
direction (mood / palette / composition / readability), not a pixel target.

## Style

Bright, saturated, friendly **blocky low-poly "Roblox toy"** look. Bold dark
outlines, soft cel shading (one shadow tone + one highlight), clean plastic
materials, chunky rounded forms. Readable at a glance. Gameplay framing:
**3/4 top-down** for casual readability.

## Palette

| Role | Hex |
|------|-----|
| Sky / portal / magic | `#3FB7FF` `#1E7BE0` |
| Snow | `#F4FAFF` `#CDE6FF` |
| Stone | `#8A8579` `#6B5E4E` |
| Hero | blue tunic `#2E6BD6`, warm skin, brown accents |
| Ice enemy | `#7FE0D0` |
| Gold / reward | `#FFC83D` |
| HP red | `#FF4D4D` · Stamina yellow `#FFD84D` · Go green `#46C84B` |
| Outline | near-black brown `#26201C` |

## Silhouettes (must read instantly)

- **Hero:** chunky, big head, clear sword; saturated blue reads as "you".
- **Enemy:** distinct icy silhouette = threat, still friendly-cartoon.
- **Frost Keep:** clear archway + glowing blue portal = the goal beacon.

## Production

- Generate source sheets via **agy** (`delegated-image-generation` skill), each
  on a flat **`#ff00ff` magenta chroma** background, isolated elements with big
  gaps, no baked text/letters.
- Cut + audit via `generated-game-ui-assets` (intake → crop → runtime PNGs →
  pixel/atlas audits) → build the runtime atlas `.ntpack` via `nt_builder`.
- Judge the **assembled screen** against this bible / the fake shot, not just
  clean crops.

## Forbidden

Baked text/letters in art, realism, muddy / low-contrast, grimdark, debug
primitives as game visuals.

## Free assets

Allowed for tileable ground textures, SFX, and fonts **if** agy can't deliver
(CC0 / permissive only — record the license + source). Prefer agy for hero,
enemy, keep, and UI so the whole screen stays one consistent style.

## First-batch inventory (T0002, generating)

hero · Frost Keep + portal · ice goblin enemy · environment kit (snow tile,
path tile, pine, rock, signpost) · UI kit (HP bar, stamina bar, level badge,
minimap, item slot, quest banner, primary button). FX (hit, sparkle, level-up)
in the next batch.
