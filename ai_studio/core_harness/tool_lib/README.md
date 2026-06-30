# Tool Lib

Small shared helpers for AI Studio command-line tools.

This is a low-level core harness utility layer. It contains dependency-light
helpers for CLI failure paths, JSON file IO, hashing, MIME lookup, path display,
and temporary export copying.

Do not put domain policy here. Module-specific validation, asset rules, task
state, browser UI, and runtime automation stay in their owning modules.

Generated or downloaded data does not belong here.
