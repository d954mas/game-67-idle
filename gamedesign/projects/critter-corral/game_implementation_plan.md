# Critter Corral — Build / Run / Handoff

Status (2026-06-16): **gameplay implemented and release-shaped** (E004 / T0065).
Free placeholder sprites + procedural placeholder audio. Visual gate PASS.
Remaining for a true release: a human fun/balance playtest, and bespoke art via
Codex (the pipeline is ready for a drop-in swap). Engine: native Game Seed
harness; the whole game lives in `src/clean_seed_main.c` (+ `src/game_audio.*`).

## Build / run / capture

```bash
cmake --preset native-debug                                   # configure (once)
cmake --build build/_cmake/native-debug --target critter_corral_packs  # build the sprite atlas pack
cmake --build build/_cmake/native-debug --target game_seed             # build the game
build/game_seed/native-debug/game_seed.exe --devapi 9123 --window-size 960x540
```
- Automated screenshot/playtest: `tools/devapi/devapi_client.py` `running_game` +
  `game.capture.framebuffer` (glReadPixels -> PPM -> PNG). CAVEAT: capture +
  DevAPI need a FOREGROUND window — a background/unfocused window is GL-throttled
  by the OS and times out (not a game bug). Launch focused or use the reuse path.

## Controls

- Move the **mouse** = move the lure (herd nearby critters toward it).
- TITLE: click / any input to start.
- Between waves: click an upgrade card (or keys **1/2/3**) to pick 1 of 3.
- **R** = restart the run.

## Systems (what's built)

- Core moment: lure herds critters; a matching-color critter entering its pen is
  captured with squash + particle burst + pen flash + a same-color CHAIN (rising
  audio pitch + growing burst); wrong color bounces (gentle bonk).
- Run/phases: TITLE -> PLAYING -> WAVE_CLEARED -> UPGRADE_CHOICE -> next wave;
  soft WIN milestone at wave 10, then ENDLESS. Restart any time.
- Progression: waves ramp 2->5 colors, ~4->40 critters (hard-capped), gentle
  speed-up. A run reaches/clears the win milestone in ~10-20 min, then continues.
- Behaviors (the herding depth, progressive): normal / skittish (flees a crowding
  lure) / stubborn (slow to steer, bigger+dark outline) / follower (clings to
  neighbours); normal stays the majority, special shares capped (calm identity).
- Meta: pick-1-of-3 upgrades between waves (lure radius, lure pull, second lure,
  wider gates, calmer critters, longer chain; capped lvl 3, visible in HUD).
- Audio: 7 soft procedural SFX (pop/chain/bonk/chime/wave/win/start).
- HUD: fontless (wave, score, per-color remaining, acquired upgrades, restart).

## DevAPI (for automation / QA)

`game.state` (rich: phase, wave, color_count, critter_count, score,
loose_by_behavior, upgrades + effective{}, pending_choice, audio counters),
`game.start`, `game.reset_playtest`, `game.debug.skip_wave`,
`game.debug.pick_upgrade {index}`, `game.capture.framebuffer {output}`.

## Placeholders + CODEX ART HANDOFF

- Art = free PIL-generated placeholder sprites in
  `gamedesign/projects/critter-corral/art/sprites/*.png`, built into
  `assets/runtime/critter-corral/critter_corral.ntpack` by
  `tools/critter_corral/` (`generate_sprites.py` -> PNGs; `build_packs.c` ->
  atlas pack; CMake target `critter_corral_packs`).
- **Codex art pass:** replace the placeholder PNGs (same filenames/atlas region
  names: `critter`, `critter_a/b`, `pen`, `flag`, `grass`, `lure`, `spark`, the
  `icon_*`/`card`/`pip`) with bespoke art at the same sizes, rebuild the pack —
  zero game-code changes needed. Keep the visual DIRECTION in `concept.md`.
- Audio = procedural placeholder tones; a real SFX/music pass is a later step.

## Known gaps before a hard release

- Subjective FUN + balance over a real 10-20 min run needs a human playtest
  (the core moment, escalation pacing, and upgrade balance can't be fully judged
  from screenshots/automation).
- Bespoke art (Codex) and a polished audio pass.
- See `reviews/first_slice_visual_gate.md` (visual gate PASS) and T0065 log.
