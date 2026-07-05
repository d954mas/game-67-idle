# Ash Border World Map 01

- Asset: `games/rb-dark-rpg/assets/ui/generated/world_map_ash_border_01/ash_border_map.png`
- Game: `rb-dark-rpg`
- Role: first playable world-map atlas for the Ash Border / Last Post route.
- Status: accepted generated procedural map-art pass.
- Origin: deterministic project-local procedural generation.
- Generator: `games/rb-dark-rpg/tools/generate_world_map_art.py`.
- Date: 2026-07-05.
- Size: 1280x720.
- SHA256: `19C3287E0C146BE98D2CE9A16ACA436A7BD186C40581F6DBAE61BE9CE99AA010`
- License: project-internal generated asset.

## Source-First Check

The required map needs exact runtime alignment with `locations.json` coordinates
and the first route content: Last Post, gate outskirts, Old Mill, road links,
river crossing, region border, and future fogged space. No existing shared or
third-party source asset can be used without either losing this alignment or
creating derivative-map risk.

This pass is generated from project data and authored procedural shapes instead.
It borrows only structural lessons from the user-provided Legend map references:
terrain-first map reading, irregular highlighted region boundaries, roads,
visible landmarks, and marker-friendly POI placement. It does not copy map
imagery, geography, icon art, names, or UI ornament.

## Regeneration

```text
py -3.12 games/rb-dark-rpg/tools/generate_world_map_art.py --out games/rb-dark-rpg/assets/ui/generated/world_map_ash_border_01/ash_border_map.png
```

The runtime marker overlay uses the same location coordinate transform as the
generator, so gameplay hitboxes remain aligned with the map art.
