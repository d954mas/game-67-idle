---
id: QTECH_003
name: Бюджет памяти (Memory Budget)
group: technical
description: Web memory limits beside the size budget, measured at startup, after a minute of play and through a bot soak.
---

# QTECH_003 Бюджет памяти (Memory Budget)

## Use When

A game with a web build ships, changes what it loads or keeps (packs, textures,
render targets, meshes, pools, caches), changes its link memory flags, or
reports crashes or WebGL context losses on weak phones.

## What It Checks

- `content/web_budget.json` carries a `memory` object beside the download
  `limits`; every limit names a census field and is a non-negative integer:
  `wasm_bytes`, `wasm_peak_bytes`, `wasm_grows`, `gpu_bytes`, `gpu_peak_bytes`,
  `js_heap_used_bytes`, `programs`. Reader and checker:
  `ai_studio/game_tools/web_memory_budget.mjs`.

  ```json
  "memory": { "viewport": "390x844@2", "limits": { "wasm_bytes": 100663296, "gpu_bytes": 83886080, "wasm_grows": 0 } }
  ```

- Limits come from measurement, not from a wish: the startup value on the
  budget viewport plus about 20% headroom, rounded up to 4 MiB. The target is a
  phone with 2 GB of RAM, where the WASM heap, the GPU resources (unified memory)
  and the browser share what Android leaves to one tab. Studio ceilings for that
  phone: `wasm_bytes` at most 128 MiB, `gpu_bytes` at most 96 MiB. A limit above
  a ceiling needs the lead's approval in the budget's `note`.
- The web release sets its initial memory from measurement, so a normal session
  does not grow the heap: every grow allocates a new, larger block and copies
  the heap, the allocation likeliest to fail on a phone. The budget sets
  `wasm_grows: 0` once the initial size is measured; until then it records the
  measured count and the note says why.
- Nothing keeps growing in a long session: after warm-up the WASM heap, its grow
  count, the GPU ledger, GL object and program counts are flat.

## How To Measure

The census is `ai_studio/game_tools/web_memory_census.mjs`: a page script that
ledgers live WebGL resources by kind and the WebAssembly memory with its grows,
without DevAPI, so a release build measures like a DevAPI build. GPU bytes are
what the page asked GL to allocate (drivers pad); they compare builds on one
machine and check a budget measured the same way, not an absolute device figure.

1. Startup: `node tools/game.mjs verify --target <portal>` measures the packaged
   release on the budget viewport once the game is ready and startup settles
   (heap, GPU and programs still for 4 s, at most 20 s), before any WebGL loss,
   and fails on overflow. Its last line prints `memory: wasm ... (N grows), gpu
   ...`. Without a game build to verify:
   `node ai_studio/game_tools/web_memory_budget.mjs --site <dir>|--zip <package.zip>`.
2. One minute of play: the same tool with `--play 60` keeps the page running;
   for real play use the game's own driver (a DevAPI census tool or the soak
   below for one minute).
3. Bot soak, 30-60 minutes:
   `node ai_studio/game_tools/web_memory_soak.mjs --site <dir> [--driver <module.mjs>]`.
   A game driver plays its transitions (room bots, windows, world changes);
   without one the seeded UI monkey taps, drags and presses keys. The run judges
   the post-warm-up peaks and writes `soak.json` and `soak.md`. A game runs it
   as a `slow`-tier test. Driver contract: `ai_studio/game_tools/README.md`.

## Evidence

- The budget diff with the measured startup values (WASM, grows, GPU) and the
  viewport, in the budget `note` or a linked evidence README.
- The `verify` line with `memory:` for the release target.
- For a release or a memory-relevant change: the soak table (duration, seed,
  verdict, per-series growth) and the action counts.
- Crash and context-loss reports carry memory at the event and every heap grow
  (`features/platform-sdk` runtime diagnostics); read them after a release.

## Do Not Use For

Games without a web build, download size (that is the `limits` object), or a
frame-time budget.

## Not Enough

A desktop census does not prove a phone survives: drivers pad allocations and
the browser's own share is not counted. A flat soak on one seed does not prove
other paths are flat; a soak without warm-up is not a leak verdict.
