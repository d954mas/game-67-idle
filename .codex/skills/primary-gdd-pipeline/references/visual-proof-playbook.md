# Visual Proof Playbook

Load this file only when producing visual GDD deliverables, fake gameplay
screens, generated art, runtime asset packs, or review packets.

## Visual Tier Decision

- Reference: mood, tone, competitor, meme, UI density, or art target. Not enough
  for implementation.
- Fake shot: one composed gameplay screen showing what the player sees, does,
  wants, and can press. Required before broad GDD expansion when the user needs
  to see the game.
- Runtime asset pack: separate production-oriented images with manifest,
  dimensions, transparency expectations, and a composed proof screen.

## Gameplay Fake Shot Checklist

A fake shot must show:

- player avatar or main object;
- current status/progression number;
- 2-4 top currencies/stats;
- primary action button or active activity;
- one upgrade or choice;
- next unlock or goal;
- feedback/result from the last action;
- environment/home/status visual state;
- enough UI density to imply the real product genre.

Reject fake shots that look like posters, splash art, isolated characters,
generic landing pages, or mood boards.

## Progression Shot Checklist

Show the same hero/world at 3-5 states:

- starting state;
- first visible improvement;
- midgame identity shift;
- late-game aspirational state;
- final fantasy if useful.

Each state needs a readable gameplay meaning, not only a nicer outfit or
background.

## Image Prompt Structure

Use concrete prompts:

```text
Game UI fake screenshot for [genre/platform].
Player fantasy: [specific fantasy].
Scene: [where the player is].
Visible UI: [currencies], [stats], [primary action], [upgrade], [goal].
Character/state: [hero status and visual state].
Art style: [camera/framing/finish].
Tone: [humor/meme/drama].
Avoid: [forbidden refs, poster style, unreadable UI, text-heavy blobs].
Composition: [portrait/mobile or desktop ratio], readable hierarchy, gameplay-first.
```

Do not ask image generation to create exact small text if the final tool cannot
reliably render it. Add precise UI text later with HTML/CSS/canvas or editing.

## Runtime Asset Pack Acceptance

Runtime-ready art requires:

- separate character/object PNGs;
- transparent backgrounds where sprites/UI need compositing;
- backgrounds separate from characters and UI;
- UI frames/buttons/icons as separate files or implementation-native shapes;
- manifest with id, file, dimensions, usage, and fallback;
- composed screen proving the separate assets recreate the target fake shot;
- ignored raw sheets, failed generations, and temporary composites.

## Visual Review Packet

After the first strong fake shot or art direction, stop and report:

- image path(s);
- visual tier;
- what gameplay is visible;
- what is intentionally placeholder;
- what should be accepted, changed, or regenerated;
- next gate if accepted.

Ask the user to choose one of: keep direction, adjust specific elements, or
regenerate from a different reference.

## Common Visual Failure Modes

- Beautiful character art but no gameplay.
- UI labels exist but do not imply decisions.
- Too much lore/drama, no current action.
- Generated text is unreadable and treated as final.
- Runtime assets are actually one flattened poster.
- Website layout shows sections but no product screen.
