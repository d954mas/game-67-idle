# Tool Lib

Small shared helpers for AI Studio command-line tools.

This is a low-level core harness utility layer. It contains dependency-light
helpers for CLI failure paths, JSON file IO, hashing, MIME lookup, deterministic
STORE ZIP artifacts, and explicit `tmp/` scratch cleanup.

Do not put domain policy here. Module-specific validation, asset rules, task
state, browser UI, and runtime automation stay in their owning modules.

`zip_store.mjs` is mirrored byte-for-byte into
`templates/template/tools/lib/zip_store.mjs` because copied games must remain
standalone; the template packaging test prevents drift across that delivery
boundary.

Generated or downloaded data does not belong here.

## Commands

```powershell
node ai_studio/core_harness/tool_lib/tmp_sweep.mjs --list
node ai_studio/core_harness/tool_lib/tmp_sweep.mjs --all-scratch --dry-run
```

`tmp_sweep.mjs` is opt-in housekeeping for ignored scratch data. It reports by
default and deletes only with `--all-scratch`.
