# items-core — collapse item stack model to a single int (build spec)

Task: stack-int migration. Author: deep-reasoner, 2026-07-08. Status: reviewed (2 independent
Opus reviews, both ACCEPT-WITH-FIXES; fixes applied), ready for the executor.
Ratified by the lead 2026-07-08: collapse `stack: {stackable, max_stack?, unlimited?}` to a SINGLE
required integer with `0` = unlimited stack, `1` = not stackable (unique/instance path), `N>1` = stack cap.
Rationale: invalid combinations become unrepresentable; matches the `currency.cap: 0 = uncapped`
convention in the same schema; three fields → one ("subtract not add").

New module doc dir: `features/items-core/docs/` did NOT exist before this spec (the module had no
`docs/` convention). This file creates it; precedent for per-module docs is `ai_studio/assets/canvas/docs/`
and `ai_studio/assets/items_viewer/docs/`.

---

## 1. Goal / non-goals

Goal: replace the 3-field `stack` object with one authoring int across the schema, the template data,
the content codegen, and the op-layer validator — without changing any runtime behavior.

Non-goals (do NOT build/scaffold):
- No runtime behavior change. `items_add`/`items_remove`/`items_move`/`items_instance_create` etc. are
  untouched; they never branched on `stack` (see §4).
- No items_viewer JS/HTML/CSS change — BUT `list --json` DOES change (see §7.2). The viewer renders every
  `schema.core` key generically via `renderTypedValue` (`site/items.js:165-168`; `CHROME_KEYS` at `:143`
  excludes only id/display_name/icon_asset_id/kind/blocks), so `stack` IS drawn on every card. `list --json`
  must therefore emit the raw `stack` int, which the viewer renders as a clean i64 scalar (`0`/`1`/`99`);
  emitting the OLD derived object under schema v2 (`stack:i64`) would fall through `renderTypedValue` to
  `String(value)` (`items.js:108`) → `[object Object]` on every card. The viewer code itself is untouched.
  Rendering polish (badges, labels) remains a separate ratified round.
- No T0316 write op-layer (`upsert`/`deprecate`) — still read-only.
- rb-dark-rpg is OUT OF SCOPE. It is a closed/shipped game with its OWN catalog struct
  (`games/rb-dark-rpg/src/game_content.h`: `bool stackable; int max_stack;`) and its OWN generator
  (`tools/generate_dialogue_content.py`); it does not consume items-core's `game_item_def_t`. Do not touch it.
- No save-fragment / migration change. `state/items.schema.json`, `items.lock.json`, and owned-record
  shape are unaffected (stack config lives in the compiled catalog, never in a save).

---

## 2. Design decisions (decided here, with justification)

**D1 — JSON field shape: top-level `"stack": N` (int), reusing the existing key.**
Chosen over a nested one-field object and over renaming to `max_stack`. Reasons: (a) flat (lead's lean);
(b) reuses the SAME key `stack` that exists today, so the migration is "object → int" on one key —
minimal churn, tooling that keys on presence of `stack` still finds it; (c) `stack: 0` reads as
"unlimited" cleanly, paralleling `currency.cap: 0` / `capacity: 0` in the same file, and avoids the
`max_stack: 0` / `max_stack: 1` cognitive dissonance ("a max of zero/one") that the name `max_stack`
would carry in hand-edited data. The compiled C field keeps the descriptive name `max_stack` (§D3); the
codegen bridges JSON `stack` → C `.max_stack`. Rejected: nested `stack:{max_stack}` (earns no keep);
JSON key `max_stack` (worse readability at the 0/1 edges).

**D2 — Bump `schema_version` 1 → 2 in `item_fields.schema.json`.**
The `core.stack` shape changes incompatibly (object → scalar). No code currently GATES on this field's
`schema_version` (only `items_ops.py cmd_schema` prints it; no test asserts `== 1`; the `schema_version != 2`
checks in `items_ops.py:588-590` are for the LOCK file, a different document). The bump is therefore safe
and is exactly the signal `schema_version` exists for — a future consumer / the T0316 editor keys off it.
This is advisory (nothing enforces it), cheap, and consistent with the lock file's own v2 discipline.
Rejected (one line): leave at 1 — loses the incompatible-shape signal for no gain.

**D3 — C representation: KEEP the three derived fields in `game_item_def_t` unchanged; change only the
codegen derivation.** This is the crux, and it follows from a verified fact (§4): the runtime consumes
NONE of `.stackable`/`.max_stack`/`.unlimited`. So "preserve runtime semantics exactly" holds trivially
regardless of C shape. Given that, keep the compiled triple because:
1. Smallest, ZERO-edit diff (the decisive reason): keeping the triple means `items.h`, all runtime `.c`,
   `test_items_catalog.c`, and `test_template_composition.c` stay unchanged — the derived values are
   identical for every item except sword's unobserved `max_stack` 0→1 (§3). The doctrine's target
   (unrepresentable invalid combos in hand-authored JSON) is fully achieved at the AUTHORING layer; the
   compiled struct is consistent-by-construction and was never part of the problem.
2. Ergonomic pre-decoded view for FUTURE template games: the compiled triple spares a game reading the
   catalog from re-deriving `stackable`/cap from the int at each use. There are ZERO consumers of
   `game_item_def_t.{stackable,max_stack,unlimited}` today (verified §4: runtime `.c` all clean; only the
   `items.h:42-44` declaration + `test_items_catalog.c:21,22,30` asserts). (rb-dark-rpg is NOT such a
   consumer — it reads its OWN `game_item_definition_t`, `game_content.h:53-54`, from its own generator.)
   So this convenience is speculative-but-cheap, not load-bearing — #1 is the real justification.
Rejected (one line): collapse the struct to a single `int64_t max_stack` + rewrite the 3 test asserts —
cleaner end-to-end but edits the public header and tests for zero present benefit.

**D4 — Equip cross-field rule: tighten to `equip ⇒ stack == 1` (rule id renamed `equip-unlimited` →
`equip-stack`).** Under the collapsed model an `equip` block IS a unique instance (per-copy,
instance_id-keyed, count=1), i.e. `stack == 1` exactly. Any other value (`0` unlimited, `N>1` capped)
contradicts per-copy ownership (an equippable can't share one stacked record and still carry per-copy
level/durability). This directly encodes the ratified semantic "instance path ≡ max_stack == 1" and makes
"equippable stackable" unrepresentable — the spirit of the change. No existing item is affected (sword is
`stack:1`). Rejected (conservative, orchestrator may pick instead): keep the exact old strictness as
`equip ⇒ stack != 0` (only forbid unlimited-equippable), rule id `equip-stack`, one-line message change.

---

## 3. Semantic mapping (old derived → new int) — this is the correctness contract

The compiled triple is derived from the single int `s` (validated `int >= 0`):

| `stack` (s) | `stackable` | `max_stack` | `unlimited` | meaning |
|---|---|---|---|---|
| `0`  | `true`  | `0`   | `true`  | unlimited stackable |
| `1`  | `false` | `1`   | `false` | not stackable → unique / instance path |
| `N>1`| `true`  | `N`   | `false` | capped stack |

Derivation rule: `stackable = (s != 1)`, `unlimited = (s == 0)`, `max_stack = s`.

Verify against every current item (old `stack_fields` output → new):
- gold/xp/energy: were `stackable=true, max_stack=0, unlimited=true` → `s=0` reproduces identically.
- potion: was `stackable=true, max_stack=99, unlimited=false` → `s=99` identical.
- wood: was `stackable=true, max_stack=999, unlimited=false` → `s=999` identical.
- sword: was `stackable=false, max_stack=0, unlimited=false` → `s=1` gives `stackable=false, unlimited=false`
  (identical) and `max_stack=1` (was `0`). **This is the ONLY changed compiled value in the whole change.**
  It is unobserved (no runtime read; no test asserts sword->max_stack) and is arguably more correct
  (a non-stackable item's max stack is 1, not 0).

---

## 4. Every C consumption site of the stack fields (verified) → the rewrite

Grep of `*.c`/`*.h` for `stackable|max_stack|unlimited` across the repo (excluding engine/rb-dark/build):

- `features/items-core/src/items_containers.c` — reads NONE of the three. `add_raw` clamps on
  `def->currency->cap` (`:133`), not `unlimited`; the unique path is the separate public API
  `items_instance_create` (`:290`), chosen by the caller, not routed by `stackable`. **No change.**
- `features/items-core/src/items_catalog.c` — lookups only. **No change.**
- `features/items-core/src/items_reconcile.c` — quarantine/seq only. **No change.**
- `features/items-core/include/features/items/items.h:42-44` — the struct fields stay. **No change**
  (optional: add a one-line comment `/* derived from content items.json "stack" int: max_stack 0=unlimited, 1=unique, N=cap */`).
- `templates/template/tests/test_items_catalog.c:21,22,30` — asserts `gold->stackable`(T), `gold->unlimited`(T),
  `sword->stackable`(F). All stay true under the new derivation → **stays green, no edit required.** (New
  assertions are ADDED, see §7.)
- `templates/template/tests/test_template_composition.c` — includes `items.h`, seeds gold/potion via
  `items_add`; does not read the three fields. **No change.**

Conclusion: no runtime or existing-test C rewrite is needed. This is the basis for D3.

---

## 5. Exact schema diff — `templates/template/content/item_fields.schema.json`

Line 3, bump version:
```
-  "schema_version": 1,
+  "schema_version": 2,
```
Lines 13-21, replace the `stack` object with a scalar:
```
-    "stack": {
-      "type": "object",
-      "required": true,
-      "fields": {
-        "stackable":  { "type": "bool", "required": true },
-        "max_stack":  { "type": "i64", "required": false },
-        "unlimited":  { "type": "bool", "required": false }
-      }
-    }
+    "stack": { "type": "i64", "required": true, "note": "0 = unlimited stack, 1 = unique/instance (non-stackable), N>1 = stack cap" }
```
(`"note"` is inert documentation — `cmd_schema` reads only `type`/`required`; `schema --json` echoes it
verbatim so the T0316 editor can surface it.)

---

## 6. Exact data migration

### 6.1 `templates/template/content/items.json` — all 6 items (replace the `stack` line only)
```
tmpl.gold   :  "stack": { "stackable": true, "unlimited": true },      →  "stack": 0,
tmpl.xp     :  "stack": { "stackable": true, "unlimited": true },      →  "stack": 0,
tmpl.energy :  "stack": { "stackable": true, "unlimited": true },      →  "stack": 0,
tmpl.potion :  "stack": { "stackable": true, "max_stack": 99 },        →  "stack": 99,
tmpl.sword  :  "stack": { "stackable": false },                        →  "stack": 1,
tmpl.wood   :  "stack": { "stackable": true, "max_stack": 999 },       →  "stack": 999,
```
Everything else on each item (id, created, currency, use, equip, tags) is untouched. energy keeps
`currency.cap: 100` — the currency cap is orthogonal to the stack rule (unlimited stack, capped currency).

### 6.2 `templates/template/tests/fixtures/items_bad.json` — consistency migration (low priority)
This fixture is NOT wired into any automated test (grep: referenced only by design docs + the T0316
taskboard note "items_bad.json фикстура всё ещё в точечной форме (косметика)"). It intentionally fails on
`"id": "gold"` (missing namespace) and missing `created`. Migrate its 6 `stack` lines with the SAME mapping
as 6.1, so its only remaining violations stay the intended ones — keep the bad id/created as-is. Cheap;
keeps the negative fixture honest and hand-diffable.

### 6.3 `features/items-core/scripts/items_ops_test.py` — fixture factory (MANDATORY, hard-coupled)
`make_item` builds catalogs validated against the REAL `item_fields.schema.json`, so an object-shaped
`stack` makes every `ok:true`/exit-0 test fail once the schema is v2 — this migration MUST land in the same
slice as §5. `make_item` (`:52`):
```
-        "stack": {"stackable": True, "max_stack": 999},
+        "stack": 999,
```

### 6.4 `ai_studio/assets/items_viewer/tests/ops.test.mjs` — fixture factory (MANDATORY, hard-coupled)
Same coupling as 6.3: `makeItem`'s catalogs run through `items_ops.py validate` against the real schema, so
the object shape fails every temp-fixture `validate.ok:true` test under schema v2. `makeItem` (`:51`):
```
-    stack: { stackable: true, max_stack: 10 },
+    stack: 10,
```

### 6.5 Regenerated artifact `templates/template/src/generated/items_catalog.gen.c`
This committed file is the CMake codegen OUTPUT; building regenerates it in-place. After the change the
ONLY diff is sword's line: `.max_stack = 0LL,` → `.max_stack = 1LL,` (all other bytes identical). The
executor regenerates by building (§9) and must confirm the diff is exactly this one line.

---

## 7. Validator + codegen changes

### 7.1 `features/items-core/scripts/generate_items_catalog.py`

(a) `stack_fields` (`:143-149`) — rewrite to derive from the int, robust to a pre-migration/malformed
catalog (called by `list` without a prior validate):
```python
def stack_fields(item: dict[str, Any]) -> tuple[bool, int, bool]:
    """Derive the compiled (stackable, max_stack, unlimited) triple from the single
    authoring `stack` int (validate_catalog guarantees int >= 0 first):
        0   -> unlimited stackable    (stackable=True,  max_stack=0, unlimited=True)
        1   -> unique / instance path (stackable=False, max_stack=1, unlimited=False)
        N>1 -> capped stack           (stackable=True,  max_stack=N, unlimited=False)
    """
    stack = item.get("stack")
    if not isinstance(stack, int) or isinstance(stack, bool):
        # malformed / pre-migration catalog: validate() reports it separately.
        # Treat unknown as unique (non-stackable) so `list` / composite-key-length
        # never crash and default to the safe branch.
        stack = 1
    stackable = stack != 1
    unlimited = stack == 0
    max_stack = stack
    return stackable, max_stack, unlimited
```

(b) `validate_catalog` stack block (`:106-109`) — replace the object/`stackable`-presence checks with an
int-range check (the generic required-field loop at `:103-105` already SKIPS `stack` via `!= "stack"`;
keep that skip — this block now covers both presence and type):
```python
        stack = item.get("stack")
        if core.get("stack", {}).get("required"):
            require(
                isinstance(stack, int) and not isinstance(stack, bool) and stack >= 0,
                f"item {item_id!r} 'stack' must be an integer >= 0 "
                "(0=unlimited, 1=unique/instance, N=cap)",
            )
```
Note `c_i64` (`:60`) is a second backstop when the value reaches render (rejects non-int), but it does NOT
reject negatives — the `>= 0` require above is the authoritative guard.

No other change in this file. `render_source` already emits `.stackable/.max_stack/.unlimited` from
`stack_fields` (`:240-253`); the `has_equip` default that used to live in `stack_fields` is GONE (the int
is now required — one fewer implicit rule).

### 7.2 `features/items-core/scripts/items_ops.py`

- `item_json_record` (`:130-157`) — REQUIRED change (Fix A): `list --json` must emit the raw authored int,
  not the derived object. Under schema v2 (`stack:i64`) the viewer renders `stack` through the i64 path and
  the old derived object would print `[object Object]` on every card (§1). Drop the now-dead destructure at
  `:136` and change the record field at `:145`:
```python
-    stackable, max_stack, unlimited = gen.stack_fields(item)
     record: dict[str, Any] = {
         ...
-        "stack": {"stackable": stackable, "max_stack": max_stack, "unlimited": unlimited},
+        "stack": item.get("stack"),   # raw authored int (0=unlimited, 1=unique/instance, N=cap)
         "blocks": block_names(item),
     }
```
  Update the docstring to state that `stack` is the raw int and the derived triple lives ONLY in the
  compiled C catalog (single-sourced through `stack_fields`), never in the read layer. Schema v2 + `list` +
  viewer + future T0316 editor then all speak ONE int. (`gen.stack_fields` import stays — still used by
  `check_composite_key_length`.)
- `check_composite_key_length` (`:425`) — no change; still destructures `stack_fields` and skips uniques
  (now `stackable == false ⟺ stack == 1`).
- `check_equip_unlimited` → rename `check_equip_stack` (`:448-471`), rewrite for D4:
```python
def check_equip_stack(doc: dict[str, Any]) -> list[Issue]:
    """L1 (adapted for the single-int stack model): an item with an `equip` block IS
    a unique instance (per-copy record, instance_id-keyed, count=1, §2.3), which under
    the collapsed model means stack == 1 exactly. 0 (unlimited) or N>1 (capped stack)
    contradicts per-copy ownership."""
    errors: list[Issue] = []
    for item in doc.get("items", []):
        if item.get("equip") is None:
            continue
        stack = item.get("stack")
        if not isinstance(stack, int) or isinstance(stack, bool):
            continue  # generator-check already reported the bad type
        if stack != 1:
            errors.append(
                issue(
                    "equip-stack",
                    f"item {item.get('id')!r} has an equip block (unique instance) but stack={stack} -- "
                    "uniques are single-instance per-copy records, so stack must be 1",
                    id=item.get("id"),
                    field="stack",
                )
            )
    return errors
```
- Call site (`:649`): `errors += check_equip_unlimited(doc)` → `errors += check_equip_stack(doc)`.
- Docstring stable rule-id list (`:37`): `"equip-unlimited"` → `"equip-stack"`.
- Docstring wording (`:26-27`): "equip/unlimited sanity" → "equip/stack sanity".
- Comment cross-ref (`:484`): `check_equip_unlimited` → `check_equip_stack`.
- Type hint on `stack_fields` in the imported module already updated in 7.1(a) to `tuple[bool, int, bool]`.

### 7.3 `.codex/skills/nt-game-items/SKILL.md:62`
Rule-id contract list: `equip-unlimited` → `equip-stack`. (Doc-only; keeps the tool contract accurate.)

---

## 8. Full file-change list

Required:
1. `templates/template/content/item_fields.schema.json` — schema_version 1→2 + stack scalar (§5).
2. `templates/template/content/items.json` — migrate 6 stack lines (§6.1).
3. `features/items-core/scripts/generate_items_catalog.py` — stack_fields + validate_catalog (§7.1).
4. `features/items-core/scripts/items_ops.py` — rename+rewrite equip rule, docstring rule-id (§7.2).
5. `features/items-core/scripts/items_ops_test.py` — migrate make_item + add new-rule tests (§6.3, §7).
6. `templates/template/tests/test_items_catalog.c` — ADD `test_item_core_stack_semantics` (§7 test plan).
7. `ai_studio/assets/items_viewer/tests/ops.test.mjs` — migrate makeItem stack (§6.4).
8. `templates/template/tests/fixtures/items_bad.json` — consistency migrate (§6.2).
9. `.codex/skills/nt-game-items/SKILL.md:62` — rule-id rename `equip-unlimited` → `equip-stack` (§7.3).
10. `templates/template/src/features/items/README.md:157-158` — the validate-rule description
    "an `equip` ⇒ not `stack.unlimited` sanity check" becomes "an `equip` ⇒ `stack == 1` sanity check"
    (Fix D; keeps the game-side README's CLI description accurate to the renamed/tightened rule).
11. `ai_studio/assets/items_viewer/docs/build_spec_phase1_2026-07-08.md:148` — the object-recurse example
    "`stack` -> stackable/max_stack/unlimited" is now wrong (`stack` is i64). Switch the example object to
    `use.params` (the remaining open-bag object field; `items.js:100-104`) — e.g.
    "`object` -> recurse into its `.fields` (e.g. `use.params`)".
12. `templates/template/src/generated/items_catalog.gen.c` — REGENERATED by build (§6.5); leave in working
    tree as evidence, do not hand-edit.

Optional (recommended, comment-only): `features/items-core/include/features/items/items.h` — one-line
comment above the three fields noting they are derived from the `stack` int.

No change: all runtime `.c`, `test_template_composition.c`, `items.lock.json`, `state/items.schema.json`,
CMakeLists (codegen already re-triggers on `items.json`/schema/generator changes), items_viewer site JS/HTML/CSS,
`features/items-core/README.md` / `INSTALL.md` (stack shape is not documented there; the schema `note` + this
spec are authoritative).

Leave-alone (historical / point-in-time docs, do NOT retrofit): `templates/design/item_system_design_2026-07-06.md:42`,
`templates/design/items_feature_study_2026-07-06.md:25`, `templates/design/build_spec_t0327_i2_2026-07-07.md` —
dated design snapshots that describe the old 3-field model as it was; editing them rewrites history.

---

## 9. Test plan

Existing tests to update:
- `items_ops_test.py::make_item` → int stack (§6.3). All existing lock-workflow/created tests then still
  pass unchanged (they build catalogs from `make_item`).
- `ops.test.mjs::makeItem` → int stack (§6.4). The happy-path test (live template, `validate.ok:true`,
  6 items) and every temp-fixture `validate.ok:true` test then still pass against the new schema.

New tests to add (worth it — they lock the new invariants):
- `items_ops_test.py` new `StackFieldTests`:
  - `stack` missing → `generator-check` error (validate_catalog require fires).
  - `stack` = `-1` → `generator-check` error (negative rejected; c_i64 would not catch it).
  - `stack` = `true` (bool) → `generator-check` error (bool-is-not-int).
  - `stack` = `{"stackable": true}` (old object) → `generator-check` error (object rejected).
  - equip + `stack` = `0` → `equip-stack` error; equip + `stack` = `1` → clean (rc 0).
- `test_items_catalog.c::test_item_core_stack_semantics` (RUN_TEST it): assert
  `potion->stackable`, `!potion->unlimited`, `potion->max_stack == 99`; `!sword->stackable`,
  `sword->max_stack == 1`; `gold->unlimited`, `gold->max_stack == 0`. This pins the derivation, including
  the one changed value (sword `max_stack == 1`).

Consumer-side contract assertions (MANDATORY — Fix A2; today NO test would catch the `[object Object]`
regression, because nothing asserts the shape of `list --json`'s `stack`):
- `ops.test.mjs` happy path (~`:88`, live template): assert `view.items` finds sword with `stack === 1`,
  gold with `stack === 0` (raw ints, NOT objects), and `view.schema.core.stack.type === "i64"`. This is the
  regression guard on both the schema shape and the `list --json` output shape.
- `items_ops_test.py` new assertion: run `list --json` (in-process, mirroring `run_validate`) on a fixture
  catalog and assert each item's `stack` is a plain `int` (e.g. `isinstance(rec["stack"], int)` and equals
  the authored value) — the Python-side pin that `item_json_record` emits the raw int, not the derived
  object.

---

## 10. Acceptance gates

- ctest GREEN in BOTH build dirs — `templates/template/build/native-debug` and
  `templates/template/build/devapi-debug` (configure/build each, then `ctest`). Critical targets:
  `test_items_catalog`, `test_items_fragment`, `test_template_composition`, `items_ops_validate`,
  `items_ops_test`, `test_progression`, `test_progression_curve`. (The build regenerates
  `items_catalog.gen.c` as a prerequisite.)
- Python: `py -3.12 features/items-core/scripts/items_ops_test.py` (or `py -3.12 -m pytest <file>`) GREEN.
- Node (per-FILE, not per-dir): `node --test ai_studio/assets/items_viewer/tests/ops.test.mjs` GREEN.
  Precondition: `templates/template` must be built first (the happy-path test resolves icon regions from
  the built pack — an existing coupling, not introduced here).
- `templates/template/build/native-debug` (or devapi) shows the regenerated `items_catalog.gen.c` diff is
  EXACTLY sword `.max_stack` `0LL → 1LL` and nothing else.
- `items.json`, `items_bad.json`, and both fixture factories stay ASCII and hand-diffable (one-value-per-key
  int, no reflowing of unrelated lines).

---

## 11. Executor constraints

- No git commits. No taskboard edits.
- Put all evidence (ctest logs, node/pytest output, `git diff --stat`, the gen.c one-line diff) under
  `tmp/`.
- clangd/IDE diagnostics are NOISE; ninja + ctest are the only truth for the C side.
- Regenerate `items_catalog.gen.c` by BUILDING (do not hand-edit the generated file); verify the one-line diff.
- Engine tree stays read-only (nothing here touches it).

---

## 12. Slice plan

ONE slice. The change is small and cohesive (schema + data + codegen derivation + one validator rule +
fixtures + tests) with no internal ordering hazard: the codegen/validator edits and the data migration must
land together (a half-migrated tree fails validate), and there is no runtime C edit to stage separately.
Splitting would only add cross-slice red states.

---

## 13. Open questions — RESOLVED (fix round, 2026-07-08)

Both independent reviews (angle 1: runtime/C correctness; angle 2: contracts/data/consumers)
returned ACCEPT-WITH-FIXES and independently CONFIRMED all four decisions:
1. D4 tighten `equip ⇒ stack == 1` — CONFIRMED (closes the old "stackable equippable" gap).
2. D2 `schema_version` 1→2 — CONFIRMED (nothing gates on it; honest incompatible-shape signal).
3. D1 JSON key `stack` — CONFIRMED (0/1 edges read cleanly; `max_stack: 0/1` would mislead).
4. D3 keep the derived C triple — CONFIRMED on the smallest-zero-edit-diff argument alone
   (there are ZERO consumers of the compiled triple today; see §2 D3).
