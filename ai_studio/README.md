# AI Studio

`ai_studio/` is the target home for reviewed and refactored AI game-studio
pipeline modules.

This folder is not a dump of the current repo. Current files stay where they
are until a module is inspected, given an owner, cleaned up, and migrated with
compatibility shims where needed.

## Current Shape

- `tree.json` is the declarative source for the working architecture tree.
- `core_harness/` is the first physical migration target.
- Everything not reviewed yet stays visible through the `Not Refactored` node
  in the generated map.

Do not pre-create domain folders such as `assets/`, `tech/`, or `validation/`
before the corresponding module is reviewed. Create a folder only when the move
is deliberate and the module has a source contract.

## Map Ownership

`docs/ai-pipeline/architecture-map.html` is generated output. Edit
`tree.json`, then rebuild:

```powershell
node tools/architecture_map/build_architecture_map.mjs
```

The generator enriches tree nodes only for display: when a node has a `path`,
it can fill missing title, description, and link data from Markdown sources or
the tool inventory. The hierarchy still comes from `tree.json`.

## Migration Rule

Only move something into `ai_studio/` when it has:

- a clear domain owner;
- a source-of-truth README or contract;
- a public surface agents should use;
- known internal helpers;
- a validation path or an explicit reason why no validator applies.

## Migration Loop

1. Pick one current module from the architecture map.
2. Decide whether it belongs in `ai_studio/`, remains external, or should be
   deleted.
3. Create or update the target module README only after that decision.
4. Move reviewed source files, keeping old public commands as shims until every
   caller is updated.
5. Rebuild the architecture maps and run focused validators.
