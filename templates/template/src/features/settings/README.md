# Settings

## Purpose

Provide the template-owned, copy-then-own settings screen and persisted master,
music, and SFX preferences.

## Public surface

`settings.h` is public: draw/open/close state plus audio preference getters and
setters. Persistence is owned by the template state fragment.

## Validation

Build the template settings/save tests and run
`node features/validate_contracts.mjs`.

## Compatibility

`feature.json.version` is exact SemVer. Patch preserves the public contract,
minor adds backward-compatible surface, and major permits breaking changes.
A copied game owns its revision after creation.

## Extension points

Add game-owned settings through the copied state fragment and screen. Promote
only generalized public-surface changes to this template reference.
