# Research: reusable STYLE and reusable PROMPT as first-class objects — competitor models + our design

Date: 2026-07-03. Lead's question: "стиль и описание — это будут новые ТИПЫ
[объектов на канвасе]? чтобы я мог их переиспользовать? Как делают конкуренты?"
i.e. should a reusable STYLE and a reusable PROMPT/DESCRIPTION be first-class
objects in our Figma-like AI canvas, and how do the competitors model exactly
this.

Builds on the sibling report `tmp/research_genart_workflow_2026-07-03.md` (§4/§7:
style-extraction schema + style-lock verdict). That report already settled WHAT a
style preset must contain (style_block + refs + pixel-sampled palette +
constraints). THIS report answers the orthogonal UX/data-model question the lead
actually asked: WHERE that reusable thing lives — canvas object vs side-panel
library vs element meta vs text — and what competitors do.

Our stack constraints that gate the answer: local Figma-like canvas (FLAT additive
element model), generation via gpt-image through Codex CLI, refs = input images,
**NO LoRA / no fine-tuning**, hard laws = tool parity (one op layer, two equal
clients: agent CLI + site page) and lean process.

---

## Part 1 — How competitors represent REUSABLE STYLE (per-product, with URLs)

The dominant pattern is identical across every mainstream product: **a style is a
NAMED, reusable LIBRARY entity created from reference images (+ optional embedded
style-prompt), stored in a side panel / "My Styles" gallery, and ATTACHED
per-generation by selection or set as a project default.** It is NOT a free-floating
object you drop next to your artwork. Details and the exceptions:

### Recraft — the closest product to us; the reference implementation
- A **"custom style" is a first-class saved entity**: you upload **up to 5
  reference images** (drag/drop), optionally add a **style-level prompt** that gets
  "embedded in the style" so later per-generation prompts can be shorter, and pick
  an interpretation mode ("Style essentials" = aesthetic only, "Style and
  composition" = aesthetic + structure). You can weight references with sliders.
- The saved style lives in a **library/panel** and is applied to any future
  generation; **Style Sharing** publishes a style so a whole team generates against
  the same identical style entity. Brand-kit framing locks palette + illustration
  style + line weight across generations.
- This is exactly option (b) below, and it is the product built for our use case.
- https://www.recraft.ai/docs/recraft-studio/styles/custom-styles/how-to-create-a-custom-style
- https://www.recraft.ai/blog/introducing-style-sharing-maintain-design-consistency-across-teams
- https://www.recraft.ai/blog/new-tools-for-brand-style-consistency-and-control

### Midjourney — style as a REFERENCE or a CODE, not an object
- `--sref <image|code>`: apply a style either from an uploaded reference image OR
  from a **numeric style code** drawn from MJ's internal library. `--sref random`
  rolls a random code you can then **save and paste to lock that aesthetic** across
  a campaign. Two srefs can be blended.
- **Moodboards**: a **named, saved collection of reference images**; each moodboard
  has a **unique ID/code** you drop into prompts. This is the canvas-adjacent idea —
  a persistent named bag of refs — but it lives on a Moodboards *page* (a library),
  referenced by code, not as an object on your working surface.
- **Personalization profiles**: a saved `--profile` code trained on your likes.
- Takeaway pattern for us: **a style can be a short stable ID you save/paste/reuse.**
  We can't mint MJ codes, but "reusable style = named id" is the transferable idea.
- https://docs.midjourney.com/hc/en-us/articles/32180011136653-Style-Reference
- https://docs.midjourney.com/hc/en-us/articles/39193335040013-Moodboards
- https://docs.midjourney.com/hc/en-us/articles/32433330574221-Personalization

### Ideogram — presets + style-reference + 4.3B saved codes
- Three tiers in one panel: **preset styles**, **style-reference** (upload up to 3
  images → extracts a style signature: palette/lighting/texture/composition/mood),
  and **random → a stable Style Code** auto-attached to each result that you
  **save and reuse** with new prompts to hold an aesthetic across assets. Same
  "style = reusable code/entity in a settings panel" model.
- https://docs.ideogram.ai/using-ideogram/ideogram-features/style
- https://docs.ideogram.ai/using-ideogram/features-and-tools/reference-features/style-reference
- https://www.testingcatalog.com/new-ideogram-styles-feature-brings-reusable-aesthetics-for-creators/

### Adobe Firefly — Style Kits + Style Reference
- **Style Reference**: upload/choose an image to steer look-and-feel per generation
  (transient control, not saved).
- **Style Kits**: generate an image you like, then **"Save as style kit"** — this
  freezes prompt + content/structure/style settings into a **named, saved file**
  in your Files tab, shareable to teammates who reuse the approved style. Again: a
  named library entity, applied per generation; enterprise-gated.
- https://helpx.adobe.com/firefly/web/generate-images-with-text-to-image/customize-generated-images/reference-images-for-styling.html
- https://helpx.adobe.com/firefly/web/work-with-enterprise-features/collaborate-using-style-kits/style-kits-overview.html

### Freepik AI suite — closest structural twin to the recommendation
- **New Style**: sidebar → "New Style" → give it a **name + description**, define
  it **by prompt, by one reference image, or by several images** → **saved,
  reusable, and adaptable across projects**, reducing repeated prompt text. Exactly
  the "named preset created from refs, reused across projects" model.
- Per-generation, styles combine with other **references** (Style + Character +
  Color palette) added in a References section below the prompt.
- **Freepik Spaces** = their infinite-canvas / node product (see Part 2).
- https://www.freepik.com/ai/docs/freepik-styles-and-custom-styles
- https://www.freepik.com/ai/docs/style

### Krea — moodboards (ref-based, transferable) + trained models (LoRA, NOT)
- **Moodboards**: upload reference images → model extracts a taste profile
  (palette/lighting/texture/composition); **"set up the style once, reuse the ID
  across any number of requests."** Ref-based, transferable to us. Up to 10 refs.
- **Custom model training**: train a Krea LoRA from 3–30 images, reuse with strength
  control + multi-LoRA stacking. This is the **trained-model class — NOT transferable
  to us** (no training path), note and skip.
- https://www.mindstudio.ai/blog/krea-2-mood-boards-visual-style-ai-image-generation
- https://www.krea.ai/docs/features/training

### Leonardo — Elements (LoRA) + Style Reference
- **Elements** = Leonardo's LoRA training wrapped in a friendly flow: "capture a
  look/subject/style, reuse across every generation." Reusable, but **trained-model
  class = NOT transferable to us.** Their **Style Reference** (image-guidance) is the
  transferable, ref-based control.
- https://leonardo.ai/element-training
- https://leonardo.ai/learn/core-feature/how-to-use-leonardo-ais-style-reference-feature

### Scenario / Layer / SpriteFlow — style = a trained model
- Entire product premise is "train your art style once" (LoRA/fine-tune). Reusable
  and game-focused, but **structurally off-limits for us** — we have no training.
  UX framing worth noting: they still surface the trained style as a **named entity
  in a library/dropdown** picked per generation — i.e. even the training products
  present style as a library item, never as a canvas object. (Covered in the sibling
  report §Q3; not re-researched here.)

**Trained-model vs reference-based — the class split that decides transferability:**
Leonardo Elements, Krea custom models, Scenario, Layer, SpriteFlow all encode a
reusable style as a *trained LoRA* — powerful but **impossible on our stack**.
Recraft custom styles, Freepik styles, Ideogram/Firefly/MJ style-reference &
moodboards & codes encode it as **refs + optional style-prompt (+ palette)** — this
is OUR class, and it maps 1:1 onto the sibling report's style-preset schema.

---

## Part 2 — Is STYLE/PROMPT ever an OBJECT ON THE CANVAS? (the lead's core question)

Two distinct product families, and the answer differs by family:

**A. Board / flat-canvas / prompt-bar tools** (Midjourney web, Recraft, Freepik AI
Image Generator, Leonardo, Ideogram, Firefly, Krea Image tool, **Playground
Board/Canvas**): style is a **side-panel settings entity**, prompt is typed in a
**prompt bar**, results land on the board. In Playground's Canvas, generation
settings apply to the **selected frame** via a panel — style/prompt are frame
settings, **never standalone canvas objects**. This is the family WE are in
(flat Figma-like canvas). In this entire family, **nobody puts style or prompt as a
free-floating object on the canvas.**
- https://www.toolify.ai/ai-news/mastering-playground-ai-filters-88811
- https://artsmart.ai/docs/playground-a-canvas-for-ai-driven-artistic-expression/

**B. Node-graph canvas tools** (Krea Nodes, Freepik Spaces, Flora, Weavy): here a
**prompt IS a node** ("text prompt node") and style CAN be a node (a **Style
Extraction node** that analyzes a reference image, a **LoRA node** to enforce a
style, an **image-reference node**), wired into generation nodes on the canvas.
So *yes*, in node graphs, prompt/style are objects on the canvas.
- BUT two caveats gut this as a model for us: (1) These are **node graphs, not flat
  canvases** — the object only has meaning because edges wire it into a pipeline; we
  are explicitly a flat additive element model, not a DAG. (2) Even here, reusable
  style across projects is backed by a **library/model**: Flora ships "Flows =
  pre-configured node templates," Freepik Spaces pulls from the **stock/style
  library**, Weavy "App Mode" publishes a graph for reuse, and Krea's Node Agent
  "reuses the style node from a previous session." The canvas object is the
  *instance*; the reusable source of truth is still a library/model. Krea's own docs
  are explicit that prompt text is a **parameter inside a node**, not an independent
  reusable object.
- https://chasejarvis.com/blog/best-generative-ai-canvas-apps/
- https://www.krea.ai/docs/user-guide/features/nodes
- https://krea.ai/articles/freepik-spaces-vs-krea-nodes

**Plain verdict for the lead:** style-as-a-canvas-object exists ONLY in node-graph
tools, and even there it is really "a node wired into a pipeline, backed by a
library." In flat-canvas / board tools like ours, **style is universally a
side-panel LIBRARY entity, never an object dropped next to the artwork.** The
competitor consensus is overwhelmingly the library model.

---

## Part 3 — How competitors represent REUSABLE PROMPT / DESCRIPTION

Much thinner than style — there is **no strong industry pattern of a separate
first-class "saved prompt" object** inside image tools. Three real patterns:
1. **Prompt folded INTO the style** (Recraft style-level prompt; Freepik "style by
   prompt"): the durable reusable text is embedded in the style entity, so per-gen
   prompts get shorter. The reusable prompt = part of the style, not its own object.
2. **Prompt persists WITH the result** (Midjourney, Ideogram): every generated image
   carries its exact prompt; you **remix/reroll** from the result. The "saved prompt"
   is just the history of generations — no separate library needed.
3. **Template bundles** (Freepik templates = "preset styles, prompts, reference
   combinations"; dedicated prompt-managers like QuestStudio/PrompTessor with
   `{{variable}}` slots + folders + versioning). Rich named-template-with-variables
   libraries exist **only in niche prompt-manager tools**, not in the core image
   products. Freepik's "template" is essentially a saved gen recipe = a bundle of
   prompt+style+refs — i.e. **exactly what our gen card already is.**
- https://queststudio.io/blog/best-ai-prompt-generator-2026
- https://www.freepik.com/ai/docs (templates; §Part 1 Freepik links)

**Takeaway:** the reusable-prompt problem is mostly solved by (a) baking the durable
part into the STYLE and (b) letting the generation recipe persist. A separate
saved-prompt-with-variables library is a niche power feature, not table stakes.

---

## Part 4 — The spectrum for OUR design (answering the lead directly)

### STYLE — four options
| Option | Reuse ACROSS canvases/games | Discoverability | Clutter | CLI parity | Migration from v1 |
|---|---|---|---|---|---|
| **(a) style in element meta** (v1 hypothesis) | **Poor** — buried in one element on one canvas; can't find it from another game | Poor | None | CLI must dig into element meta | it IS v1 |
| **(b) named PRESET in a library store** (Recraft/Freepik consensus) | **Excellent** — pick by name on any canvas/game | Excellent (named gallery) | None (off-canvas) | **Clean** — one op layer lists/creates/applies presets by id; panel + CLI equal | Low — lift meta blob into a store record keyed by id |
| **(c) style = CANVAS OBJECT** (node-tool style) | **Poor** — a canvas object lives on ONE canvas; needs a library anyway to instantiate elsewhere | Medium (visible only on that canvas) | Adds a persistent card to every board | Awkward — CLI must place + wire objects | New element type |
| **(d) plain text block, copy-paste** (v1 clipboard) | Manual + **lossy** (drops refs + sampled palette) | None | None | Trivial but weak | it IS v1 |

Only **(b)** delivers the lead's stated goal ("чтобы я мог их переиспользовать")
across canvases and games, and it is what every competitor in our family does.
(a) and (d) — which are **exactly what T0239 v1 currently specifies** — are the two
worst for reuse. (c) matches only node tools, contradicts our flat model, and still
needs a library underneath, so it buys nothing v1.

### PROMPT / DESCRIPTION — the gen card already IS the saved prompt
Per §Part 3, the industry does **not** build a separate saved-prompt library for the
image case; the reusable prompt is either embedded in the style or persisted with the
generation. **Our genspec card already persists prompt + refs on the canvas by
design — it is the saved recipe (Freepik "template" equivalent).** So: no separate
prompt-library object for v1. Cross-canvas reuse of a recipe = duplicate the card (a
plain element op). The durable, cross-project part of the prompt (the style clauses)
lives in the STYLE preset, not the card — matching Recraft's "embed the style-prompt,
keep per-gen prompts short."

---

## Part 5 — RECOMMENDATION

**v1 — STYLE = a named PRESET record in a small library store (option b). PROMPT
reuse = the genspec card itself (no separate object).**

Grounding: (1) competitor consensus in our product family is unanimously the library
model; (2) it is the only option that gives cross-canvas/cross-game reuse, which is
the lead's explicit ask; (3) it maps cleanly onto **tool parity** — the preset store
is one op layer that the site panel and the agent CLI drive identically; (4) it keeps
the **flat additive element model clean** (no new element type, no canvas clutter);
(5) migration from the v1 element-meta hypothesis is cheap (lift the blob into a
keyed record). It also honors "everything through a canvas" without making style an
element: the canvas is where you **create** a preset (Extract-style on an image
element) and where you **consume** it (a gen card references it) — the canvas is the
workshop, the library is the shelf.

Reconcile with the lead's canvas instinct: the reusable artifact should NOT be a
canvas element in v1, because a canvas element lives on one canvas and fails the
reuse test. Make it a library record; the canvas keeps the create/consume verbs.

### Data-model sketch (chosen option)

**Where stored:** a preset store as JSON files, sibling to the canvas element store,
NOT inside the flat element list. Two scopes: **per-game**
`games/<id>/design/styles/<preset-id>.json` (default), and optional **global**
`ai_studio/.../styles/` for cross-game reuse (the "reuse across games" the lead
wants). Refs stored as file paths into the game's asset/canvas files (not inlined
bytes), `<=5` per the cap.

**Record** (reuse the sibling report §7 schema verbatim — do not invent a new one):
```json
{
  "id": "hand-painted-card-art",           // stable slug = the "style code"
  "name": "hand-painted card art",
  "scope": "game:little-lives | global",
  "style_block": "<the [STYLE] segment injected into prompts>",
  "constraints_block": "<the [CONSTRAINTS] segment>",
  "palette": { "swatches": ["#c8631e", ...] },   // SAMPLED from pixels, not VLM-guessed
  "refs": ["canvas://.../el-123", "files/<exemplar>.png"],  // <=5 anchor images
  "medium": "...", "line": "...", "shading": "...", "proportions": "...",
  "source_element": "canvas://<canvas>/<image-el-it-was-extracted-from>",
  "created": "2026-07-03", "version": 1
}
```

**How a gen card references it:** the `genspec` element carries `style_ref:
"<preset-id>"` (a by-id pointer, not a copy). "Expand prompt" resolves the id →
injects `style_block` + `constraints_block` into the labeled template and
auto-attaches the preset's `refs` as `--input-image` (on top of the card's own
dropped refs). **Freeze on run:** each run snapshots the resolved style_block + refs
into the run record (the `gen_hash` inputs from the sibling report) so an old run
still reproduces after the preset is later edited/versioned.

**CLI surface (tool parity — identical ops behind panel + CLI):**
```
canvas style extract <image-el> --name "..." [--scope game|global]   # mints preset from an image
canvas style list [--scope ...]                                       # the library, for humans + agent
canvas style show <preset-id>
canvas style apply <preset-id> --to <genspec-el>                      # set style_ref
canvas style export <preset-id> [--clipboard]                        # option (d) kept as convenience only
```
The site "Extract style" button and "style picker" on a gen card call the *same* ops.
This is the parity law satisfied by construction: one op layer, two equal clients.

### What to leave for v2
- **Style CANVAS-CHIP affordance** (option c, done right): a lightweight visual token
  (a chip, not a full element type) you can drop on a board and drag onto a gen card
  to set `style_ref` — the canvas representation the lead wants, but as a *view of a
  library record*, not the source of truth. Add only after the library proves out.
  This is the MJ-moodboard / sref-code idea made visible.
- **Style versioning + short shareable id** (MJ/Ideogram "code" ergonomics), a
  **project/canvas default style**, and **brand-kit grouping** (Recraft/Firefly
  Style Kit = a named bundle of style + palette + constraints).
- **Prompt template with a `{{subject}}` slot** — ONLY if real friction appears; the
  card + expander + embedded style already cover it, so treat as YAGNI for now.
- Recraft "interpretation mode" (style-only vs style+composition) and per-ref weight
  sliders — nice controls once the basic preset ships.

---

## 8-line summary (return to orchestrator)
1. Competitor consensus is overwhelming: in our product family (flat-canvas / board /
   prompt-bar tools — Recraft, Freepik, Ideogram, Firefly, Leonardo, Midjourney web,
   Playground) a reusable STYLE is a **named side-panel LIBRARY entity**, created from
   refs (+ optional embedded style-prompt), applied per generation. Nobody makes it a
   free-floating canvas object.
2. Style-as-canvas-object exists ONLY in node-graph tools (Krea Nodes, Freepik Spaces,
   Flora, Weavy) — and even there it's a node wired into a pipeline, still backed by a
   library/model; irrelevant to our flat additive model.
3. Two style classes: **reference-based** (Recraft/Freepik/Ideogram/Firefly/MJ/Krea
   moodboards = OUR class) vs **trained-model/LoRA** (Leonardo Elements, Krea models,
   Scenario, Layer, SpriteFlow = impossible on our no-training stack — note and skip).
4. **Recommendation — STYLE:** make it a **named PRESET record in a small library
   store** (option b, the Recraft/Freepik model), referenced by id from the gen card,
   created via Extract-style on a canvas image, driven by one op layer (CLI + panel).
   NOT a new canvas element type in v1.
5. **Recommendation — PROMPT reuse:** no separate saved-prompt object. The **genspec
   card already IS the persisted recipe**; the durable cross-project text lives in the
   STYLE preset (Recraft "embed style-prompt, shorten per-gen prompt"). Reuse a recipe
   across canvases = duplicate the card.
6. **The one thing T0239 must change:** its current v1 line "style = element meta +
   re-extract + copy-to-clipboard" is options (a)+(d) — the two WORST for reuse
   (buried in one element; clipboard drops refs + palette). Replace with **style = a
   named preset in a per-game (optionally global) library store, referenced by id**;
   keep copy-to-clipboard only as an export convenience, not the reuse mechanism.
7. Data model + CLI: reuse the sibling report's style schema as the record; add
   `style_ref: <preset-id>` on the genspec element; freeze the resolved style into each
   run's `gen_hash` snapshot so old runs reproduce after a preset edits.
8. v2 (only after the library proves out): a draggable style **chip** on the canvas
   (the lead's canvas instinct, done as a view of a library record), style versioning /
   short shareable id, project-default style, and brand-kit grouping.

---

## Sources
- Recraft custom styles: https://www.recraft.ai/docs/recraft-studio/styles/custom-styles/how-to-create-a-custom-style
- Recraft style sharing: https://www.recraft.ai/blog/introducing-style-sharing-maintain-design-consistency-across-teams
- Recraft brand style tools: https://www.recraft.ai/blog/new-tools-for-brand-style-consistency-and-control
- Midjourney Style Reference (sref / codes): https://docs.midjourney.com/hc/en-us/articles/32180011136653-Style-Reference
- Midjourney Moodboards: https://docs.midjourney.com/hc/en-us/articles/39193335040013-Moodboards
- Midjourney Personalization: https://docs.midjourney.com/hc/en-us/articles/32433330574221-Personalization
- Ideogram Style: https://docs.ideogram.ai/using-ideogram/ideogram-features/style
- Ideogram Style Reference: https://docs.ideogram.ai/using-ideogram/features-and-tools/reference-features/style-reference
- Ideogram styles (presets/codes/refs) writeup: https://www.testingcatalog.com/new-ideogram-styles-feature-brings-reusable-aesthetics-for-creators/
- Adobe Firefly Style Kits overview: https://helpx.adobe.com/firefly/web/work-with-enterprise-features/collaborate-using-style-kits/style-kits-overview.html
- Adobe Firefly style reference images: https://helpx.adobe.com/firefly/web/generate-images-with-text-to-image/customize-generated-images/reference-images-for-styling.html
- Freepik custom styles: https://www.freepik.com/ai/docs/freepik-styles-and-custom-styles
- Freepik style: https://www.freepik.com/ai/docs/style
- Krea moodboards: https://www.mindstudio.ai/blog/krea-2-mood-boards-visual-style-ai-image-generation
- Krea training (LoRA): https://www.krea.ai/docs/features/training
- Krea Nodes docs: https://www.krea.ai/docs/user-guide/features/nodes
- Freepik Spaces vs Krea Nodes: https://krea.ai/articles/freepik-spaces-vs-krea-nodes
- Leonardo Element training (LoRA): https://leonardo.ai/element-training
- Leonardo style reference: https://leonardo.ai/learn/core-feature/how-to-use-leonardo-ais-style-reference-feature
- Best generative AI canvas apps (Weavy/Flora/Freepik Spaces, node prompt/style objects): https://chasejarvis.com/blog/best-generative-ai-canvas-apps/
- Node-based image tools survey: https://www.wireflow.ai/blog/best-node-based-image-generation-tools-in-2026
- Playground filters/board/canvas: https://www.toolify.ai/ai-news/mastering-playground-ai-filters-88811 , https://artsmart.ai/docs/playground-a-canvas-for-ai-driven-artistic-expression/
- Prompt templates / variables / saved-prompt managers: https://queststudio.io/blog/best-ai-prompt-generator-2026
- Sibling report (style schema + style-lock verdict): tmp/research_genart_workflow_2026-07-03.md
</content>
</invoke>
