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

Concept sketch (to refine at design phase):
- type:"genspec" element (additive): x/y/w/h box (the target footprint),
  fields {prompt, refs: [src...], style?, alpha: none|dual_plate|matte,
  status: draft|generating|done|failed, result?: elementId}. Rendered as a
  dashed card with prompt excerpt; refs draggable onto it from the canvas.
- "Generate" action on the card -> runs the generation flow (T0238 engine
  when alpha=dual_plate; plain imagegen otherwise) -> result element minted
  AT the placeholder's box (this subsumes T0238's "hybrid placement": the
  placeholder IS the placement) + linked in card meta; card keeps history of
  runs (N candidates rail later = T0211 masked-regen sibling).
- Agent parity: CLI/skill can create/fill/run placeholders (the lead can ask
  me to fill a screen with placeholders from a GDD list, then generate).
- Relation: T0211 = masked REGEN of regions inside existing art; T0238 =
  the alpha-generation engine; T0239 = the on-canvas UI/workflow object that
  triggers both.

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
