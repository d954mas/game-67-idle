# Mining v0.01 Fake Shot Review - 2026-06-17

Image:

`visual/fake_shots/mining_v001_fake_shot_2026-06-17.png`

## Verdict

Status: `accepted as visual direction draft`, not final runtime target.

The shot communicates the intended pivot: blocky mining idle RPG, no cards, no
combat, one active Mining activity, progress bar, reward log, node list, pickaxe
upgrade, and a geode bonus.

## What Works

- Strong first-viewport signal: this is a mining idle game.
- Original blocky miner reads clearly and is not a direct Steve copy.
- Activity panel, reward log, node list, and upgrade panel explain the loop.
- The screen looks like a playable game screen rather than a poster.
- Text scale is directionally good for the important labels.

## Mismatches To Resolve In Runtime

- The fake shot shows higher resource counts than a true fresh v0.01 start.
- The node timer and upgrade speed values do not match `data/balance.json`; use
  the JSON as balance source until reviewed.
- Bottom navigation includes `Achievements` and `Shop`, which are likely later
  surfaces. v0.01 runtime can hide or disable them.
- The composition is visually rich; native first pass must still use real assets
  through the engine asset path, not debug shapes.
- Generated text must be replaced by real UI text in runtime.

## Next Proof

Before native implementation is called done:

- create a native screenshot of the Mining screen;
- run the UI readability zoom gate;
- compare against this fake shot by direction: mood, readability, composition,
  blocky mining fantasy, and game-loop clarity.
