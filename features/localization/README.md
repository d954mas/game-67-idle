# localization

In-place module (see `features/README.md` — same `.c` across games, data-only
customization). Precedent: `features/progression-core`, `features/items-core`.
One copy of the source lives here; each consuming game compiles it against ITS
OWN generated table.

## What it is

Every player-visible string in a game declared as DATA in
`content/loc/strings.json`, compiled by `scripts/loc.py` into typed C
accessors (`loc_strings.gen.{h,c}`) plus a machine-readable key index
(`loc_keys.gen.json`) for tools that need to name a key without re-implementing
the mangling.

The module is universal: it knows nothing about any game, any surface, or any
key vocabulary. Everything game-specific is data.

## Contents

```text
features/localization/
  include/features/localization/loc.h   public API + the codegen ABI
  src/
    loc.c                    lookup, fallback chain, CLDR plurals, grouping, arena formatting
  scripts/
    loc.py                   strings.json -> loc_strings.gen.{h,c} + loc_keys.gen.json
    loc_test.py              generator contract tests
  schema/strings.schema.json the data contract
  feature.json
  README.md   (this file)
  INSTALL.md
```

## Layer

L1 — depends on the engine only (`nt_mem_scratch` for per-frame allocation,
`nt_log` for the fallback warning). It never includes a game's `src/`, which is
what keeps digit grouping in the language block instead of routing through some
game's number formatter.

## The data

```json
{
  "languages": {
    "ru": { "group_separator": " ", "group_min_digits": 5 },
    "en": { "group_separator": ",", "group_min_digits": 4 }
  },
  "fallback": "ru",
  "strings": {
    "common.close": { "ru": "ЗАКРЫТЬ", "en": "Close" },

    "upgrades.requires": {
      "args": { "parent": "str", "n": "int" },
      "ru": "Нужно {parent} Ур {n}",
      "en": "Requires {parent} Lv {n}"
    },

    "upgrades.coins_needed": {
      "args": { "n": "int:group" },
      "plural_on": "n",
      "ru": { "one": "нужна {n} монета", "few": "нужно {n} монеты",
              "many": "нужно {n} монет" },
      "en": { "one": "need {n} coin", "other": "need {n} coins" }
    }
  }
}
```

### Argument types

Declared explicitly, never inferred from the placeholder name.

| type | generated C parameter | rendering |
|---|---|---|
| `int` | `int64_t` | plain digits |
| `int:group` | `int64_t` | digit grouping from the **language block** |
| `float:1`, `float:2` | `float` | fixed decimals |
| `float:g` | `float` | shortest round-trip (`%g`) |
| `str` | `LocStr` | verbatim |

Digit grouping is language data because it *is* language data: Russian groups
from five digits with a space, English from four with a comma. A game-side
number formatter cannot be both.

### Escaping

`{{` and `}}` are literal braces. `%` is literal everywhere — the generator
emits the interpolation template and `loc.c` walks it directly, so no printf
format string ever originates at a call site.

### Plurals

`plural_on` names the numeric argument that selects the form — explicit, like
the types. Each language's required category set comes from its rule, which the
generator derives from the language code (or from an explicit `"plural_rule"`
in the language block for a code it does not know). The set must match exactly:
a missing form is an error, and so is an unreachable extra one.

The table is **24 partitions over 172 locales, ported verbatim from the
studio's Defold i18n project** — a rule set already shipped in a released game,
including where it takes an older, simpler CLDR snapshot than cldr-latest
(French has no `many` at 10⁶; Portuguese and Hebrew are plain one/other).
Matching that reference is the point: it is the spec, not an approximation.

The required categories are **derived**, not written down: each rule is
evaluated over the integers and the categories it can actually return become
the required set. So "which forms must exist" cannot disagree with the
arithmetic that picks them. Some rules therefore have no `other` at all —
Russian and Polish select one/few/many for every integer.

| rule | locales | required categories |
|---|---|---|
| `one_other` | en de es it nl pt sv he ca ta … (89) | `one` `other` |
| `other_only` | ja zh ko th vi id tr hu fa ka … (31) | `other` |
| `slavic` | ru uk be hr sr bs sh | `one` `few` `many` |
| `polish` | pl | `one` `few` `many` |
| `czech` | cs sk | `one` `few` `other` |
| `french` | fr ff kab | `one` `other` |
| `arabic` | ar | `zero` `one` `two` `few` `many` `other` |
| `slovenian` | sl | `one` `two` `few` `other` |
| … 16 more | br cy ga gd gv ksh lag lt lv mk ro mt shi tzm iu-family hi-family | see `PLURAL_RULES` in `loc.py` |

`loc.c` carries the same 24 partitions again, in C. Neither half is trusted:
`loc.py plural-cases` emits every (rule, n) → category pair from the Python
table as a C array, and `test_loc_e2e` walks all 10 152 of them through the
compiled `loc_plural_category()`. Editing one half without the other is a red
test, not a wrong word on screen.

**One deliberate divergence from the reference.** It selects on the raw number,
so French 1.5 is `one`. Here a fractional value always takes `other`, so a
`plural_on` bound to a float type must declare exactly `other` in every
language, and the rules only ever see integers.

## The generated C

```c
typedef struct { const char *s; } LocStr;

typedef enum LocLang { LOC_LANG_RU = 0, LOC_LANG_EN = 1, LOC_LANG_COUNT } LocLang;
typedef enum LocKey  { LOC_COMMON_CLOSE = 0, /* … */ LOC_KEY_COUNT } LocKey;
typedef enum LocKey0 { LOC0_COMMON_CLOSE = 0, /* … */ LOC0_COUNT } LocKey0;

LocStr loc_common_close(void);
LocStr loc_upgrades_requires(LocStr parent, int64_t n);
LocStr loc_by_key(LocKey0 key);
```

Three properties fall out:

- A raw `const char *` cannot be passed where localized text belongs — `LocStr`
  is a struct, so it does not compile. `loc_raw()` is the explicit, greppable
  escape hatch.
- `loc_by_key` takes `LocKey0`, so a key with arguments cannot be resolved
  without them — `loc_by_key(LOC_UPGRADES_REQUIRES)`, which would paint
  `Нужно {parent} Ур {n}` on screen braces and all, is a compile error. Data
  tables that carry a key (a node's name, an ability's description) type the
  field `LocKey0`.
- Keys are never assembled by string concatenation. The data table carries the
  key; nesting one localized string inside another is a `str` argument fed by
  `loc_by_key`.

The header carries the source text beside each enumerator, so grep works from
the Russian on screen back to the key:

```c
LOC_UPGRADES_REQUIRES = 12, /* ru "Нужно {parent} Ур {n}"  en "Requires {parent} Lv {n}" */
```

Key → identifier mangling is not injective (`a.b_c` and `a_b.c` both give
`a_b_c`), so the generator builds the identifier set and hard-errors on a
collision.

## Return storage

Zero-argument keys return a pointer into the static table — no copy, no length
limit, so a long description never allocates. Formatted strings come from the
engine's per-frame arena (`nt_mem_scratch_alloc`, sized exactly), valid until
the next reset. Copy it if it must outlive the frame.

That makes the arena reset placement load-bearing: it MUST run at the top of
the frame, before anything allocates. `nt_mem_scratch.h` states the contract;
a reset that happens later in the same frame frees pointers already handed to
`nt_ui`.

## Contracts are asserts, holes are data

The engine's rule — *"asserts are contracts, not error handling; never use them
for conditions that can legitimately occur"* — cuts this module in two.

**NT_ASSERT**, because generated code cannot produce it and a plausible string
painted over the bug is worse than a crash: an unbound table (`loc_init()` not
called), a key index out of range, a `LocKey0` a data table carries but the
corpus never declared, an argument count over `LOC_MAX_ARGS`, a NULL `args`
with a non-zero count, a plural argument index outside the argument list, a
language index out of range, a malformed table at bind time.

**Fallback**, because it legitimately happens: a language with no text for a
key (falls back and logs), an absent OS locale passed to `loc_lang_from_code`
(returns -1), a buffer too small for the grouped form in `loc_group_i64`
(drops separators, never digits).

`loc_bind_table` validates the table once so the accessors can trust it
afterwards. `test_loc.c` proves both halves through the engine's hookable
`nt_assert_handler`.

## Numbers that have no key

`int:group` renders a number *inside* a key's text. A coin readout or a bare
price is the whole string and has no text around it, so it has no key —
`loc_group_i64(value, buf, sizeof buf)` groups it for the active language.
Reach for it instead of a game-side thousands formatter, which is one
language's typography applied to all of them.

## Fallback

`active language → fallback language → the key itself`. The fallback language
is complete by construction (the generator errors on a hole), so a missing
translation shows the authoring language — obviously untranslated but
meaningful — rather than `settings.volume.master`. The bare key is the
emergency case only.

Debug builds log the first fallback per key:
`[LOC] no 'en' for upgrades.locked_at (using ru)`. Deduplicated per key per
session — the UI rebuilds every frame, so an undeduplicated warning would emit
sixty lines a second. `loc_fallback_log_count()` exposes the count for tests.

## Validation

`loc.py generate` fails on:

- a placeholder present in one language and absent in another
- an argument declared but used by no form, or used but not declared
- a plural form the language's rule requires, missing — or an extra one the
  rule can never select
- no text for the fallback language
- a mangled-identifier collision
- a key not matching `^[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)+$`
- a malformed or unbalanced brace, or a control character other than tab/newline
- an unknown language code with no explicit `plural_rule`

and warns (without failing) on a missing non-fallback translation.

Placeholder parity is checked per language over the union of that language's
forms, not per form: an English `one` form may read "need a coin" as long as
some form in English still uses `{n}`.

## Output determinism

Keys are emitted in sorted order, so the generated diff does not depend on
where a machine-run import happened to append. `write_if_changed` keeps the
build idempotent.

## Adding a language

Three places, not one:

1. `strings.json` — the `languages` block plus the translations.
2. The game's `state/settings.schema.json` `language` enum.
3. `node tools/state_lock.mjs --update` to re-seal.

Plus the font atlas: the packed charset must cover the new script.
