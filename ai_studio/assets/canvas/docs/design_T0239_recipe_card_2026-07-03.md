# Design — T0239 Generation Recipe Card (canvas module)

Date: 2026-07-03. Module: `ai_studio/assets/canvas/`. Design only — no production
files changed by this doc. Folds in `tmp/research_genart_workflow_2026-07-03.md`
and `tmp/research_style_objects_2026-07-03.md` (do not redo that research).

## 1. Summary / recommendation

Build the recipe card as a **GROUP carrying an additive `group.recipe` object** —
not a new element type. The group primitive already gives us a container, a framed
render with a label, membership (drag an image in = a ref), move-as-a-whole,
z-order, marquee, copy/paste, and undo **for free**; a recipe is just a group with a
`recipe` meta blob and one extra inspector section. This satisfies the schema
additivity law (additive-only, compose existing primitives) and maps 1:1 onto
decision 3 (refs = member images that travel with the call).

Three increments, run **sequentially** (each builds on the prior, all touch
`ops.mjs` + `inspector.js`): (1) the card object + `recipe` meta + inspector
surface; (2) the generate action end-to-end with an injectable generator seam +
placement; (3) the Expand-prompt / Extract-style text helpers through ONE codex
text seam.

Generation mints a **NEW raw image element** each run (decision 5), placed
**outside** the card (decision 8), never replacing anything; the card persists
(decision 2). Every gesture is one op / one journal entry with strict tool parity
(site button = CLI verb = API route), mirroring the existing `alphaDualPlateGenerate`
precedent (T0238).

One genuine open question for the lead: **where reusable STYLE lives** — his settled
meta+clipboard (decision 6) vs the competitor-consensus named-preset library. Ship v1
exactly as decided, reserve a `style_ref` pointer now, backlog the library. Details in
§7 and §11.

## 2. Settled constraints honored (recap, not reopened)

Card exists to REPEAT (1); PERSISTS after generating (2); refs = member images that
travel with the call (3); frame/size does NOT feed the generation (4); output = RAW
art, no alpha, a NEW element, alpha is a separate later action (5); Expand-prompt +
Extract-style helpers, extract → META (re-generatable) + CLIPBOARD (6); all
generation goes through the canvas (7); placement hybrid — prompt-only card →
viewport center, regenerate-from-element → beside the source (8); ops with strict
parity, one journal entry per gesture, loud errors (9).

## 3. Card representation — a GROUP with `group.recipe` (decided)

**Chosen: recipe card = a group (`groups[]` entry) with an additive optional
`recipe` object.** Confirmed cost-free: `store.updateProject` spreads `groups`
through verbatim with no per-group normalization (`store.mjs:189-194`), exactly how
`group.background` and `group.clip` were added additively — a new `group.recipe`
field round-trips through every snapshot, undo/redo, and `project.json` read/write
with **zero store changes**. The ops layer owns all recipe validation (loud).

Rejected — **new `type:"recipe"` element**: would force re-implementing container
semantics (members, framed render, move-cascade, marquee, copy/paste) that the group
already has, and members-as-refs (decision 3) IS the group container model. Net
negative against the additivity law. Rejected — **style/prompt as free-floating
canvas objects** (node-graph tools only; see style research Part 2): contradicts our
flat model and still needs a library underneath.

**What marks a group as a card:** presence of `group.recipe`. A card is not a
"screen"; readers that iterate screens (`exportProject`, project export) must skip
groups with a `recipe` (a card is a workshop object, not an exportable screen) —
one guard, noted in increment 2.

**Decision 4 (frame doesn't feed the call):** the generate op reads
`recipe` + member images, never the group's `w`/`h`. The frame is only a visual
container. No enforcement needed — just never pass frame dims to the generator.

**Rendering / inspector surface (thin-page architecture, inspector-driven):**
- Canvas: the card draws as a normal group frame + label, plus a small **"Recipe"**
  badge and a one-line prompt preview inside the frame (precedent: T0231 live-text /
  layers content preview). No interactive buttons painted on canvas — the page is
  thin and every action is an API call.
- Inspector: an additive **Recipe** section shown when the selected group has
  `recipe` (same pattern as the plate-thumbnails row that appears only when
  `meta.alpha.plates` exists). Holds: title, the prompt textarea, a compact params
  row, a refs list (= member images, read-only mirror of membership), and the
  **Expand prompt / Generate** buttons. Extract-style is on an image element's
  inspector, not the card.

## 4. Data model

### 4.1 `group.recipe` (the durable recipe — lives on the card)
```jsonc
{
  "v": 1,
  "prompt": "<lead's simple base prompt>",
  "expanded": "<last Expand-prompt output, editable; the labeled template>" | null,
  "use_expanded": true,          // generate sends `expanded` when present+enabled, else `prompt`
  "params": {
    "size": "1024x1024",
    "quality": "high",           // generate_image.py choices: low|medium|high|auto
    "model": "gpt-image-2",
    "bg_key": "#FF00FF",         // flat key colour for a LATER cutout (research: magenta/green, no decontam)
    "supersample": true,         // advisory: gen 2x, export-fit (research clean-art ladder); v1 stores it, gen honors size
    "n_candidates": 1            // reserved; >1 = gen_batch fan-out (v2, since per-call n=1)
  },
  "style_ref": "<preset-id>" | null,  // by-id pointer, reserved now (see §7); null in pure-decision-6 v1
  "last_run": { "at": "...", "gen_hash": "...", "result_element_id": "el_...", "verdict": "ok" } | null
}
```
**Refs are NOT in `recipe`.** They are the group's member image elements
(`elements[]` with `groupId === cardId`, `type === "image"`), discovered at generate
time (decision 3). This is why the card is a group: dropping an image in is the
existing `assignToGroup` op — no new membership machinery.

**No persisted status machine.** "draft" = `last_run == null`; "done" = a run
exists; "generating"/"error" are transient (the long-op queue toast, like
detect/slice). A persisted `status:"generating"` risks a stuck card if the process
dies — avoid it; `last_run` is the only durable signal.

### 4.2 Result element `meta.recipe` (the frozen per-run snapshot)
Each generated element carries its own frozen recipe snapshot so an old result
**reproduces via cache** even after the card's prompt later changes (research: freeze
per run; the `gen_hash` sidecar is the identity/cache key — do not invent a new one):
```jsonc
element.meta.recipe = {
  "cardId": "grp_…",
  "gen_hash": "<generate_image.py sidecar hash>",
  "run_at": "…",
  "prompt_snapshot": "<exact final prompt sent>",
  "refs_snapshot": ["files/<hash>.png", …],   // <=5
  "params_snapshot": { "size": "…", "quality": "…", "model": "…" }
}
```
`meta.alpha` is **absent** (decision 5: raw, no alpha). The `cardId` back-pointer is
what powers decision 8's "regenerate from an existing element → beside the source"
(the page finds the card via `meta.recipe.cardId`).

### 4.3 Provenance → `tool_runs` (the existing pattern)
One `tool_runs` row per generate gesture, exactly like every other tool
(`ops.mjs` `recordToolRun`/`capToolRuns`, e.g. `alpha_dualplate_generate` at
`ops.mjs:2905-2944`):
```jsonc
{ "id": "run_…", "op": "generate_from_recipe", "cardId": "grp_…", "at": "…",
  "params": { "prompt_snapshot": "…", "refs": [...], "size": "…", "quality": "…",
              "model": "…", "gen_hash": "…" },
  "result_summary": { "result_element_id": "el_…", "bytes": 1234567, "verdict": "ok" } }
```

### 4.4 Version relationship (decision 5: new element, never replace)
Every `generateFromRecipe` mints a fresh element; the card and all prior results are
untouched; the card persists (decision 2). Repeated presses form a natural candidate
rail (research: the "prompt repetition" strategy). Nothing is ever overwritten.

## 5. Generation flow, seam, placement (increment 2)

**Op:** `generateFromRecipe(root, { projectId, groupId, generator?, x?, y? })`
(async; mirrors `alphaDualPlateGenerate`, `ops.mjs:3008`). Steps:
1. Validate loudly: project + group exist, group has `recipe`, prompt non-empty.
2. Collect member image srcs (`groupId === cardId`, `type === "image"`); **>5 is a
   loud error** (generate_image.py `--input-image` cap; research ≤5); 0 refs is fine
   (prompt-only card).
3. Resolve the text to send: `use_expanded && expanded ? expanded : prompt`.
4. Freeze the run snapshot (the `gen_hash` inputs) BEFORE the call.
5. Call the injectable `generator({ prompt, refPaths, params }) -> Buffer|path`
   (default shells `generate_image.py`). Generation runs **outside** the journal
   (mirrors T0238: only the final mint commits).
6. Mint a NEW image element via `storeAddImage` (which accepts `meta`, `store.mjs:220`)
   with `meta: { recipe: <snapshot> }`, RAW, no alpha (decision 5).
7. Record the `tool_runs` row, set `recipe.last_run`, in ONE `commitMutation` — one
   journal entry, one undo removes just the new element (card untouched).

**Generator seam** — new `tools/recipe_generate.mjs`, an exact structural mirror of
`tools/dual_plate_generate.mjs`:
- Pure builders (tested directly, no spawn): `buildGenerateCommand({ prompt,
  refPaths, size, quality, model, outPath })` → argv
  `[GENERATE_IMAGE_SCRIPT, "--prompt", p, ("--input-image", ref)×N, "--size", …,
  "--quality", …, "--model", …, "--out", …]`.
- Default `generateImage({ prompt, refPaths, params })` spawns `generate_image.py`
  into a throwaway temp out-path, returns the bytes. Codex/network failure = a loud
  Error, no silent fallback.
- Tests inject a fake `generator`, so **codex never runs in the suite** (the T0238
  contract).

**Refs path caveat (fold in, flag as dependency):** `generate_image.py` attaches
`--input-image` only on the **codex-backend** credential path (`gen_codex`); the
`sk-` REST path (`gen_rest`) silently drops refs (the T0240 bug, backlogged). So the
card's member refs reach the model **only on the codex-backend path today.** The
seam passes `--input-image` per ref regardless (correct on codex now, correct on
REST once T0240 lands). Note this in the increment-2 packet so refs aren't assumed to
work on an `sk-` key.

**Placement (decision 8) — parity-safe:** the op takes optional `x`/`y`. The result
element's scope is the **card's own parent scope** (`groupId = card.parentId ?? null`,
i.e., a sibling of the card) — **never `groupId = cardId`**, so a result can never
become a ref feeding future runs (a key correctness point). Per-client placement:
- Page, prompt-only card: passes **viewport center** (page state the op can't know).
- Page, regenerate-from-element (decision 8 second half): the element's Regenerate
  button reads `meta.recipe.cardId`, calls the SAME op with `x`/`y` **beside the
  source element** (like `alphaDualPlateGenerate`'s +16px placement).
- CLI (no viewport): default = 24px **below** the card frame (deterministic,
  headless-safe). Same op, different arg — parity preserved because the viewport
  genuinely exists only on the page (same shape as `addImage` taking `x`/`y`).

**Chaining note:** the raw result on a flat key bg feeds straight into the existing
Alpha section — `alphaDualPlateGenerate` works from ANY art since T0248, so
card → raw art → cutout is a clean, already-built downstream.

**Long-op:** the page runs generate through the existing `runLongOp` queue (like
detect/slice/render — it is a slow python+codex spawn) with a pinned-result toast.

## 6. Expand-prompt / Extract-style — ONE codex text seam (increment 3)

**Transport:** the sanctioned codex **CLI** path — `codex exec "<instruction>"`
returns text on stdout, and `codex exec -i <image> "<instruction>"` gives vision
(the `-i` vision path already used by the visual gate). This is the text/VLM analog
of the image seam; both helpers go through ONE injectable module so the codex
invocation lives in exactly one place (mirroring `dual_plate_generate.mjs`).

**Seam** — new `tools/prompt_assist.mjs`:
- Pure builders (tested directly): `buildExpandInstruction({ prompt, styleBlock })`
  → the labeled `[TASK][SUBJECT][STYLE][COMPOSITION][BACKGROUND][CONSTRAINTS][OUTPUT]`
  template from the research (§"Recommended prompt template"); restates exclusions;
  never mentions a seed. `buildExtractInstruction()` → asks for the research
  §"style-extraction schema" JSON + a plain-language description.
- Default impls `expandPrompt({ prompt, styleBlock }) -> string` and
  `extractStyle({ imagePath }) -> { style, description }` shell `codex exec`
  (extract with `-i imagePath`). Tests inject a fake `assistant`, so codex never runs
  in the suite.

**Op — Expand:** `expandRecipePrompt(root, { projectId, groupId, assistant? })`:
reads `recipe.prompt` (+ resolved style block if `style_ref` set), calls
`assistant.expand`, writes `recipe.expanded`, one journal entry. The lead edits the
result in the textarea before Generate (research: show editable, skimmable).

**Op — Extract (decision 6):** `extractStyle(root, { projectId, elementId,
assistant? })`: resolves the element's png, calls `assistant.extractStyle`, writes
`element.meta.style = { …schema, description, at }` (META, decision 6), one journal
entry; **returns** `{ style, description, clipboard }` where `clipboard` is the
pasteable `style_block + constraints + description` text (CLIPBOARD, decision 6 —
the page copies it, the op just returns it). "Regenerate ability" (decision 6) =
re-running the op overwrites `meta.style` (a new journal entry). VLM JSON is parsed
loudly (a non-JSON reply is a clear error, no silent fallback).

**Research correction to note (not a blocker):** VLMs are unreliable on exact hex —
the palette should ideally be **sampled from real pixels** (we have quantize
tooling), not VLM-guessed. v1 stores the VLM's described palette; a pixel-sampled
palette is an additive enhancement (needs a python quantize spawn) — flag as a
follow-up, don't block T0239.

## 7. Style reuse — the reconciliation (decision 6 vs the research)

Decision 6 is FIXED: extract → META + CLIPBOARD. The style research is blunt that
this is options (a)+(d) — the two **worst for reuse**: meta is buried in one element
on one canvas, clipboard drops refs + palette. The competitor consensus in our
product family (Recraft/Freepik/Ideogram/Firefly/MJ) is unanimous: a reusable style
is a **named preset in a side-panel library**, referenced by id, giving cross-canvas
and cross-game reuse — which is the lead's own stated goal ("чтобы я мог их
переиспользовать").

**Reconciliation that does NOT reopen decision 6** (additive layer on top):
- v1 ships decision 6 **exactly** — `extractStyle` writes `element.meta.style` +
  returns the clipboard payload. Nothing removed.
- Reserve `recipe.style_ref` (a nullable by-id pointer) in the schema **now**
  (additive, zero cost) so a library slots in later without reworking the card.
- The optional promotion path (backlog, not v1): one verb
  `saveStylePreset({ elementId | inline, name, scope })` lifts a `meta.style` blob
  into a named record — per-game `games/<id>/design/styles/<slug>.json` (default),
  optional global `ai_studio/.../styles/` for cross-game reuse. The card references
  it by id; Expand resolves the id → injects `style_block` + `constraints_block` +
  auto-attaches the preset's refs; each run freezes the resolved style into its
  `meta.recipe` snapshot (reproduce-by-cache after a preset edit).

This is the one open question worth the lead's time (§11): ship v1 meta+clipboard
only and backlog the library, or add the library as a 4th small increment now.

## 8. Tool-parity surface (law 9 — one op layer, two equal clients)

| Op (`ops.mjs`) | API route | CLI verb | Page |
|---|---|---|---|
| `createRecipeCard` | `POST /projects/:id/recipe-cards` | `recipe-create` | tool-rail / context "New recipe card" |
| `patchRecipe` | `PATCH /projects/:id/recipe-cards/:gid` | `recipe-set` | Recipe inspector (prompt/params edits) |
| `generateFromRecipe` | `POST /projects/:id/recipe-cards/:gid/generate` | `recipe-generate` | **Generate** button (long-op queue) |
| `expandRecipePrompt` | `POST /projects/:id/recipe-cards/:gid/expand` | `recipe-expand` | **Expand prompt** button |
| `extractStyle` | `POST /projects/:id/elements/:eid/extract-style` | `style-extract` | **Extract style** on an image element |

Refs in/out = the existing `assignToGroup` / layers-drag (no new op). Every row is
one journal entry (generate/expand/extract commit once; generation itself runs
outside the journal). Errors are loud and specific (bad id, >5 refs, empty prompt,
non-image element, unparseable VLM JSON, codex failure).

## 9. Increment split (sized for fast-workers, run SEQUENTIALLY)

They share `ops.mjs` + `site/inspector.js` + `site/actions.js`, so **do not
parallelize** — one fast-worker per increment, in order (2 needs 1's card object;
3 needs 1's card + inspector section).

**Increment 1 — card object + `recipe` meta + inspector surface (no generation).**
- Files: `ops.mjs` (`createRecipeCard`, `patchRecipe`, recipe validation +
  `historyEntryLabel` cases), `api.mjs` (2 routes), `cli.mjs` (`recipe-create`,
  `recipe-set`), `site/inspector.js` (Recipe section), `site/workspace.js` (card
  badge + prompt-preview render), `site/actions.js` (2 actions), `README.md`
  (Operations + Recipe card section), `tests/ops.test.mjs` + `tests/api.test.mjs`.
- No store change (additive `group.recipe` round-trips via `updateProject`).
- Tests: create → group has `recipe`; patch prompt/params round-trips + journaled
  undo; drop image in = member (existing `assignToGroup`); invalid recipe = loud;
  `exportProject` skips recipe groups.

**Increment 2 — generate end-to-end + seam + placement.**
- Files: NEW `tools/recipe_generate.mjs` (pure builders + default `generateImage`),
  `ops.mjs` (`generateFromRecipe`, tool_runs row, `last_run`), `api.mjs` (generate
  route, folded `history`), `cli.mjs` (`recipe-generate`), `site/inspector.js`
  (Generate button via `runLongOp`), `site/actions.js` (viewport-center /
  beside-element placement), `README.md`, `tests/recipe_generate.test.mjs`
  (argv builder) + `tests/ops.test.mjs` (inject fake generator).
- Tests: new element minted OUTSIDE the card (scope = card.parentId), card
  unchanged, `meta.recipe` frozen snapshot present, `meta.alpha` absent (raw),
  tool_runs row, `last_run` set, one undo removes only the new element; >5 refs =
  loud; 0 refs OK; placement honored; fake generator so no codex.

**Increment 3 — Expand-prompt + Extract-style helpers.**
- Files: NEW `tools/prompt_assist.mjs` (pure instruction builders + default codex
  `expand`/`extractStyle`), `ops.mjs` (`expandRecipePrompt`, `extractStyle`),
  `api.mjs` (2 routes), `cli.mjs` (`recipe-expand`, `style-extract`),
  `site/inspector.js` (Expand button on card; Extract button on image element +
  clipboard copy), `site/actions.js`, `README.md`,
  `tests/prompt_assist.test.mjs` (instruction builders) + `tests/ops.test.mjs`
  (inject fake assistant).
- Tests: expand writes `recipe.expanded` (journaled); extract writes
  `element.meta.style` + returns `{ description, clipboard }`; re-extract overwrites
  (regenerate ability); non-image element = loud; unparseable JSON = loud; fake
  assistant so no codex.

## 10. Risks / dependencies (not open questions)

- **T0240 (backlogged):** `gen_rest` silently drops `--input-image`. Card refs reach
  the model only on the codex-backend credential path until fixed. Flag in the
  increment-2 packet.
- **VLM palette drift:** Extract's palette should be pixel-sampled, not VLM-guessed
  (research). v1 stores the described palette; pixel-sampling is an additive
  follow-up.
- **`codex exec` reliability:** the Avast TLS MITM env + codex's tendency to fake
  work make the text seam the riskiest spawn; keep it injectable (tests never spawn)
  and parse/verify outputs loudly.
- **No seed / determinism:** design nothing around a seed (research — gpt-image gives
  none usable). Reproducibility = cache the bytes via the existing `gen_hash`
  sidecar; consistency = sheet+slice / anchor-ref / verbatim style block / palette
  quantize.
- **`n_candidates`/`supersample`:** stored but advisory in v1 (fan-out = `gen_batch`
  is v2; supersample honored via `size`). Keep the fields so v2 is additive.

## 11. Open question for the lead (the only one that changes the design)

**Where does reusable STYLE live?** Decision 6 (fixed) puts the extracted style in
element META + CLIPBOARD. Every competitor in our product family instead uses a
**named preset in a library**, referenced by id — the only model that delivers the
cross-canvas / cross-game reuse the lead explicitly wants; META+CLIPBOARD are the two
weakest options for reuse (buried in one element; clipboard drops refs + palette).

Proposed reconciliation (does not reopen decision 6): **ship v1 exactly as decided
(meta + clipboard), reserve a nullable `recipe.style_ref` pointer now (additive, free),
and backlog the named-preset library** (`saveStylePreset` → per-game, optional global
`styles/<slug>.json`, referenced by id from the card) as a fast-follow. Decision to
confirm: add the library as a 4th increment in this feature, or backlog it as its own
task (T0208-adjacent)? Recommendation: **backlog it** — it keeps T0239 unblocked while
the reserved `style_ref` guarantees the library drops in without reworking the card.

---

## REVISION R1 (2026-07-03, lead discussion) — STYLE CARD replaces meta+clipboard

Decision 6 (extract -> meta + clipboard) and the "backlog a named preset library"
recommendation are SUPERSEDED by an explicit on-canvas STYLE CARD component
(lead: "смысл карточки стиля был в том что это явно"; "если мне нужна
библиотека, то я её сам сделаю" — a library is just a canvas of style cards).

Style card structure (lead's own formulation, accepted verbatim):
- name + STYLE PROMPT (editable text);
- exactly ONE REF image — the only image that is SENT to generation as a
  reference (image > text per the genart research);
- any number of EXAMPLE images — for eyes only, NEVER sent.

Ref marking (lead): a "ref" LABEL badge on the image inside the card + a
border/outline highlight; first image dropped into the card auto-becomes the
ref; a "Make ref" button in the inspector on any member switches it.

Recipe-card linkage: inspector dropdown "Style: <name>|none" over the style
cards of THIS canvas (recipe.style_ref = style card id, the reserved nullable
field). At generation time: style prompt is APPENDED to the recipe prompt, the
style ref image is attached as an input reference ALONGSIDE the recipe card's
own member refs. Examples never travel.

Extract-style now CREATES a style card beside the source image (source image
becomes the ref via a copied element, extracted text = the style prompt;
clipboard copy kept as a bonus). Cross-canvas reuse = node-spec copy/paste of
the card (buildNodesSpec must carry the recipe/style blobs — increment detail).

Engine: codex path only (REST path drops input images — T0240; agy/Gemini as a
possible second engine is explicitly OUT, separate task only if the lead asks).

Increment re-scope: (1) recipe card object + inspector; (2) generate end-to-end;
(3) STYLE CARD component (structure, ref badge+border, Make ref, dropdown
linkage, generation mixing); (4) Expand-prompt + Extract-style (extract mints a
style card). Still strictly sequential (shared ops.mjs/inspector.js).

## REVISION R2 (2026-07-03, lead) — engine choice codex | gemini

Lead: "хочу выбор, кодекс или гемини". The recipe card gains an ENGINE field:
recipe.engine = "codex" (default) | "gemini". Inspector dropdown on the card;
CLI --engine codex|gemini; one seam module (tools/recipe_generate.mjs) with two
pure command builders + two default generators:
- codex -> generate_image.py (refs supported — the T0238 precedent);
- gemini -> the agy wrapper (skill Path B: agy.exe headless prompt run, verify
  by file existence not stdout).
REF SUPPORT ON GEMINI MUST BE VERIFIED by the implementing worker (agy is an
agent CLI; whether it reliably consumes local image refs is unproven). Law: if
the gemini path cannot take refs, generating from a card WITH refs/style on
engine=gemini REFUSES loudly naming the limitation — never a silent text-only
generation (no second T0240).
Dual-plate generation (T0238/T0248) STAYS codex-only: the chain depends on
subject-locked EDITS; agy edit fidelity is unproven and gate costs are real.

## REVISION R3 (2026-07-03, lead) — engine "both" (compare mode)

Lead confirmed: engine choice is codex | gemini | BOTH. "both" mirrors the
skill's gen_both.sh compare mode: run the same recipe through BOTH engines,
mint TWO result elements side by side (named "<recipe> codex" / "<recipe>
agy"), each with its own frozen meta.recipe snapshot carrying `engine`; the
lead picks by eye. One journal entry for the pair (one undo removes both).
One engine failing = the other's result still lands + a loud status naming
the failed engine (partial success is real success here — compare mode is
best-effort by nature, unlike a single-engine run which stays loud-fail).
Refs caveat per engine unchanged (R2): if gemini can't take refs, a card
WITH refs on engine=gemini refuses; on engine=both it generates codex-only
and REPORTS the gemini skip loudly instead of failing the whole run.

## REVISION R4 (2026-07-03, lead) — extraction lives in image META; cards are promotions

Supersedes the increment-4 extract shape (both §6's meta+clipboard AND R1's
direct card-minting extract). Lead: "промпт и стиль это же мета? может там и
хранить извлечённые. Полный промпт, промпт без стиля, и стиль. Извлечённые из
картинки отдельно от ссылки на стиль." Confirmed: "да".

- ONE Extract action on an image = ONE codex vision call = ONE journal entry
  writing `element.meta.extracted = { prompt_full, prompt_subject, style:
  {style_block, palette, materials, lighting, composition, constraints_block},
  description, at }`. Re-extract overwrites. Never touches recipe.style_ref.
  - prompt_full: standalone (subject+style) — the "take it to another tool"
    text (lead: "точный промпт и уйти попробовать в другом месте").
  - prompt_subject: no style descriptors — composes with style cards.
- Inspector "Extracted" section: description + 3 rows (Full/Subject/Style),
  each View (read-only modal) + Copy; Re-extract.
- Card minting = separate cheap PROMOTION gestures (no codex): "→ Recipe card"
  (below the image, prompt_subject + image copy as member/ref) and "→ Style
  card" (right of the image, style_block(+constraints), copy as style.ref).
  Each one entry; promoting twice mints two independent cards.
- Expanded flow (same session decisions): "Send expanded" checkbox STAYS,
  default true (fresh card sends expanded once it exists); Discard button
  clears `expanded` (null) to revert to the short prompt; patch surface
  accepts expanded + use_expanded.
