# Shader checker

Agent instructions: [QTECH_002](rules/technical/checks/QTECH_002_shader_rules.md).
For reference compilation, linking, and vendor metrics, use the shared
[Shader analysis](../shaders/README.md) entry point.

## Usage

```powershell
node ai_studio/quality/check_shaders.mjs [--json] <file-or-directory> [...]
```

- Accepts `.vert`, `.vs`, `.vsh`, `.glsl` and directory trees.
- Reports file, line and rule; `--json` returns file count and diagnostics.
- Exit codes: `0` clean, `1` violations, `2` invalid/unavailable input.
  An empty input tree fails.
- Include every directory supplying shared shader sources.

## Scope

The current check permits only one whole-vector `gl_Position` assignment at
the top level of vertex `main`. Shared `.glsl` files cannot access it.

Continued lines are joined before comments and strings are ignored. Inactive
preprocessor branches are checked; macros/includes are not expanded. The
checker does not require an output assignment or validate GLSL completeness.
Extract embedded C/JS shader strings before checking.

Directory scans skip `.git`, `node_modules`, `build`, `release`, `tmp` and
symlinks. To check generated output, name its real file explicitly.

Full Studio CI and harness-domain verification check `templates`, `features`
and `ai_studio/feature_testing/consumer`. Template/feature-only owner checks
do not run this command. Private games must be named explicitly.

## Adding checks

Extend QTECH_002 and this script for documented problems or reproducible
failures. State the reason, safe alternative and source in the rule. Automate
only unambiguous prohibitions and cover safe/unsafe cases in regression tests.
Keep judgment-based advice for review. Compiler and device checks remain
necessary: translators and optimizers can change the source form.
