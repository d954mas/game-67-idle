---
name: nt-game-feature
description: "Use when creating a new reusable feature or in-place module under features/, or when deciding whether a piece of game code belongs in a root in-place module (features/<id>-core/), a template feature-pointer (src/features/<id>/), or plain game code -- module-vs-feature-pointer-vs-game-code triage, module scaffolding (include/src/scripts + README/INSTALL/feature.json + in-place CMake wiring), and byte-identity relocation gates."
---

# NT Game Feature

Router for creating or improving a reusable feature/module in this repo.
Canon lives in `features/README.md` (root decisive rule + Current Packs),
`templates/template/src/features/README.md` (in-template feature
convention), and each module/feature's own `README.md`/`INSTALL.md`/
`feature.json` — do not duplicate their content here.

## Start

1. Read `features/README.md` "Categories" — the decisive rule for module vs
   feature-pointer vs game code, `Current Packs` (existing in-place modules:
   `game-state`, `items-core`, `progression-core`), and the pointer-feature
   section (`settings`, `resource_panel`).
2. Read `templates/template/src/features/README.md` — the in-template
   feature convention (one `<id>.h` public header, `<id>_*` symbol prefix,
   layers L0/L1/L2, `game_features.c` phase wiring, state/asset rules).
3. Precedent to imitate for a NEW module: `features/game-state/` (toolkit)
   and `features/items-core/` + `features/progression-core/` (T0337
   relocation, `templates/design/build_spec_t0337_2026-07-07.md`) — read one
   module's `README.md` + `INSTALL.md` before scaffolding a new one.

## Create or improve a feature or module

1. **Triage the category first** — apply the decisive rule
   (`features/README.md` "Categories"): would this `.c` be byte-IDENTICAL in
   a second game, customized only by data/config? Consumer count right now
   does NOT decide this — `game-state`/`items-core`/`progression-core` all
   started with exactly one consumer and are still modules.
   - YES, and it is genuinely invariant -> **in-place module**.
   - NOT yet proven invariant, OR high "repaint" pressure (UI/taste code a
     game is expected to hand-edit, not just configure) -> **feature-pointer**
     in the template.
   - Encodes THIS game specifically (seed, verbs, save history, composition)
     -> **game code**, never promoted.

2. **Scaffolding a module** (`features/<id>-core/`):
   - Layout: `include/features/<id>/<id>.h` (public header; keep the
     spelling if relocating existing code — see "Rules" below), `src/*.c`,
     `scripts/*.py` (codegen/CLI — take paths ONLY from the caller, CWD/args,
     never `__file__` inside the module), `README.md` (WHAT: public API,
     ownership/behavior model, tools, layer, backdoor), `INSTALL.md` (HOW:
     CMake wiring, required game-owned files, ctest wiring, verify,
     uninstall), `feature.json` (mirror `features/game-state/feature.json`;
     `layer`, `dependencies`, `registers` describing the CONSUMER's existing
     wiring, not install steps — the module is already in-place).
   - CMake: the consuming template/game defines
     `<ID>_CORE_DIR = "${CMAKE_CURRENT_SOURCE_DIR}/../../features/<id>-core"`
     (+ `_INC`/`_SRC`/`_SCRIPTS`) near `ENGINE_DIR`. `../../` resolves to the
     repo root from BOTH `templates/template` and any `games/<id>` (both sit
     exactly two levels below root) — this is what lets a `games/new_game.mjs`
     copy build against the module with ZERO wiring changes; verify it with a
     throwaway `new_game.mjs` probe before calling a module extraction done
     (G-newgame in `templates/design/build_spec_t0337_2026-07-07.md` §10).
   - Include path: `<ID>_CORE_INC` goes AHEAD of the consumer's own `src` on
     `target_include_directories`, so a stray same-named copy in the
     consumer's tree can never shadow the module.
   - Layer edges: include only DOWN (an `L2` module may include an `L1`
     module's public header; never the reverse) — declare the edge in
     `feature.json.dependencies` and grep-gate both directions (no mention of
     the dependent inside the dependency's source).

3. **Relocating existing feature code INTO a new module** (the T0337 shape):
   treat it as a pure relocation, not a refactor — moved `.c`/`.h`/`.py`
   content stays byte-identical (only the physical path and include/CMake
   wiring change). Gate it: `git diff -M --diff-filter=R --summary` (or
   `git mv`) shows a pure rename for every moved file; a snapshot-diff of
   generated output before/after the move is empty; the consumer's full test
   suite stays green throughout — one commit per increment, never a
   mid-state where files moved but the build still points at the old path
   (kills `git bisect`). Leave anything not yet proven invariant behind as
   game code (e.g. content-coupled integration tests that assert a demo
   catalog's values stay in the template, not the module).

4. **New feature-pointer** (`src/features/<id>/`, in the template):
   - One folder, one public header `<id>.h` (first line `// feature-layer:
     L1|L2`), everything else `static`, symbols prefixed `<id>_`.
   - Wire it with ONE added line per phase in `game_features.c` — no hook
     tables, no self-registration.
   - Add a pointer-entry (not a copy) to `features/README.md`'s "Features
     (reference implementations live in the template)" section.

## Rules

- **Include spelling survives a relocation.** Moving a header into a module
  does not mean renaming its include string in every consumer — keep the
  logical `features/<id>/<id>.h` spelling by mirroring that path under the
  module's own `include/features/<id>/` subtree and adding the module's
  `include/` to the consumer's include path (see `items-core/README.md`
  "Include spelling"). A spelling rename touches every consumer's include
  lines and breaks byte-identical relocation for no benefit.
- **Layer strictly downward.** L2 may depend on L1 (module or feature); L1
  never depends on L2; lateral L2-to-L2 includes are not allowed — push the
  dependent feature down a layer instead of adding a side edge.
- **Zero generalization without a second consumer, right now.** No new
  parameter, switch, hook registry, or config knob added to a module or
  feature "for later" — LEAN forbids it until a second real consumer
  actually needs it. A game with different needs copies the module/feature
  out and owns the fork (the documented "backdoor", not a built switch).
- **Per-game customization stays in per-game files.** Migrations, seed data,
  and closed verb/reason lists are always game code, even when the code that
  calls them lives in a module — see `items-core/README.md` "Reason verbs" /
  "Migrations" for the concrete split.

## Boundary

Keep this skill as a router. Canonical facts — public API, ownership model,
CMake wiring, layer edges, backdoor — live in each module's own
`README.md`/`INSTALL.md`/`feature.json` or each feature-pointer's own
`README.md`, plus the root `features/README.md` decisive rule and
`templates/template/src/features/README.md` convention. For an existing
feature's day-to-day workflow (content authoring, CLI commands, save
migrations), use its own skill instead, e.g. `nt-game-items` for `items`.
