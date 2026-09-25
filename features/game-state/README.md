# Game State Feature

Reusable schema-first game-state feature pack.

The generator and the byte-invariant persistence runtime are owned here and
compiled in place by templates and games. Consumers own only their schemas,
migrations, domain hooks, and composition.

## Purpose

Use this feature when a game needs typed, persistent, DevAPI-visible state that
agents can inspect and update safely.

The feature provides:

- schema-first `GameState` generation;
- stable persisted field paths and `reserved` tombstones;
- generated C storage/serialization and hand-written registry DevAPI dispatch;
- save/load envelope and migration guidance;
- review rules for state changes, fixtures, and runtime proof.

## Readable save format

Release saves are UTF-8 text. They may be edited while the game is closed:

```text
NTGS 1
format=1
save_version=2
saved_at=1788012345678
save_seq=42
playtime_ms=1250
app="sample-game"
build="0"

[settings 1]
master_volume=0.8
muted=false

[game 4]
coins=1250
tutorial_done=true
```

Blank lines and lines beginning with `#` are ignored. Unknown fields are
preserved by legacy import paths or ignored by tolerant fragment readers;
duplicate known fields and invalid typed values are rejected.

## Active playtime

The template and all current games count active playtime by default. Read it
with `game_save_playtime_ms()`; it persists with the save, starts at zero for
legacy saves, and resets on New Game. The game owns the activity gate through
`game_save_update_playtime(active)`; the shared counter reads monotonic time.
Pauses, ads, and browser lifecycle suspension close the active interval.
A custom shell calls the update once per frame and with false before its
native shutdown flush. Simulation speed never changes recorded playtime.

`game_save_validate_document_string(text, error, cap)` validates an encoded
JSON or NTGS document without publishing live state. It requires a registered
staged document validator. Text-only builds return unsupported because they
cannot stage-validate fragment input without mutation.

## Contents

```text
features/game-state/
  README.md
  INSTALL.md
  feature.json
  include/
    game_save.h
    game_save_seal.h
    game_save_text.h
    game_state_doc.h
    game_state_json.h
    game_storage.h
  src/
    game_save.c
    game_save_devapi.c
    game_save_platform_native.c
    game_save_platform_web.c
    game_save_text.c
    game_state_json.c
    game_storage.c
    game_storage_backend.h
    game_storage_backend_native.c
    game_storage_backend_web.c
    game_storage_web.c
  references/
    contract.md
    workflow.md
    review.md
  scripts/
    generate_state.py
    generate_state_test.py
    run_tests.py
    state_modules_test.py
    state_codegen/
  tests/
    items_containers.schema.json
  benchmarks/
    benchmark_codegen.py
    baseline.json
    fixtures/multi_fragment.schema.json
```

## Integration Model

There is no general feature installer. Consumers use this module in place:

1. Compile the reusable runtime from `features/game-state/src/` and expose
   `features/game-state/include/` without copying those files.
2. Keep consumer-owned `state/*.schema.json`, `state/migrations/`, fragment
   registration, domain hooks, and save configuration in the template/game.
3. Generate each fragment's `<id>_state*` files from its local schema into the
   build directory, or
   into a checked-in generated folder if that project explicitly chooses to
   version generated C.
4. Define a unique `GAME_STORAGE_APP_ID` in every consumer.

The default template consumes this module in place. New games inherit only its
CMake wiring and keep using the same root runtime.

For exact install, enable/disable, verification, and uninstall steps, read
`features/game-state/INSTALL.md`.

Default template integration uses:

- schema sources: `templates/template/state/{settings,items,progression,game_state}.schema.json`;
- generated output: `templates/template/build/<config>/generated/game-state/`;
- migrations: `templates/template/state/migrations/`;
- always compiled (the `FEATURE_GAME_STATE` on/off flag was removed
  2026-07-07 — a game without state is impossible);
- DevAPI registrations from the hand-written
  `features/game-state/src/game_save_devapi.c` registry
  dispatch (`game_save_register_devapi(on_change, user)`) only when
  `GAME_DEVAPI_ENABLED` is
  also on;
- semantic runtime commands and domain actions in the game or template source.

For a game-specific variant, pass explicit paths:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/generate_state.py --schema games/<game-id>/state/game_state.schema.json --out-dir games/<game-id>/src/generated
```

Runtime state cannot be disabled (no build flag). To remove DevAPI commands
from the build, configure `GAME_DEVAPI_ENABLED=OFF`.

## Save synchronization

`game_save_cloud.h` is the optional shared runtime for the registered save.
It owns cloud retries, the exact in-flight snapshot, durable adoption, and the
last acknowledged base. The template's `systems/sys_cloud_save` only binds
the platform transport. Compile the shared source in place; do not copy it
into each game.

The game supplies its progress policy and controls startup, tick, and the
safe point where a remote save may replace live state. Successful adoption
returns true so the game can rebind its own world and UI. A single
`game_save_choice_t` carries automatic and player decisions. Contradictory
progress can remain a conflict for game-owned UI. No generic layer interprets
levels, currency, playtime, or content.

`game_save_sync.h` remains the smaller, transport-independent building block
for consumers with their own save shell. Its documents are opaque; the caller
supplies transport, validation, persistence, and retry scheduling. Failed or
unknown reads prohibit uploads, and only an acknowledgment advances the base
to the exact sent snapshot. `game_save_sync_decide(sync, choose, user)` is the
cloud coordinator's automatic decision for such a shell: the base settles
"remote unchanged -> upload local" and "local unchanged -> adopt remote", and a
conflict goes to the game's `choose` callback, which sees both whole documents.
The reference policy is `choose_by_progress` in `tests/test_game_state_doc.c`:
migrate both documents first, and answer `GAME_SAVE_ASK` when either is
unreadable or newer than the build, so neither is overwritten automatically.

An adoption never destroys a local document with unsynced changes: the sync
keeps it (`game_save_sync_displaced_document`) and refuses the next displacing
adoption until the caller has persisted it and called
`game_save_sync_clear_displaced`. `game_save_sync_adoption_displaces` names the
copy before the adoption. The registered-save coordinator writes it to the
`cloud_sync_displaced` storage slot first and overwrites the local slot only
if that write succeeded; otherwise it stays in conflict and retries later.

## Instance documents

The registered save is one process-wide document. A process that holds many
profiles -- a room server, a solo client comparing its local and cloud copies --
uses instance documents instead, which never touch the `game_save` singleton:

- `generate_state.py --instance` generates a fragment with no process-wide
  state and no `GameSaveFragment`; it exports a `game_state_doc_fragment_t`
  descriptor over caller-owned states. Only scalar fragments qualify (the
  readable text codec), and hooks are refused, since they act on process-wide
  state. Migrations and `reserved` tombstones work as for any fragment.
- `game_state_doc.h` writes and reads a whole NTGS document over an array of
  states, with a `save_version`, a `save_id` (the profile's lineage, 16 hex
  digits) and a `rev` (its stored-write counter) in the header. A write
  validates every state first. A read requires the current `save_version` and
  fragment versions, treats a missing fragment as defaults, and refuses a
  fragment the schema lacks: only a document migration drops one.
- `game_state_doc_migrate` is pure: document steps, then each fragment's steps,
  in the order the `game_save` load path uses, on the same cJSON shapes. A
  current document, and every current fragment no document step changes, is
  copied byte for byte; a stepped fragment holding an integer of magnitude 2^53
  or more is refused rather than rounded.
- `game_save_seal.h` seals any text with ChaCha20 and HMAC-SHA256 under a
  32-byte game key, encrypt-then-MAC, with a nonce derived from the plaintext so
  one document always seals to one text. The key ships in the client: it stops
  casual reading and editing, not a determined player. The header documents the
  format a server needs to verify the tag. Nothing seals the registered save by
  accident; a caller seals explicitly. A text that fails to unseal is kept
  (quarantined) by the caller, never overwritten.

Sources: `src/game_state_doc.c` (with `game_save_text.c`, `game_state_json.c`,
cJSON) and `src/game_save_seal.c` (standalone). The template builds and runs
`tests/test_game_state_doc.c` over the fixture schemas in `tests/profile/`.

## Commands

Generate from the template schema:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/generate_state.py --schema templates/template/state/game_state.schema.json
```

Without `--out-dir`, the command writes to `build/generated/game-state` under
the template or game that owns the required `--schema` path.

Run generator tests:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/run_tests.py
```

The aggregate runner executes these focused suites:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/generate_state_test.py
node ai_studio/dev_environment/python_run.mjs -m unittest features/game-state/scripts/state_modules_test.py
node ai_studio/dev_environment/python_run.mjs -m unittest features/game-state/benchmarks/benchmark_codegen_test.py
```

Run the advisory local benchmark:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/benchmarks/benchmark_codegen.py
```

## Boundaries

- The schema is source of truth. Do not hand-edit generated `game_state.*`
  files.
- Runtime feature code always compiles. DevAPI registration is gated by
  `GAME_DEVAPI_ENABLED`; release builds must not compile or register those
  commands.
- `game_save.c` and `game_storage.c` are platform-neutral policy. Exactly one
  save/storage backend is selected by the consumer; Emscripten and DOM access
  stay in the web adapters.
- Generated state stores and serializes data. Gameplay rules belong in domain
  actions owned by the game or template.
- Schema v2 supports one deliberately narrow depth-two `list<Object>`
  aggregate. It generates separate fixed top-level and nested pools and a
  nested JSON projection; it is not a recursive object graph facility.
- Migrations transform old JSON before parsing into current runtime structs.
  They must not call domain actions.
- Raw `game.state.*` writes are for debug/editor overrides, fixtures, and
  targeted tests. Bots and gameplay checks should prefer semantic actions.
- Runtime proof collection belongs to `ai_studio/runtime_automation/`.
- Quality acceptance belongs to `ai_studio/quality/`.
- DevAPI dispatch is compiled from this module only when the consumer enables
  `GAME_DEVAPI_ENABLED`; release builds must keep it disabled.

## Feature-Pack Example Rules

Use this folder as the minimum bar for future feature packs:

- explain what the feature does and what it does not own;
- list dependencies and copy points;
- keep reusable scripts close to the feature;
- keep references specific to the feature;
- expose an agent-facing skill only as a thin router when discoverability helps.

## Public surface

Generated `GameState` files, fragment descriptors, and commands declared in
`feature.json` are public. Generator internals are not.

Version 4 makes New Game transitions explicit: `game_save_new_game()` and
`game_save_apply_pending_new_game()` return `{ state_changed, persisted }`, so
callers always rebind live state when persistence is temporarily unavailable.

Version 3 changes the DevAPI change callback to
`(change, fragment_id, user)`. `fragment_id` is set for `EDIT` and is `NULL`
for full-state `REPLACE`, allowing the game session to reconcile only the
affected domain.

## Validation

Run the `test` command from `feature.json`, then
`node features/validate_contracts.mjs`.

## Compatibility

`feature.json.version` is exact SemVer. Patch preserves the public contract,
minor adds backward-compatible surface, and major permits breaking changes.
Consumers pin both this version and an exact repository revision.

Version `4.2.0` zeroes the alignment gap a repeated section leaves before its record
array. The gap ships inside the payload, so skipping it let two identical emits copy
different bytes into the log. Generated sources change; regenerate. The schema
validator also claims the names the emit body uses as locals and the descriptor
tables it emits, so a field or event named like one of them is rejected instead of
producing C that does not compile.

Version `4.3.0` adds a nullable fragment snapshot writer. Generated fragments
can serialize their payload into a caller-owned bounded buffer without cJSON;
legacy JSON serialization remains the compatibility path for load, import,
export, migrations, and DevAPI.

Version `4.3.1` preserves the JavaScript storage imports when Emscripten links
the runtime with full LTO. The public API is unchanged.

Version `4.4.0` adds allocation-free readers and writers for the readable
`NTGS 1` save format. Scalar-only generated fragments expose text callbacks;
JSON remains available for legacy import, migrations, and DevAPI.

Version `4.5.0` adds an opt-in `GAME_SAVE_TEXT_ONLY` release shell. It reads and
writes NTGS directly, rejects legacy JSON, omits document/fragment migrations,
and lets release LTO discard the JSON persistence path. Changing
`GAME_STORAGE_APP_ID` at the same time intentionally starts every player with a
fresh namespace. Enable it only when every reachable save is current NTGS or a
save reset is approved.

The hot lane is opt-in. Before `game_save_init`, a consumer supplies one static
buffer with `game_save_set_hot_snapshot_buffer(buffer, sizeof buffer)` and an
allocation-free whole-state validator with `game_save_set_live_validator()`.
Every registered fragment must provide `write_text`; scalar generated fragments
do so automatically. The buffer must remain valid for the game runtime. Storage
writes are synchronous, so it is never published while the writer is still
filling it. Without those hooks, or when transforms/orphans are present, the
save shell uses its legacy JSON path rather than claiming an allocation-free
tick.

Version `4.8.0` adds instance fragments (`generate_state.py --instance`),
instance documents with a pure migration (`game_state_doc.h`), the save seal
(`game_save_seal.h`), and `game_save_sync_decide`. Generated singleton
fragments and every existing API are unchanged. One behaviour is new: when a
cloud adoption replaces a local save that held unsynced changes, the
registered-save coordinator keeps that save in the `cloud_sync_displaced` slot.

## Extension points

Extend through game-owned schemas, migrations, hooks, and DevAPI adapters;
game-specific state policy stays outside the generator.
