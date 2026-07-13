---
type: Game Concept
title: Runway Awakening Concept
description: Bright Poki fashion-alchemy MVP where a styled outfit awakens a collectible magical form.
tags: [concept, poki, dress-up, fashion-alchemy]
game_id: web-dressup
status: accepted
date: 2026-07-11
---

# Runway Awakening

## One-liner

A bright 2D browser dress-up game where the player creates a beautiful look,
steps onto a magical runway, and discovers which unique awakening is produced
by the outfit's two focus pieces.

## Player fantasy

> I created this beautiful outfit. Now I want to see which magical star it
> awakens me into.

## Product promise

`Dress -> Awaken on runway -> Win -> Discover recipe -> Restyle`

- The outfit remains visible through the awakening.
- The player always wins; there is no hidden beauty score or fail state.
- A main outfit provides the primary Essence.
- A magic accent provides the secondary Essence.
- Every pure or paired recipe has a distinct silhouette and hero moment.
- Exact item properties are discovered after first use, while their visual
  language hints at the result beforehand.

## Poki MVP locks

| Lock | Decision |
|---|---|
| Target | Poki browser MVP and 500-player Player Fit test |
| Session content | 8-12 minutes without timers or forced waiting |
| Presentation | Bright, juicy 2D magical editorial anime |
| Layout | Portrait and landscape first-class |
| Character | One locked fixed-pose doll with limited face/hair/skin personalization |
| Content | 3 Essences, 6 awakenings, 30 wearables, 8 meaningful rounds |
| Outcome | Always-win runway spectacle and collectible result card |
| Animation | Static art plus transforms, particles, light, camera and sound |
| Multiplayer/social | None in MVP |
| Initial payload | Hard internal gate <=6,500,000 bytes; must stay below Poki requirement |

## MVP Essences

- `moon`: cobalt, silver, crescents, orbits, smooth arcs.
- `bloom`: hot pink, coral, petals, vines, butterfly geometry.
- `flame`: tangerine, crimson, gold, rays, flame tongues, sharp forms.

The six MVP recipes are Moon/Moon, Bloom/Bloom, Flame/Flame, Moon/Bloom,
Moon/Flame, and Bloom/Flame.

## Design pillars

1. **Beautiful before magic** - styling must be satisfying without the reveal.
2. **First change immediately** - target first equip within 3 seconds of interaction; hard acceptance <=5 seconds.
3. **Readable causality** - the player can connect two focus pieces to the result.
4. **Surprise, not randomness** - exact awakening is a discovery; resolution is deterministic.
5. **Every pair is a scene** - no result may be only a recolor or renamed particle preset.
6. **Second-look compulsion** - the first reveal immediately creates a reason to restyle.
7. **Web discipline** - compressed assets, responsive layout, no text-wall onboarding.

## No-go list

- Theme picker, theme compliance score, random stars or fake judging math.
- Full character animation, generated video per outfit, 3D or multiplayer.
- Dialogue-heavy story, detective mode, currencies, shop, gacha or energy.
- Licensed magical-girl names, signature costumes, music or transformation sequences.
- Legacy isolated garment placement or unreviewed AI output in the shipping pack.
- More than three Essences before the vertical slice passes external testing.

## Concept success gate

The concept survives only if unfamiliar players can reach the first awakening
without explanation, understand that clothing caused it, distinguish the six
results, and voluntarily start another outfit.
