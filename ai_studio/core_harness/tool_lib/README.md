# Tool Lib

Small shared helpers for AI Studio command-line tools.

This is a low-level core harness utility layer. It contains dependency-light
helpers for CLI failure paths, JSON file IO, hashing, MIME lookup, and explicit
`tmp/` scratch cleanup.

`studio_config.mjs` also owns the single environment-level Python resolver;
ordinary modules consume it rather than duplicating interpreter discovery.

Do not put domain policy here. Module-specific validation, asset rules, task
state, browser UI, and runtime automation stay in their owning modules.

Generated or downloaded data does not belong here.

## Commands

```powershell
node ai_studio/core_harness/tool_lib/tmp_sweep.mjs --list
node ai_studio/core_harness/tool_lib/tmp_sweep.mjs --all-scratch --dry-run
```

`tmp_sweep.mjs` is opt-in housekeeping for ignored scratch data. It reports by
default and deletes only with `--all-scratch`.
