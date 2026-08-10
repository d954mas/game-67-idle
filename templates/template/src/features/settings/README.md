# Settings

## Purpose

Provide the template-owned, copy-then-own settings screen and persisted master,
music, SFX, and interface-language preferences.

## Public surface

`settings.h` is public: draw/open/close state plus audio and language getters
and setters. Persistence is owned by the template state fragment.

The persisted language is an index into `SettingsStateLanguage`, whose order is
fixed by `state/settings.schema.json`. It becomes the string table's active
language in exactly one place, `settings_apply_language()`, which the game calls
once the save has loaded — before that call every accessor renders the corpus
fallback.

## Validation

Build the template settings/save tests and run
`node features/validate_contracts.mjs`.

## Compatibility

`feature.json.version` is exact SemVer. Patch preserves the public contract,
minor adds backward-compatible surface, and major permits breaking changes.
A copied game owns its revision after creation.
Version `1.1.0` adds the language getter/setter and `settings_apply_language()`,
plus the `language` field in the settings fragment.

## Extension points

Add game-owned settings through the copied state fragment and screen. Promote
only generalized public-surface changes to this template reference.
A game that wants the OS locale as the first-run default detects it itself and
calls `settings_set_language()`; the module deliberately ships no platform
locale probe.
