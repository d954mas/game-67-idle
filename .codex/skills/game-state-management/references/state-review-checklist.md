# State Review Checklist

Load this reference when reviewing game-state changes, migrations, generated
files, DevAPI state behavior, fixtures, inventory/equipment refs, or save/load
behavior.

## Review Checklist

- Reject gameplay/UI direct writes to raw `GameState`; require domain actions.
- Reject `game_state_actions` calls from migrations.
- Check integer and enum parsing rejects fractional JSON numbers.
- Check save envelopes validate both `schema` and `document`.
- Check every persisted schema field has a stable `id`; removed fields are
  `reserved`.
- Check every string has `max_length`, and every list/map has `max_count`.
- Check saves use a safe write path: parent dirs, temp file, replace.
- Check `game.state.save/load` use logical `key`; use `unsafe_path` only for
  fixtures and explicit debug.
- Check autosave load/save behavior with a restart scenario, and keep ordinary
  tests isolated from persisted state.
- Check inventory/equipment references point to existing owned objects.
- Check schema edits are reflected in generated headers, runtime C adapters,
  DevAPI paths, actions, fixtures, and scenarios.
- When variants exist, check each variant generates into a separate directory
  and the default clean output has no archived prototype fields.

## Common Failure Signals

- A generated `game_state.*` diff appears without a matching schema or generator
  change.
- A migration contains current runtime constants with no versioned copy.
- A test writes raw state where a semantic action should exist.
- A DevAPI `set` path can modify fields that are not explicitly
  `devapi: read_write`.
- A fixture or bot assumes list index identity for important owned objects.
- A clean template schema contains fields from a closed prototype.
