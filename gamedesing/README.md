# Game Design Workspace

Current active test:

- `fantasy-pocket-rpg/` - compact fantasy RPG concept, refs, visual GDD site, first slice, and data contracts.

Reusable knowledge:

- `knowledge/` - short Obsidian-like design notes and reusable lessons.

Rules:

- Keep temporary generation and rejected work in `tmp/` or ignored source folders.
- Put final durable docs/data/assets inside the current project folder.
- Do not expand broad docs before a concrete first slice and visual proof direction are clear.
- Previous test artifacts were removed from the active workspace.

Run current visual GDD:

```powershell
node gamedesing\fantasy-pocket-rpg\server.mjs
```

Open:

```text
http://127.0.0.1:8068/
```

Validation note: keep checks cross-platform. Prefer direct `node`/`python`
scripts and foreground server runs. Avoid spending time on PowerShell-specific
background wrappers for ordinary GDD/site checks.

Validate current GDD package:

```text
python gamedesing/fantasy-pocket-rpg/tools/validate_package.py
```

Validate current visual site without keeping a server running:

```text
node gamedesing/fantasy-pocket-rpg/tools/validate_site.mjs
```
