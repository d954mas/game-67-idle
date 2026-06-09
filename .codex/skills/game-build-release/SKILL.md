---
name: game-build-release
description: Use when discovering, adding, fixing, or running game build, launch, debug, release, web, desktop, mobile, console, package, asset-pack, or CI tasks. Triggers include VS Code tasks, CMake presets, build scripts, launch configurations, release outputs, serving web builds, packaging, and explaining how to run or distribute the game.
---

# Game Build Release

Use this skill to make build and launch workflows explicit, repeatable, and discoverable.

## Workflow

1. Discover local build sources before inventing commands:
   - `CMakePresets.json`
   - `.vscode/tasks.json`
   - `.vscode/launch.json`
   - package manager scripts
   - engine docs or examples
2. Separate configure, build, run, release, serve, and package tasks.
3. Give important tasks clear names that show up in the user's IDE.
4. Keep asset-pack generation explicit unless the project intentionally requires automatic packs.
5. Verify changed tasks with the exact command or preset the user will run.

## Task Naming

Prefer labels like:

- `Build: native debug`
- `Release: native`
- `Build: web debug`
- `Release: web`
- `Pack: build game pack`
- `Web: serve release`

When adding IDE launch entries, make them visible in the run/debug picker if the user expects to click them there.

## Validation

After editing build config:

- Parse JSON/YAML/TOML files.
- List available build presets or tasks if the tool supports it.
- Run the smallest affected build.
- State output paths for executables, web artifacts, and packages.

