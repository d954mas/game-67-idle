# Build spec — T0327 tail: storefront doc-sync + 4-fragment integration ctest (2026-07-08)

Closes the two `[M]` tails from the T0327 arc review (card
`ai_studio/taskboard/items/active/T0327-...md`, Log «вечер»): (1) the storefront
docs don't know about the feature model; (2) no integration ctest lifts all 4
state fragments together. Arc code is complete; this is the doc + test tail only.

## §1 Goal / scope

IN:
1. **Storefront doc-sync** — make `templates/template/README.md` and
   `templates/TEMPLATE.md` factually match the shipped feature-based template.
   Minimal, honest edits; **link** the live convention docs, do **not** duplicate
   their doctrine.
2. **One integration ctest** — a new `test_template_composition` binary that lifts
   all 4 real fragments (settings/items/progression/game) through the real
   `game_save` registry and drives a cross-fragment lifecycle. ctest **16 → 17**
   in both presets.

NOT in scope (adjacent, named so the executor doesn't chase them):
- **Generator banners** — fixed by a PARALLEL pass; do not touch here.
- **`CONVENTIONS.md` and `src/features/README.md`** — already current on the
  feature model (verified). No edits — these are the docs the storefront should
  *point at*, not re-explain.
- **`TEMPLATE.md` mojibake** (`вЂ”`/`в†’` — real UTF-8 corruption in the file, not a
  display artifact). Out of doc-sync scope; a separate re-encode pass is the
  lead's call (see §6).
- Repo-root `README.md` (out per task). The legacy rb-dark `game`-fragment fields
  `[M]` (already resolved by the hygiene package `d1a4ad0cc`). The "keep" dead
  demo fields (separate lead decision). DevAPI/web/visual acceptance.

## §2 Storefront edit map (file → section → edit)

Ground truth used for every row: real `src/` tree from the Glob sweep, `CMakeLists.txt`
(4 fragments registered `settings→items→progression→game`, `main.c:398-401`),
`content/` (items.json/progression.json/item_fields.schema.json/items.lock.json),
in-place modules `features/{game-state,items-core,progression-core}`.

### templates/template/README.md

| Section | What is stale / wrong | Replace with (minimal) |
|---|---|---|
| Layout block, **L6-24** | Describes the tree as PROSE, not the shipped file set: marks `render/ systems/ scene/ devapi/` `[wip]`; names systems only generically ("input, camera, character move" — no such files, only `systems/sys_move.c`); lists a `scene/` dir and a src-level `devapi/` dir that do NOT exist; **omits `features/` and `content/` entirely**. (No phantom file *tokens* here — they are prose — so the §4 grep already returns 0 for README; this row is completeness + the false `[wip]`/dir claims, NOT token removal.) | Rewrite to the EXPLICIT, COMPLETE shipped tree — the executor does not decide inclusion on the fly. LIST (present; keep/add): `src/main.c` (conductor); `src/features/` = `game_features.c` (7-phase aggregator, list=z-order) + `settings/ items/ resource_panel/`; `src/systems/sys_move.c`; `src/render/{render_mesh,capture}.c`; `src/ui/{hud,ui_runtime,theme,demo_hud}.c`; `src/world/world.h`; the L0 shell TUs `src/game_{save,storage,state_json,events,event_render,log,analytics,format}.c` (+ `game_{save,events}_devapi.c` under `GAME_DEVAPI_ENABLED`); `src/build_packs.c` (L17 keep); `src/game_audio.*` (L18 keep — seed infra; note it is NOT yet compiled into the game target); top-level `devapi/` Python bots (L19 keep); `assets/shaders/` (L20 keep); `state/` = the 4 fragment schemas (L21, expand); `design/` scaffold (L22-23 keep); ADD `content/` (items.json / progression.json / item_fields.schema.json / items.lock.json). DELIBERATELY DROP (say so in one clause, don't silently omit): the src-level `devapi/ [wip]` line and `scene/ [wip]` — neither exists; game-owned DevAPI commands ship as `src/game_*_devapi.c` under `GAME_DEVAPI_ENABLED`, not a `src/devapi/` dir, and a `scene/` builder is future work. Add ONE pointer line: *"Feature-based architecture: see `CONVENTIONS.md` + `src/features/README.md`; reusable-feature ownership model: `features/README.md`."* Do not restate layers/categories. |
| Status, **L25-28** | "starter shell (empty scene + text)… the full shell … **in progress** — see epic E009." Still paints a settings-only, empty-scene shell. | Update to reality: runnable shell — gear/settings panel (sliders + long-press reset), items (L1) + progression (L2) systems, and a **live resource-panel HUD on launch (gold counter + xp bar via `resource_panel`/`demo_hud`)** over two sample cubes; feature-based architecture; native + 3 web presets green; test base 16 (→17 with this spec). E009 arc is code-complete. |
| Feature flags / state, **L40-48** | Accurate re: `FEATURE_GAME_STATE` axis removed. | Keep. Add one line: persistent state = **4 fragments** (`settings/items/progression/game`) over the `features/game-state` registry, each registered by one line in `main.c`. |
| DevAPI block, **L30-38** | Accurate (7 `game.state.*` commands). | No change. |

### templates/TEMPLATE.md

| Section | What is stale / wrong | Replace with (minimal) |
|---|---|---|
| "Concrete `src/` layout", **L143-192** | Phantom file TOKENS (these ARE what the §4 grep-gate targets): `world/world_state.{c,h}`, `scene/scene_setup.{c,h}`, `systems/{sys_input,sys_character_move,sys_camera}`, `render/{render_setup,render_world,render_character}`. Only `features/game_features` + `settings/` in this block are real. | Replace with the shipped tree (same list as README row 1). Every phantom token MUST be deleted or generalized — the §4 grep-gate requires ZERO hits, and this is the SINGLE path (do NOT leave the block carrying phantom tokens under an "illustrative" label). An "illustrative decomposition" framing is allowed ONLY with role-name prose ("a movement system", "a render system") and NO concrete non-existent filenames. Add the shipped **learning features** (items L1, progression L2, resource_panel L2 widget), `content/`, and the **in-place modules** `features/{items-core,progression-core,game-state}`. |
| "decomposition BY EXAMPLE", **L116-142** | Sample character via `sys_character_move` / `sys_character_render` — phantom tokens (grep-gate targets). | Retarget to the REAL example (`systems/sys_move.c` + `render/render_mesh.c`), OR keep generic role prose with NO phantom filename tokens; either way the anti-god-file message stays and the phantom tokens must be gone (grep-gate). |
| "Feature library", **L54-75** | Generic "copy a feature" model; predates the module / feature-pointer / game-code doctrine. | Add ONE pointer line to `features/README.md` §"Categories: module vs feature-pointer vs game code". Do not restate the decisive rule. |
| **"Reuse tiers", L21-52** | FALSE after T0337: L40-42 "(No separate frozen 'core' … copy-then-own, not link.)" and L49-52 "the engine as the stable linked core absorbs the cross-cutting fixes that clone-and-own otherwise can't propagate" — the in-place modules `features/{items-core,progression-core,game-state}` ARE a shared linked core (ONE copy, compiled in-place by every consumer, edited under shared test discipline, NOT copy-then-own). | Correct to the three categories in one clause — **in-place module** (shared, linked, one copy: items-core / progression-core / game-state) / **feature-pointer** (copy-then-own: settings, resource_panel) / **game code** — and point to `features/README.md` §"Categories". Do NOT restate the byte-invariance decisive rule. Fix the "engine is the ONLY linked core" framing: linked-shared tier = engine **+** in-place modules; copy-then-own tier = feature-pointers + game code. |
| **"Startup UX / Template Seed", L82-114 (esp L105)** | "opens to an **empty scene** with a settings (gear) button … Settings are NOT shown on launch" — still a settings-only shell; after И3b the launch view carries a live resource-panel HUD (gold counter + xp bar via `resource_panel`/`demo_hud`) over the sample cubes. | One clause: launch shows the gear panel **and** the live resource-panel HUD (gold counter + xp bar); keep the gear + long-press-reset description. |

Guiding cut: the storefront answers *"what is this and how do I use it"* and points to
the three current convention docs; the doctrine (layers, categories, byte-invariance
rule) stays single-sourced in `features/README.md` / `src/features/README.md`.

## §3 Integration test design

**File:** `templates/template/tests/test_template_composition.c` (game-side test of
THIS template's composition — which 4 fragments, their order, the `50 gold + 1 potion`
seed — is game-specific, not module-invariant; per T0337 Q1, content-specific tests
= the consumer's tests, so it lives in the template `tests/`, not in any module).

**Why these invariants (and no others):** the existing 16 tests are all
per-fragment / per-module (`test_items_fragment`, `test_progression*`,
`test_game_save` uses a FAKE fragment). None assembles the 4 REAL fragments through
the real registry. The four cases below each cover a cross-fragment invariant no
existing test touches, and nothing else.

1. **`test_registry_has_four_fragments_in_order`** — `game_save_fragment_count()==4`
   and `game_save_fragment_at(i)->id` == `settings, items, progression, game`.
   *Value:* locks the exact registration contract `main.c` depends on
   (order matters for reconcile/on_new_game fan-out and the L1-before-L2 rule).
2. **`test_new_game_seeds_across_all_fragments`** — `game_save_new_game()` then
   assert the composed seed: items `items_count("purse","tmpl.gold")==50` and
   `items_count("backpack","tmpl.potion")==1`; progression `progression_level("hero")==0`
   (empty tracks = lazy); settings master ≈ `0.8` (default). *Value:* proves the
   orchestrator's `on_new_game` fan-out composes the documented starting state —
   the single thing the review said is untested.
3. **`test_cross_fragment_save_load_roundtrip`** — mutate across fragments, snapshot,
   scramble, restore, assert. *Value:* proves ONE envelope carries cross-fragment
   state through `build_root → load_from_doc` (incl. `reconcile_all`), the composed
   save→load nobody exercises. Uses the **items→progression edge** so the L2→L1
   dependency is proven end-to-end through the composed registry.
4. **`test_hold_to_reset_preserves_settings`** — the live T0327 hygiene mechanic:
   `game_save_request_new_game("settings")` + `game_save_apply_pending_new_game()`
   resets gameplay but skips settings. *Value:* proves "Hold to reset progress"
   wipes items+progression back to seed while **volumes survive**, on the REAL
   4-fragment registry (the review's flagged live mechanic).

**Scenario skeleton (steps):**

```
// registration ONCE in main() (no unregister API; registering per-setUp would
// duplicate/overflow), in the documented order:
main():
  game_events_init();
  game_save_register_fragment(&settings_state_fragment);
  game_save_register_fragment(&items_state_fragment);
  game_save_register_fragment(&progression_state_fragment);
  game_save_register_fragment(&game_state_fragment);   // `game` last
  UNITY_BEGIN(); RUN_TEST(...x4); r=UNITY_END(); game_events_shutdown();

setUp():   remove build/saves/test_composition.{json,bak,tmp};  // deterministic FRESH
           game_event_frame_reset();
           game_save__set_clocks_for_test(fixed_mono, fixed_wall);  // GAME_SAVE_TESTING
           game_save_set_transforms(NULL,0);
           game_save_init();
tearDown():remove the same slot files.

// (1) count==4; ids in order settings/items/progression/game.

// (2) game_save_new_game(err); assert 50 gold / 1 potion / hero level 0 / master≈0.8.

// (3) game_save_new_game(err);                        // baseline
     items_add("purse","tmpl.gold",25,"test:rt");      // gold -> 75
     int64_t need = progression_xp_needed("hero");     // read from API (curve-agnostic)
     items_add("purse","tmpl.xp",need,"test:rt");
     progression_update();                             // auto-mode consumes xp -> level 1
     TEST_ASSERT_EQUAL_INT(1, progression_level("hero"));
     settings_set_master(0.30f);
     char *snap = game_save_export_string(err,cap);    // in-memory envelope (no disk)
     settings_state_fragment.reset(); items_state_fragment.reset();
     progression_state_fragment.reset(); game_state_fragment.reset();  // scramble
     // assert scrambled (gold 0 / level 0 / master 0.8) so restore is load-bearing
     game_save_import_string(snap,err,cap); free(snap);
     assert gold==75 && hero level==1 && master==0.30f.

// (4) game_save_new_game(err); settings_set_master(0.30f);
     items_add("purse","tmpl.gold",999,"test");
     progression_set_level("hero",3,"test:prologue");  // deterministic, no xp economics
     game_save_request_new_game("settings");
     TEST_ASSERT_TRUE(game_save_apply_pending_new_game());
     assert master STILL 0.30f          // skipped fragment survived (the crown invariant)
         && items gold==50 && potion==1 // reset + on_new_game re-seeded
         && progression_level("hero")==0; // reset, no hook -> empty tracks
```

Determinism notes: read `progression_xp_needed("hero")` from the API instead of a
literal (survives content-curve changes); step 3 uses `export/import` (no disk) for
the roundtrip; the disk path is still exercised by `new_game`/`apply` (real seed +
hold-to-reset paths). Hermetic: no window/render/DevAPI — links `nt_hash/nt_log/nt_core`
only, label-agnostic (runs identically native-debug and devapi-debug). Includes system
headers before `unity.h` (MSVC `noreturn` quirk, per `test_game_save.c`).

**CMake registration** — append immediately AFTER the `test_game_format` block
(after `add_test(NAME test_game_format ...)`, currently `CMakeLists.txt:789`) and
BEFORE the closing `endif()` at `CMakeLists.txt:790`:

```cmake
    # T0327 tail: 4-fragment composition test -- lifts settings/items/progression/game
    # through the REAL game_save registry (envelope + on_new_game fan-out + skip-reset).
    # GAME_SAVE_TESTING injects clocks & avoids nt_time (precedent test_game_save).
    add_executable(test_template_composition
        tests/test_template_composition.c
        src/game_save.c src/game_storage.c src/game_state_json.c src/game_events.c
        "${GAME_STATE_GENERATED_SOURCE}" "${GAME_STATE_GENERATED_EVENTS_SOURCE}"
        "${SETTINGS_STATE_GENERATED_SOURCE}" src/features/settings/settings.c
        "${ITEMS_STATE_GENERATED_SOURCE}" "${ITEMS_STATE_GENERATED_EVENTS_SOURCE}"
        src/features/items/items_bootstrap.c
        "${ITEMS_CORE_SRC}/items_reconcile.c" "${ITEMS_CORE_SRC}/items_containers.c"
        "${ITEMS_CORE_SRC}/items_catalog.c" "${ITEMS_CATALOG_GENERATED_SOURCE}"
        "${PROGRESSION_STATE_GENERATED_SOURCE}" "${PROGRESSION_STATE_GENERATED_EVENTS_SOURCE}"
        "${PROGRESSION_CORE_SRC}/progression.c"
        "${GAME_SOURCE_GENERATED_DIR}/progression_tracks.gen.c")   # REAL hero curve (cf. test_progression_curve)
    add_dependencies(test_template_composition progression_tracks_gen)  # progression.c #includes .gen.h
    target_link_libraries(test_template_composition PRIVATE cjson unity nt_hash nt_log nt_core)
    target_include_directories(test_template_composition PRIVATE
        "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" src
        "${GAME_STATE_GENERATED_DIR}" "${GAME_SOURCE_GENERATED_DIR}")
    target_compile_definitions(test_template_composition PRIVATE
        GAME_SAVE_TESTING=1 GAME_STORAGE_APP_ID="template_test"
        GAME_SAVE_AUTOSAVE_SLOT="test_composition"
        GAME_SAVE_DEBOUNCE_MS=2000 GAME_SAVE_MAX_INTERVAL_MS=30000 GAME_SAVE_DOC_VERSION=1
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_template_composition PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_template_composition COMMAND test_template_composition
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
```

Distinct slot `test_composition` (not `test_slot`) so it can't collide with
`test_game_save` under parallel ctest. Generated `.gen.c` are custom_command OUTPUTs
→ auto-built; the `progression_tracks_gen` phony dep (already defined, `CMakeLists.txt:738`)
guarantees `progression_tracks.gen.h` exists before `progression.c` compiles.

**Executor note — compile surface (review №1):** this is the FIRST non-`game` target to
compile `settings.c`, which transitively pulls `ui/nt_ui.h` + `world/world.h` via
`settings.h`. Per review №1 those resolve through `nt_module`'s PUBLIC include dirs
(engine usage-requirements + the target's own `src`) and pull no extra LINK symbols — so
no added link libs are expected. Still, smoke-check that `settings.c` actually COMPILES in
this target's minimal TU set on BOTH presets before calling the CMake block done; if a
header is unreachable, add the owning engine module's include interface (e.g. link `nt_ui`
for its headers), never stub the header.

**What it does NOT check, and why:** debounce/MAX_INTERVAL timing, corrupt/backup/NEWER
state machine, orphan retention, transforms, and cross-fragment failure-isolation (a bad
fragment not dropping its neighbours, at the envelope level) → owned by `test_game_save`. Container/
quarantine/purse mechanics → `test_items_fragment`. Curve math / T5 caps →
`test_progression*`. DevAPI dispatch → devapi-only + `smoke_bot`. Render/count-up
animation → headless-unverifiable (lead's eye). `game`-fragment demo-field semantics
→ known dead surface. This test owns ONLY the cross-fragment composition seam.

## §4 Gates

- **ctest 17/17 in BOTH presets:** `cmake --build build/native-debug` and
  `build/devapi-debug`, then `ctest --test-dir build/<preset> --output-on-failure`
  → 17 pass each (was 16). New test green in both.
- **pytest not regressed:** `py -3.12 -m pytest` over the state/items generator
  suites still green (baseline 44/44 per card Log — no source touched here, so this
  is a no-regression check).
- **Storefront honesty grep-gate (advisory, run by executor + reviewer; not wired
  into ctest — gates are advisory, lead is backstop):**
  - **No phantom files** (must return EMPTY):
    `grep -REn 'sys_input|sys_camera|sys_character|scene_setup|render_setup|render_world|render_character|world_state\.' templates/template/README.md templates/TEMPLATE.md`
  - **Points at live convention** (must be NON-empty in each doc):
    `grep -l 'features/README.md' templates/template/README.md templates/TEMPLATE.md`
    and each doc mentions `content/` and `src/features/`.
- **`native-release` configure** (advisory-optional, NOT required): a cheap arch-untouched
  sanity check. The two-preset ctest gate above already catches CMake/link errors, and a
  docs + single-test-target change does not justify a `wasm-release` build (cut).

## §5 Slicing (commits)

Two commits, each independently shippable:

1. **Storefront doc-sync** — the §2 edits to `README.md` + `TEMPLATE.md` only.
   Gate: both grep-gates green; docs read true. (Docs-only, no build impact.)
2. **Integration ctest** — `tests/test_template_composition.c` + the §3 CMake block.
   Gate: ctest 17/17 both presets, pytest no-regress, native/web link.

Order independent; do 2 first if the executor wants the harder gate settled early.
Keep them separate so a doc wording nit never blocks the test landing (or vice versa).

## §6 Risks / LEAN cuts

- **Generated-header ordering (progression):** `progression.c` includes
  `progression_tracks.gen.h`. Mitigated by linking the real `.gen.c` (OUTPUT edge)
  **and** `add_dependencies(... progression_tracks_gen)` — belt-and-suspenders, one
  line, mirrors `test_progression`'s proven guard. If a clean parallel build ever
  races, that dep is the fix.
- **Native storage path:** `game_storage` writes `build/saves/test_composition.*`
  relative to CWD (`WORKING_DIRECTORY = build/tests`). setUp/tearDown must remove the
  slot so seed assertions start FRESH. Precedent `test_game_save` proves this works on
  both presets; keep the cleanup minimal (primary/.bak/.tmp only — no corrupt sweep).
- **Auto-level coupling:** step 3 depends on `hero` being an auto-mode track that
  consumes purse xp. Reading `progression_xp_needed()` from the API (not a literal)
  keeps it robust to curve edits; if `content/progression.json` ever drops the `hero`
  track, this test updates with it (acceptable — it IS the composition test).
- **Case-1 contract coupling (deliberate):** case 1 locks `count==4` and the exact
  fragment order/ids — an INTENTIONAL composition contract, not brittleness. A new
  template fragment updates this assertion together with its own registration (symmetric
  to the hero-track coupling above); that update IS the test doing its job.
- **LEAN cuts (deliberately NOT added):** no debounce/timing assertions, no
  corrupt/NEWER paths, no DevAPI round-trip, no per-fragment re-tests — all already
  owned elsewhere (§3 "does NOT check"). Four cases, one binary. Doc edits are
  factual fixes + pointers, **not** a rewrite of the convention docs.
- **TEMPLATE.md mojibake:** flagged, left to a separate re-encode pass (lead's call).
  It does not block the grep-gates (phantom-file terms are ASCII). If the executor is
  already rewriting the affected blocks, re-encoding just those lines is a free win —
  but do not expand the diff to the whole file under this spec.
