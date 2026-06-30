# Web GDD Site

Load this only when creating or revising a visual GDD website, local design
server, editor surface, or web-based art bible.

## Goal

The site is a readable control panel for the current GDD: what the game is,
what the player does, what it looks like, what is accepted, and what should be
implemented next. It is not a marketing page.

## First Screen

The first viewport should answer:

- what game this is;
- what the player fantasy is;
- what the player does in the first minute;
- what the main progression/status number is;
- what visual proof exists;
- what gate is current.

Do not hide the game behind a hero slogan or lore-only intro.

## Sections

Keep sections short and connected:

- concept: hook, fantasy, pillars, no-go list;
- gameplay: first 30 seconds, first 5 minutes, core loop, verbs;
- economy/progression if used: resources, stats, sources, sinks, first unlock;
- activities: actions, unlocks, blocked states;
- UI/UX map: main screen, panels, actions, feedback, navigation;
- visual proof: fake shot, progression shot, prepared asset proof if available;
- references: compact borrow/avoid/copy-risk;
- risks: top gates and smallest tests;
- handoff: first playable scope, commands, next prompt.

## Source Of Truth

Avoid duplicating data manually when structured files exist:

- read/link `data/core_loop.json` for verbs, rules, feedback, risk, goals, and
  continue reason;
- read/link `data/ui_flow.json` for screens/actions;
- read/link `data/asset_manifest.json` for visual assets;
- link current concept/GDD/handoff files.

If the site cannot auto-read the data, include a visible source list and update
both docs and site in the same commit.

## Validation

Before finishing:

- verify HTTP 200 when served locally;
- inspect desktop viewport;
- inspect mobile portrait if web surface is deliverable;
- confirm images load from final project paths, not temp paths;
- confirm fake shots and data match current source-of-truth files;
- confirm no raw generation/source sheets are linked as final assets.
