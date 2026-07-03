---
id: T0239
title: "Canvas: generation placeholder element - object with prompt/refs/meta, generation invoked from it"
status: doing
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
- type:"genspec" element (additive): x/y/w/h = the card's own visual size
  ONLY (lead: frame does NOT influence generation — model output size is
  not ours to control; results land at native size). Fields {prompt,
  style?, status: draft|generating|done|failed, runs: [{at,
  resultElementId}...]}.
- REFS = IMAGES DROPPED INSIDE THE CARD (lead's design, 2026-07-03): the
  card behaves like a group — the lead drops N image elements into it and
  those members ARE the refs that ship with the generation (visible, live
  on the canvas, reorder/remove = plain element ops). Same-canvas elements
  only. Our generate path passes input images; the script currently wires
  ONE input image — extending to N refs is part of this build. Ref roles
  (style vs content) — see genart research before deciding.
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
- 2026-07-03: lead decision: card frame does NOT influence generation (cannot control model output size anyway) - result lands at native size; frame is just the card's own visual size, 'target footprint' framing dropped
- 2026-07-03: lead decisions: refs = images dropped INSIDE the card (group-like membership = ref list, same canvas); style extraction v1 = element meta + re-extract + copy-to-clipboard; OPEN research question: style/description as first-class reusable canvas TYPES - how competitors model this (Recraft styles, MJ sref, Krea/Leonardo presets)
- 2026-07-03: Design phase launched (deep-reasoner, doc-only): recipe card per settled lead decisions; style-library question folded in as the doc's open question.
- 2026-07-03: Design done: tmp/design_T0239_recipe_card_2026-07-03.md. Card = GROUP + additive group.recipe (store spreads groups verbatim - zero store changes); refs = member images; generate mints raw element in PARENT scope (results never feed back as refs); seams mirror T0238 (recipe_generate.mjs image, prompt_assist.mjs text via codex exec). 3 sequential increments (share ops.mjs+inspector.js - also blocked on T0232-3 landing). Open question to lead: style library as increment 4 vs backlog task (recommendation: backlog; reserve nullable recipe.style_ref now).
- 2026-07-03: Style discussion settled with lead: STYLE CARD component supersedes meta+clipboard (design doc REVISION R1). Card = name + style prompt + ONE ref image (sent to generation, marked with ref label + border, Make ref button, first-drop auto) + N examples (never sent). Recipe links style via inspector dropdown (recipe.style_ref); generation appends style prompt + attaches style ref alongside recipe refs. Extract-style mints a style card beside the source. Library = a canvas of style cards (lead builds his own). Codex engine only (T0240 REST drops refs; agy/gemini = separate task only on ask). Increments re-scoped to 4, sequential, start after T0232-3 frees ops/inspector.
- 2026-07-03: Lead: engine choice on the recipe card - codex | gemini (design R2). recipe.engine field, inspector dropdown + CLI --engine; gemini via agy wrapper (skill Path B); worker must VERIFY ref support on agy - if absent, refs+gemini = loud refusal, never silent text-only (no second T0240). Dual-plate stays codex-only (subject-locked edits).
- 2026-07-03: Lead: engine 'both' confirmed (design R3) - gen_both-style compare: two result elements side by side (codex+agy), one journal entry, partial success allowed with loud skip report; refs caveat per R2.
- 2026-07-03: Increment 1 (recipe card object + inspector, NO canvas badge - deferred to inc 2 to avoid workspace.js conflict with the rotate-handle worker) fast-worker launched.
- 2026-07-03: Increment 1 landed + committed f71a54b8 (card object, inspector Recipe section, CLI/API parity, copy/paste carries recipe, export skips cards). Increment 2 (generate end-to-end) launching next - workspace.js badge still deferred (rotate-handle worker owns the file).
- 2026-07-03: Increment 2 BACKEND launched (ops/tools/api/cli/actions/tests - inspector.js excluded, owned by T0249 worker; Generate button enable = orchestrator inline after both land).
- 2026-07-03: Increment 2 LIVE-VERIFIED: lead's chest prompt on Demo card grp_a0f668ca generated via CLI recipe-generate -> el_bb322a00 'Recipe card codex' 1254x1254 beside the card, last_run ok, one journal entry. Generate button enabled in inspector (6204a28c), server restarted. Remaining: increment 3 (style card) + 4 (expand/extract).
- 2026-07-03: Increment 3 (style card) launched: fast-worker owns ops/api/cli/inspector/workspace/css + new style.test.mjs. Spec: group.style blob {v,prompt,ref}, auto-ref on first image, Make ref, recipe style_ref dropdown linkage, effective prompt = recipe + '\n\nStyle: ' + style prompt, style ref appended to refPaths (last), examples never travel, style_snapshot in frozen meta. R2 gemini-refs caveat obsolete post-T0252.
- 2026-07-03: Lead confirmed: generation works (2026-07-03 afternoon). Increment 3 worker mid-flight: ops/api/cli/inspector done, workspace/css/tests remain.
