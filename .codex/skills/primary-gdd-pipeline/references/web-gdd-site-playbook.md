# Web GDD Site Playbook

Load this file only when creating or revising a visual GDD website, local design
server, editor surface, or web-based art bible.

## Goal

The site is not a marketing page. It is a readable visual control panel for the
current GDD: what the game is, what the player does, what it looks like, what is
accepted, and what should be implemented next.

## First Screen Requirements

The first viewport should answer:

- what game this is;
- what the player fantasy is;
- what the player does in the first minute;
- what the main progression/status number is;
- what visual proof exists;
- what gate is current.

Do not hide the actual game behind a hero slogan or lore-only intro.

## Required Sections

Keep sections short and connected:

- Concept: hook, fantasy, pillars, no-go list.
- Gameplay: first 30 seconds, first 5 minutes, core loop, verbs.
- Core loop: player verbs, rules, feedback, risk, goals, continue reason.
- Progression/economy, if used: resources, stats, sources, sinks, first unlock.
- Activities: actions, unlocks, blocked states.
- UI/UX map: main screen, panels, actions, feedback, navigation.
- Visual proof: fake shot, progression shot, runtime asset proof if available.
- References: compact borrow/avoid/copy-risk.
- Risks: top gates and smallest tests.
- Handoff: first playable scope, commands, next prompt.

## Visual Requirements

The site must include actual images when visuals are in scope:

- gameplay fake shot;
- progression image;
- art bible or runtime asset preview;
- composed runtime proof if claiming runtime-ready assets.

Text descriptions alone are not enough. A site with no game screen is not a
visual GDD.

## Source-Of-Truth Rules

Avoid duplicating data manually when structured files exist:

- read/link `data/core_loop.json` for player verbs, rules, feedback, risk, goals, and continue reason;
- read/link `data/ui_flow.json` for screens/actions;
- read/link `data/asset_manifest.json` for visual assets;
- link current concept/GDD/handoff files;
- show "last updated" or current gate when useful.

If the site cannot auto-read the data, include a visible source list and update
both docs and site in the same commit.

## Editable Surface

If the user wants to edit later:

- serve over a local web server, not only `file://`;
- document the server command;
- keep editable source files simple and durable;
- do not write edits into generated/minified bundles only;
- add editor whitelist or file map if the project has an editor UI.

## Layout Rules

- Use dense, scannable product-doc layout, not a landing page.
- Keep game fake shots near gameplay/economy sections.
- Show relationships with simple diagrams, tables, or cards only where they
  clarify flow.
- Avoid nested decorative cards and hero-only presentation.
- Desktop must be comfortable for review; mobile portrait must remain readable
  when the site is a deliverable.

## Validation

Before finishing:

- verify HTTP `200` for the site when served locally;
- inspect desktop viewport;
- inspect mobile portrait viewport if web surface is deliverable;
- confirm images load from final project paths, not temp paths;
- confirm fake shots and data shown match current source-of-truth files;
- confirm no raw generation/source sheets are linked as final assets.

### Local Server Rule

Keep server validation cross-platform by default:

1. Prefer existing project commands or direct `node`/`python` scripts.
2. Run the server in foreground first to reveal syntax/runtime errors.
3. For automated checks, prefer a small cross-platform Node/Python validator or
   browser automation over shell-specific process management.
4. Use shell-specific wrappers such as PowerShell `Start-Process`,
   `Start-Job`, Bash job control, or OS launchers only when a stable project
   command is unavailable and the task truly needs a background process.

Do not spend repeated attempts on fragile background wrappers, redirected logs,
or platform launchers. If a simple foreground run already proves the server
works, switch to a deterministic validator or browser check instead of trying
more process-launch variants.

## Failure Modes

- Beautiful website, but no fake gameplay screenshot.
- Site says "current GDD" while numbers differ from JSON.
- Images load from `tmp/` or ignored raw generation folders.
- Hero/lore dominates and first player action is unclear.
- Editor exists but only edits generated output.
- Mobile layout hides tables, buttons, or fake shots.
