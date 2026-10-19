# Tool Lib

Small shared helpers for AI Studio command-line tools.

This is a low-level core harness utility layer. It contains dependency-light
helpers for CLI failure paths, hashing, MIME lookup, deterministic
STORE ZIP artifacts, and explicit `tmp/` scratch cleanup.

Do not put domain policy here. Module-specific validation, asset rules, task
state, browser UI, and runtime automation stay in their owning modules.

`zip_store.mjs` is also the ZIP writer of the shared game CLI
(`ai_studio/game_tools`). Games created before that module still carry their own
copy in `tools/lib/`.

Generated or downloaded data does not belong here.

## Commands

```powershell
node ai_studio/core_harness/tool_lib/tmp_sweep.mjs --older-than 14 --include-games
node ai_studio/core_harness/tool_lib/tmp_sweep.mjs --older-than 14 --include-games --apply
node ai_studio/core_harness/tool_lib/tmp_sweep.mjs --all-scratch --dry-run
```

`tmp_sweep.mjs` is opt-in housekeeping for ignored scratch data. It reports by
default and deletes only with `--apply` (entries untouched for the given number
of days, judged by the newest file inside) or `--all-scratch`. `--include-games`
adds every game's `tmp/`. Anything worth keeping belongs in the game's
`design/knowledge/` or its YandexDisk workspace, not in `tmp/`.
