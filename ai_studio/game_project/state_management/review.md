# State Review

Load this when reviewing game-state changes, migrations, generated files, DevAPI
state behavior, fixtures, inventory/equipment refs, or save/load behavior.

## Checklist

- Reject gameplay/UI direct writes to raw `GameState`; require domain actions.
- Reject domain-action calls from migrations.
- Check integer and enum parsing rejects fractional JSON numbers.
- Check save envelopes validate `schema`, `document`, and integer `version`.
- Check every persisted schema field has a stable `id`; removed fields are
  `reserved`.
- Check every string has `max_length`, and every list/map has `max_count`.
- Check saves use safe paths: parent dirs, temp file, replace.
- Check normal save/load uses logical `key`; `unsafe_path` is only for fixtures
  and explicit debug.
- Check autosave load/save behavior with a restart scenario.
- Check ordinary tests are isolated from persisted state.
- Check inventory/equipment references point to existing owned objects.
- Check schema edits are reflected in generated headers, runtime C adapters,
  DevAPI paths, actions, fixtures, and scenarios.
- When variants exist, check each variant generates into a separate directory.

## Failure Signals

- Generated `game_state.*` changed without schema/generator/template changes.
- A migration contains current runtime constants with no versioned copy.
- A test writes raw state where a semantic action should exist.
- A DevAPI state path can modify a field that should not be writeable.
- A fixture or bot assumes list index identity for important owned objects.
- A clean template schema contains fields from a closed prototype.
