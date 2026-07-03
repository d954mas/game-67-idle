---
id: T0239
title: "Canvas: generation placeholder element - object with prompt/refs/meta, generation invoked from it"
status: idea
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Lead (2026-07-03): "положить объект с мета информацией, рефы, промпт — и из
него позвать генерацию". A GENERATION PLACEHOLDER as a first-class canvas
element: you place a card on the canvas where the art should be, it carries
the generation spec (prompt, ref images, size, style, alpha mode), and
generation is INVOKED FROM the object. The placeholder is the design
document of the future sprite living at its future position.

ESSENCE (lead, 2026-07-03): "по сути я хочу сохранить промпт и рефы, чтобы
потом легко повторять генерации" — the card is a SAVED GENERATION RECIPE.
LEAD CORRECTIONS (same discussion): NO alpha in the placeholder — it
produces RAW art ("Я получаю арт. И потом с артом уже делаю всё что
захочу" — alpha/quantize/etc are his explicit manual steps after). The
placeholder REMAINS on the canvas after generation ("Заглушка остаётся") —
rerunnable anytime.

SKILL MANDATE (lead): ANY generation goes THROUGH a canvas — when the agent
works on a game it creates/uses generation canvases; the recipe (prompt +
refs) is saved there, results land beside it. Extends the existing
canvas-handoff convention (accepted art w/ meta) to the RECIPE itself:
handoff starts BEFORE generation, not after. nt-asset-image-generation
skill must be updated when this builds.

Concept sketch (to refine at design phase):
- type:"genspec" element (additive): x/y/w/h box (the target footprint),
  fields {prompt, refs: [src...], style?, status: draft|generating|done|
  failed, runs: [{at, resultElementId}...]}. Rendered as a dashed card with
  prompt excerpt; refs draggable onto it from the canvas.
- "Generate" action on the card -> plain imagegen -> RAW result element
  minted at/beside the placeholder's box + linked in card runs; the card
  stays. Repeat = press again (new run, new element).
- Agent parity: CLI/skill can create/fill/run placeholders (the lead can ask
  me to fill a screen with placeholders from a GDD list, then generate).
- Relation: T0211 = masked REGEN of regions inside existing art; T0238 =
  auto dual-plate alpha as an ACTION ON AN ART ELEMENT (not part of the
  card); T0239 = the recipe object.

PROMPT TOOLS (lead, 2026-07-03, same discussion):
- "Expand prompt" button on the card: the lead writes a SIMPLE prompt
  ("рыжий кот с мечом"), the button runs an LLM pass that produces the
  production-ready generation prompt (style clauses, composition, bg-for-
  keying, negative constraints) and shows it editable before generation.
  Should weave in the active style preset (below) automatically.
- "Extract style" action on any IMAGE element: a vision pass produces a
  reusable STYLE + CONTENT description ("чтобы я потом переиспользовал") —
  saved as a named style preset; placeholders reference a preset by name and
  the expander weaves it into prompts. Ties directly into T0208 (style
  locks: prefix/suffix + refs) — the preset IS the style lock, extracted
  from an exemplar image instead of written by hand.
- Both are agent-side LLM/vision calls surfaced as canvas actions (vision
  via the codex -i path the visual gate already uses).

## Done when

- [ ] Design phase: data model, card UI, run lifecycle, parity surface,
      increments (needs a design pass before build).

## Open questions

- Placeholder box vs result size mismatch: scale result into the box or
  resize the box to the result?
- Where do ref images live (element srcs vs external paths)?

## Log

- 2026-07-03: created from lead question during dual-plate flow discussion;
  no prior task covered this (T0211 is region regen only).
