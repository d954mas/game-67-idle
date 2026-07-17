# Features

Reusable feature modules live here as `features/<feature-id>/`; this README
also indexes template-owned feature implementations that deliberately have no
second library copy.

A feature is a game capability, not just one source file. It can include code,
assets, state schema, migrations, UI, DevAPI hooks, tests, examples, and notes.
Its owner determines installation: reusable core modules compile in place;
template-owned UI features copy with the template and are then game-owned;
game-specific composition stays in the game.

There is no plugin manager, install command, dependency solver, or automatic
enable/disable system here yet. Keep feature packs small enough that a human or
agent can inspect and copy them safely.

## Install And Flags

Follow the feature's `INSTALL.md`: an in-place module is compiled from this
folder, while a feature-pointer is copied from its template owner. Generated
files should be reproducible from the owning source, either in the build
directory or as explicitly checked-in generated outputs when a project chooses
that policy.

Every feature must include an install manual. Use `INSTALL.md` for the concrete
copy/build/enable/verify/uninstall steps. Keep high-level purpose and ownership
in `README.md`; keep exact integration commands in `INSTALL.md`.

Feature runtime flags use `FEATURE_<FEATURE_ID_UPPER_SNAKE>`, for example
`FEATURE_GAME_STATE`. Dev-only integrations stay behind their own global guard,
for example generated state DevAPI code compiles only under
`FEATURE_GAME_STATE && GAME_DEVAPI_ENABLED`.

## Suggested Shape

```text
features/<feature-id>/
  README.md        what it does, how to copy it, dependencies, origin
  INSTALL.md       exact install, enable/disable, verify, uninstall steps
  feature.json     optional metadata when the feature needs it
  src/             code to copy into the game or template
  assets/          source assets or packed asset inputs
  state/           schemas, migrations, or seed state
  tests/           focused validation or smoke tests
  example/         tiny runnable example when useful
```

Only add the folders a feature actually needs.

## Categories: module vs feature-pointer vs game code

Three categories of code live in or are pointed at from this folder:

- **In-place module** (`features/<id>-core/`, `features/game-state/`) — one
  copy of the source lives here; every consuming template/game compiles it
  IN-PLACE against its own generated headers and content (relative path
  `../../features/<id>-core/`, depth-2 invariant: both `templates/<x>` and
  `games/<id>` sit exactly two levels below the repo root). Shared; edits are
  under test discipline (every consumer's tests must stay green).
- **Feature-pointer** (`templates/template/src/features/<id>/`, single copy,
  copy-then-own) — the template IS the single source of truth; a new game
  gets it via a full template copy, and any further move is copy-then-own
  from that point on.
- **Game code** (in the template or a specific `games/<id>`) — content/
  config, seed data, closed verb/reason lists, save-history migrations,
  composition. Never promoted to a shared copy; it encodes one game's
  specifics.

**Decisive rule (primary test = byte-invariance of the `.c`, not consumer
count):** would this piece of `.c` be byte-IDENTICAL in a second game, with
that game customizing it ONLY through data/config? YES -> **in-place
module**. How many consumers exist RIGHT NOW is not the deciding factor —
`game-state`, `items-core`, and `progression-core` each have exactly ONE
consumer today (the template) and are still modules, because their `.c` is
proven invariant. "One consumer" alone is never grounds for keeping
something a feature-pointer.

**Feature-pointer** applies when the `.c` is NOT yet proven invariant, OR
there is high "repaint" pressure — UI/taste code every game is expected to
edit directly, not just configure (`settings`, `resource_panel`).

**Game code** applies when the piece encodes THIS game specifically — its
seed, its verb vocabulary, its save history, its composition.

### Backdoor: forking a module

A game with genuinely different semantics (e.g. a different ownership model
than `items-core` assumes) is not expected to get a speculative switch added
to the shared module for it. Instead it copies the module's `src/`+`include/`
out into its own tree (copy-then-own, same escape hatch `settings`/
`resource_panel` already use) and owns that fork going forward. No code in
the shared module exists to support this — it stays a documented
possibility, not a feature.

## feature.json fields

`feature.json` (schema `ai_studio.feature.v1`) описывает переиспользуемую фичу.
Идентичность фичи = строковый `id` (= имя папки); НИКАКИХ числовых id-диапазонов
(нет поля `state_id_range` — идентичность стейта = имя JSON-ключа фрагмента).

Поля:

- `schema` — всегда `ai_studio.feature.v1`.
- `id` — строковый id (= имя папки, `[a-z_][a-z0-9_]*`), C-префикс символов.
- `title`, `summary`, `status`, `kind` — человекочитаемая метаинформация.
- `layer` — `L0` | `L1` | `L2` (слой; include-и строго вниз).
- `provides` — список публичных API-имён/возможностей, которые фича даёт
  фичам выше (напр. геттеры/операции её публичного хедера).
- `registers` — список точек шелла, куда фича добавляет ОДНУ строку
  (фазы `game_features.c`, сейв-фрагмент, DevAPI-команды), чтобы установка
  сводилась к append.
- `assets_tag` — значение тега `feature=<id>` в `assets.jsonl` игры (как
  ассеты фичи помечаются в общем `assets/`-дереве).
- `art_needs` — список деклараций арта `{slot, kind, hint}`: `slot` — роль
  (icon/panel/…), `kind` — тип (sprite/atlas-region/…), `hint` — подсказка
  подбора/генерации. Фича берёт арт ТОЛЬКО как хендлы в конфиге, с graceful-
  фолбэком; `build_packs.c` остаётся кодом игры (фича в паки не пишет).
- `dependencies`, `flags`, `commands`, `manuals`, `outputs`, `default_template`
  — как прежде (движок/тулинг/инсталл/генвыходы/дефолтная привязка к шаблону).

## Current Packs

- `game-state/`: schema-first generated GameState, save/load contract,
  migrations, and DevAPI state adapters. It is consumed in place by
  templates/games.
- `items-core/` (`L1`): typed compact item catalog + ownership
  model (`items_add`/`items_remove`/`items_move`/`items_count`/
  `items_can_afford`/purse/unique instances) + isolated Lua evaluation,
  normalized Snapshot, compact runtime package, and focused semantic CLI. In-place module, same
  shape as `game-state/` (`../../features/items-core/` from any
  `templates/<x>` or `games/<id>`). The consuming template/game owns its
  Lua manifest/modules (`items.lua.json` + `design/items/*.lua`), release
  history (`content/items.lock.json`), its state schema
  (`state/items.schema.json`), and a small game-owned "items corner"
  (`templates/template/src/features/items/`: `reason_tags.h` + the
  `items_bootstrap.c`'s `items_on_new_game` seed — see that folder's own
  `README.md`). Reference: `items-core/README.md` + `items-core/INSTALL.md`.
- `progression-core/` (`L2`, depends on `items-core`): level/xp tracks
  (manual/auto/threshold modes), T5 tick caps, lazy allocation, plus curve
  codegen (`generate_progression_tracks.py`). In-place module, consumed the
  same way via `../../features/progression-core/`. Unlike items, progression
  has no game-owned C corner. The consuming template/game owns its content
  (`content/progression.json`), its state schema
  (`state/progression.schema.json`), and its own composition (e.g. the
  template's `src/ui/demo_hud.c` idle-income binding). Reference:
  `progression-core/README.md` + `progression-core/INSTALL.md`.
- `game-events/` (`L0`): reusable in-place event/analytics spine. Owns
  `game_events`, event descriptor/rendering contracts, DevAPI tail
  (`game.events.tail`), and the local `game_analytics` NDJSON writer. Higher
  features emit through this pack; analytics subscribes to it. Reference:
  `game-events/README.md` + `game-events/INSTALL.md`.
- `audio-core/` (`L1`, `1.0.0`): versioned fixed-pool playback contract with
  generation-safe clip/voice handles, native miniaudio and browser WebAudio
  backends. Games own cue/music catalogs, source assets, codec-neutral BLOB IDs,
  settings and lifecycle composition. Its contract version records the public
  API; compatibility still requires the owning validation commands. Reference:
  `audio-core/README.md` + `audio-core/INSTALL.md`.

## Features (reference implementations live in the template)

`settings` and `resource_panel` do NOT have a library copy under `features/`.
Their single source of truth is the live implementation in
`templates/template/src/features/<id>/`, which ships with tests and a real
consumer (the template itself). Entries are pointers, not copies:

- **settings** (`L2`) — settings screen + persisted settings fragment.
  Reference: `templates/template/src/features/settings/` (`settings.c/.h`,
  `settings_screen.c`) + `templates/template/state/settings.schema.json`.
- **resource_panel** (`L2`, UI widget) — generic counter/bar HUD driven by
  game-supplied entries/getters; zero items/progression coupling. Reference:
  `templates/template/src/features/resource_panel/` (`resource_panel.c/.h`,
  `README.md`, `feature.json`).

The game-owned portions of items and progression remain in the template:

- **items** — game corner `templates/template/src/features/items/`
  (`reason_tags.h` + `items_bootstrap.c`'s `items_on_new_game` seed, plus its
  own `README.md`) + catalog `templates/template/items.lua.json` and
  `design/items/*.lua`, `content/items.lock.json` + state
  `templates/template/state/items.schema.json` + the template's own
  package-backed integration tests `templates/template/tests/test_items_fragment.c`
  and `test_template_composition.c` (+ focused Items fixtures).
- **progression** — no game-side C corner at all (see
  `progression-core/README.md` "No game-owned C hooks") — just content
  `templates/template/content/progression.json` + state
  `templates/template/state/progression.schema.json` + the demo composition
  `templates/template/src/ui/demo_hud.c` + the template's own content-coupled
  integration tests `templates/template/tests/test_progression.c`,
  `test_progression_catalog.c`, `test_progression_curve.c`.
- `platform-sdk/` (`L1`): in-place platform SDK facade for commercial web
  builds. Targets are `local`, `itch`, `poki`, `yandex`, and `playgama`; SDK
  adapters are `mock`, `poki`, `yandex`, and `playgama`. `local` and `itch`
  both use `mock` because `mock` is an SDK adapter, not a target, and itch has
  no mandatory runtime game SDK. Web builds copy only the selected adapter into
  `platform-sdk-adapter.js`; the pack also owns target manifest inspection and
  local NDJSON scorecards.

### Ownership model

- Single source of truth for `settings`/`resource_panel` = `templates/template`
  (live reference implementation with tests and a real consumer). There is
  deliberately no library copy of these two features, so there is no duplicate
  implementation to keep in sync.
- A brand-new game gets all of them via a full template copy
  (`games/new_game.mjs`).
- Moving `settings`/`resource_panel` into an existing game, or into a second
  template, is copy-then-own straight from the template pointer above — that
  copy then belongs to its new project (see Rules).
- Promote a feature-pointer into `features/<id>/` or `features/<id>-core/`
  only once its `.c` is proven byte-identical across a real second
  consumer/divergence test.

## Rules

- A feature must be self-contained enough to copy without guessing hidden files.
- `README.md` is required for each feature folder.
- `INSTALL.md` is required for each feature folder, even when the install is
  only "already installed by this template".
- List dependencies explicitly: engine APIs, template systems, other features,
  assets, state keys, build changes, and runtime hooks.
- Do not reach into a specific game's globals. Use the game/template's public
  world, state, and system boundaries.
- After copying a feature into `games/<game-id>/` or `templates/<template-id>/`,
  that project owns its copy and may edit it freely.
- Promote useful local improvements back here only after they are generalized.

## Later

A future architecture pass can add installer scripts, dependency checks, preview
examples, or richer enable/disable tooling. Until then, install manuals are the
contract that keeps feature packs copyable.
