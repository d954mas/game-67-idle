---
type: Tool Guide
title: RB Dark RPG Content Editor
description: Local web editor for RB Dark RPG design data.
tags: [tooling, editor, content]
game_id: rb-dark-rpg
status: draft
---

# RB Dark RPG Content Editor

## Server Mode

Run from the repository root:

```powershell
node games/rb-dark-rpg/design/editor/server.mjs 5191
```

Then open:

```text
http://127.0.0.1:5191/
```

The editor reads and writes only allowlisted files from
`games/rb-dark-rpg/design/data/content_manifest.json`.

## Folder Mode

You can also open `index.html` directly in Chrome and press `Open data folder`.
Pick either `games/rb-dark-rpg/design/data` or `games/rb-dark-rpg/design`.
The editor then uses browser file access to save the same JSON files without a
Node server.

Current scope:

- browse/search characters, locations, items, shops, healing services,
  dialogues, quests, encounters, and assets;
- show thumbnails and large previews for assets with `file_path`;
- group entities by type/status/location/priority/archetype where available;
- filter lists by the same high-signal fields;
- edit common scalar fields in grouped sections instead of one flat form;
- edit the full selected entity as JSON;
- validate duplicate ids, broken refs, quest steps, dialogue links, and item
  stat keys;
- save changed JSON files through the local server.
