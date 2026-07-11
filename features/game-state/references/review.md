# State Review

Load this when reviewing schemas, generated files, migrations, DevAPI state,
fixtures, references, or save/load behavior.

## Checklist

- Check schemas use `schema_version: 2`, a fragment id/version, path-keyed
  fields, and `reserved` tombstone paths; reject numeric field ids.
- Check every string has `max_length`, and every list/map has `max_count`.
- Check generated files follow the schema and were not hand-edited; DevAPI must
  remain the hand-written universal registry dispatch.
- Check all fragments register before save initialization in deterministic
  order: template `settings`, `items`, `progression`, then `game` last.
- Check the envelope validates `format`, `save_version`, metadata, and the
  `features` object; the shell owns each fragment's `v`.
- Check unknown feature blobs round-trip as orphans.
- Check native temp/replace, backup, and quarantine behavior; check web
  namespacing, persistence status, and lifecycle flush.
- Check ABSENT produces `FRESH`, while an existing unreadable save is
  quarantined and produces `CORRUPT_RESET`; check `RECOVERED_BAK` and `NEWER`
  behavior, including zero writes/autosave pause for newer saves.
- Check fragment migration/parse failures reset only the affected fragment.
- Check dirty is set only after validated mutation, retained after save failure,
  and cleared only after durable save.
- Check DevAPI exposes the seven registry commands, paths begin with a fragment
  id, and save/load accept no `key` or `doc` parameters.
- Check normalized schemas expose ordered `fields`, keep `document == fragment`
  for compatibility, and use only `bad_params`/`internal` error codes.
- Check patches validate and commit atomically per fragment group; do not assume
  cross-fragment rollback.
- Reject gameplay/UI direct writes to generated state; require domain actions.
- Reject domain-action calls or mutable runtime dependencies from migrations.
- Check shipped cross-fragment moves bump `GAME_SAVE_DOC_VERSION` and migrate
  raw `features`; otherwise require an explicit pre-ship data-loss decision.
- Check integer and enum parsing rejects fractional JSON numbers.
- Check ordinary tests are isolated from persisted state and exercise restart.
- Check inventory/equipment references point to existing owned objects.

## Failure Signals

- Generated `<id>_state*` changed without schema/generator changes.
- Fragment registration order is nondeterministic or occurs after save init.
- A load error is treated as absence and defaults overwrite a live save.
- A migration uses current runtime constants without a versioned copy.
- A test writes raw state where a semantic action should exist.
- A DevAPI path can modify a field that should not be writable.
- A fixture or bot assumes list index identity for important owned objects.
- A clean template schema contains fields from a closed prototype.
