# Research: how image editors present multiple independent destructive cleanup/filter ops with live preview

Date: 2026-07-04
Author: deep-reasoner (UX evidence, not a UI design)
Scope: evidence inventory for our "Cleanup" inspector section (Quantize + Denoise, one shared preview slot, shared Reset/Apply, sequential-apply accepted by lead).

---

## 0. Our situation, restated (the thing we are comparing against)

Two INDEPENDENT destructive pixel ops on one selected image:
- **Quantize** (reduce palette to N colors, slider)
- **Denoise** (median filter, strength 0|1|2|3)

Each has a live debounced on-canvas preview, but there is **ONE shared preview slot** (previewing B silently replaces A's preview), a **shared Reset/Apply row**, and Apply = one journal write (Ctrl+Z reverts).

Lead verdict (verbatim RU):
- «кажется что это про одно и что это настройки» — reads as ONE process with settings, not two procedures.
- «Мне подходит вариант по очереди применять, но тогда нужно либо не давать менять шум пока есть квантование и наоборот. Или давать но явно говорить что было сброшено состояние» — sequential-apply is fine, but either LOCK the other tool while one is uncommitted, or ALLOW it but EXPLICITLY say the previous state was reset.
- «К работе нет претензий, ui ux неудачный» — the work is correct; the UI/UX is the failure.

So the two sins to fix are (1) the two tools look like settings of one process, and (2) preview swapping is silent. The research below is aimed exactly at those two.

---

## 1. Pattern inventory (product × the four questions)

For each product: **(a)** how it signals "preview, not yet applied"; **(b)** how it separates independent operations; **(c)** what happens when you switch to another op while one is uncommitted; **(d)** commit/cancel affordances + wording.

| Product | (a) preview-vs-applied signal | (b) separation of independent ops | (c) switching while uncommitted | (d) commit / cancel wording |
|---|---|---|---|---|
| **Photoshop** (classic filter dialog) | Live preview drawn on canvas + inside dialog thumbnail; a **Preview checkbox** you toggle on/off to A/B before vs after. Nothing is written to pixels until commit. | **Modal, one filter at a time.** Each filter is a separate menu entry (Filter ▸ Noise ▸ Median, etc.); its own dialog owns the screen. | You physically **cannot** start another filter — the modal blocks the rest of the app until you resolve it. Serialization is enforced by modality. | **OK** (commit, writes pixels / bakes into Smart Filter) / **Cancel** / **Esc**. Alt-drag sliders for a faster live preview. |
| **Photopea** (web Photoshop clone) | Same as Photoshop: modal filter dialog with live canvas preview; commit writes pixels. | Modal, per-filter menu entries; mirrors Photoshop 1:1. | Modal blocks; one at a time. | **OK / Cancel** buttons in the dialog header. |
| **Aseprite** (closest domain: pixel/game art) | **Despeckle** (median) shows a modal dialog (`ui=true`) with live on-canvas preview; matrix width/height (default 3×3) + channels. **Color Mode ▸ More Options** (RGB→Indexed / quantize) shows a modal with dithering dropdown + factor and a live preview before commit. | **Separate menu commands.** Despeckle lives under a filter menu; quantization lives under **Sprite ▸ Color Mode**. They are never presented as one "cleanup" panel. Palette count is surfaced elsewhere (the Palette panel shows the color list; indexed mode caps at 256). | Each is its own modal command; you finish one, then invoke the next. No shared preview slot to collide. | **OK / Cancel**. Dialog is small and docked-modal, not a wizard. |
| **Krita** | Menubar **Filter dialog** has an explicit **Preview checkbox**; live on-canvas preview; not applied until OK. Non-destructive **Filter Layer / Filter Mask** has *no* preview checkbox because it never bakes — it just shows live. | Two clearly different affordances: (1) destructive one-shot **Filter dialog** (per-filter, from a categorized filter list on the left of the dialog); (2) non-destructive **Filter Mask/Layer** in the layer stack. A **"Create Filter Mask"** button inside the dialog bridges the two. | Filter dialog is **modal** — you can't even hide the layer/mask to compare while it's open; you resolve it first. | **OK / Cancel**; plus "Create Filter Mask" to promote to non-destructive instead of baking. |
| **GIMP 2.10+** | On-canvas live preview (default on) via a **Preview checkbox**; "changes are directly displayed on canvas … not applied until you click OK." Optional **Split view** draws a before/after divider you can drag. | Modal, per-filter menu entries. Shared *common* dialog chrome across all filters: **Presets, Input type (selection/layer), Clipping, Blending options, Preview, Split view**. | Modal blocks other filters; one at a time. Blending options let you tune the *just-applied* filter, but only for the current op. | **OK / Reset / Cancel**. Reset = restore this dialog's defaults (not undo pixels); Cancel = discard preview; OK = commit. |
| **paint.net** | Effect dialogs render a live preview directly on the canvas as you drag sliders/handles; commit writes pixels. | Modal, per-effect **Effects menu** entries (Effects ▸ Noise ▸ Median, etc.). One effect owns the dialog. | Modal blocks; serialize by finishing the dialog. | **OK / Cancel**. Separate app-level "unsaved changes" dialog exists for the document, distinct from per-effect commit. |
| **Affinity Photo** | Two explicit tiers. **Destructive filter** = modal-ish dialog, live preview, **Apply** bakes pixels irreversibly ("once applied cannot be changed"). **Live Filter layer** = filter placed on its own layer in the stack, always-on live, re-editable forever, maskable, opacity/blend. | Destructive filters vs **Live Filter layers** are two different menus; live filters compose in the **layer stack** (multiple stack independently). | Destructive dialog serializes (commit/cancel). Live filters just add another layer — they compose, they don't collide. | Destructive: **Apply / Cancel** ("confirm by clicking Apply … destructive"). Live: close the dialog to keep the live layer; delete the layer to revert. |
| **Lightroom / Camera Raw** (the OPPOSITE model) | **No commit at all.** Parametric, non-destructive: every adjustment is stored as an instruction; the preview *is* the composed result of the whole stack, recomputed live. Original pixels never change. | A **persistent panel stack** (Basic, Detail/Denoise, etc.); all panels coexist and **compose** into one pipeline. In modern LrC you can even reorder panels, but the **engine order is fixed** regardless of slider order. | There is nothing to "switch away from" — all controls are simultaneously live and additive. No mutual exclusion, no discard-on-switch. | No OK/Apply per op. Reset per-panel or global; export bakes. |
| **Canva / Recraft** (web one-click enhance / magic eraser) | One-click actions show a processing state then swap the result in place; some offer a before/after or an "undo". Minimal per-op settings. | Each action is a **separate button/tool** ("Enhance", "Magic Eraser", "Upscale") — never bundled as tabs of one panel. Object-level cleanup uses a brush + a single **apply/render** action. | One action at a time; the result replaces the image, undo reverts. No shared-preview-slot ambiguity because there's essentially no settings surface to collide. | Big single **primary action** button ("Enhance" / "Erase"); undo for revert. Wording is verb-first and action-scoped. |

Sources: Photoshop/Elements filter + Smart Filter docs; GIMP 2.10 common-features + Apply-Canvas docs; Aseprite Despeckle API + Color-mode docs; Krita Filters Dialog wiki + Krita manual (filter masks/layers); paint.net Effects menu docs; Affinity Photo "Applying filters" + Live Filter Layers help; Adobe Lightroom Classic Develop-module help; Canva/Recraft feature pages; NN/g "Cancel vs Close"; Baymard/Onething accordion-vs-tabs. (URLs at bottom.)

---

## 2. The three dominant industry patterns (with the modal-per-filter vs settings-stack dichotomy addressed head-on)

### Pattern A — **Modal-per-filter (preview → commit → cancel)**  ← the mainstream for destructive one-shot ops
Photoshop, Photopea, Aseprite, Krita (dialog), GIMP, paint.net all converge here. Properties:
- **Each destructive op is its own named, self-contained surface** (menu entry → dialog). It is never confusable with "settings of a bigger thing," because it has its own title, its own preview, and its own commit button.
- **Preview-vs-applied is unambiguous** because commit is an explicit, discrete act (OK/Apply) and there is a matching escape (Cancel/Esc). Many add a **Preview checkbox** so you can A/B before vs after in place.
- **Mutual exclusion is enforced by modality, for free.** You literally cannot start op B while op A's dialog is open. There is never a "silent swap" of one preview for another, because there is never more than one uncommitted preview alive.
- Cost: modality is heavy/interruptive, and it doesn't match an always-visible inspector panel.

### Pattern B — **Non-destructive settings-stack (parametric, everything composes)**  ← Lightroom/Camera Raw; Affinity/Krita live-filter layers
- **No commit, no preview-vs-applied distinction at all** — the preview *is* the live composition of every adjustment. Order-independent at the UI (fixed engine order).
- Operations don't "collide" because they **coexist and add up**; there is no single shared slot.
- **Why it does NOT fit us:** it presupposes the backend keeps every op re-editable forever and re-composes them on demand. Our ops are **destructive one-shots with a single shared preview slot and one journal write** — deliberately. Bolting a Lightroom stack on top would contradict "single preview slot is a deliberate backend simplification" and the tool-parity law (UI is a thin client over ops; the ops layer is one-shot destructive, not a live parametric graph). It also re-introduces exactly the "these are just settings of one thing" reading the lead rejected — Lightroom sliders ARE settings of one composite. So Pattern B is a useful contrast, not a target.

### Pattern C — **Two-tier hybrid: destructive Apply vs promote-to-live**  ← Affinity, Krita's "Create Filter Mask"
- The app offers the SAME filter as either a destructive **Apply** (bakes now) or a non-destructive **layer/mask** (stays editable). Krita even puts a **"Create Filter Mask"** button *inside* the destructive dialog.
- Takeaway for us: even the pros treat "destructive one-shot" and "persistent composable" as **two visibly different things with different affordances and different commit wording** — they never let one masquerade as the other. That is precisely the lead's complaint.

### The dichotomy, resolved for our case
Our accepted model (per the lead) is **sequential destructive apply** = squarely **Pattern A's mental model** (discrete op → discrete commit), just hosted in an inspector instead of a modal. The industry's #1 way to make "these are two separate procedures, not one settings blob" legible is **one named op = one self-contained block = one preview = one commit, and only one uncommitted preview alive at a time.** Modality is just the cheap way most tools *achieve* the "only one uncommitted preview" rule; we can achieve the same rule in a panel without going modal.

---

## 3. Cross-cutting UX conventions worth borrowing (from the literature + the products)

1. **Name the operation, verb-first, at the block level.** Every product labels the op ("Median", "Despeckle", "Reduce Noise", "Color Mode") — the label is what stops it reading as "settings". Our two tools need to read as two named procedures, each with its own header, not two sliders under one "Cleanup".
2. **Commit is a discrete, explicit act with a matching cancel.** OK/Apply + Cancel/Esc is universal. NN/g's "Cancel vs Close": don't conflate discarding a preview with closing a panel; use text labels, not just an ✕.
3. **Only ONE uncommitted preview may be alive at once.** Modal apps get this for free. In a non-modal panel you must *enforce* it — which maps directly to the lead's two accepted options: **(i) lock/disable the other op while one is uncommitted**, or **(ii) allow the switch but explicitly announce "previous preview was reset/discarded."** Both are legitimate; the sin today is doing (ii)'s action *silently*.
4. **Single-open accordion = the panel-native way to express mutual exclusion.** Baymard/Onething: a single-expansion accordion "mimics tabs' mutually exclusive behavior while maintaining vertical stacking." That is the inspector-friendly analogue of "one modal at a time" — opening op B's section can visibly collapse/relinquish op A's preview, making the swap *seen* rather than silent.
5. **Uncommitted-state signaling should be visible, not implicit.** Oracle ADF / general dirty-state guidance: warn before losing uncommitted work; show a "modified/unapplied" marker. Our fix for the "silent swap" is exactly this: a visible "unapplied preview" state per op + an explicit reset notice when it's dropped.
6. **A Preview toggle (A/B) is a cheap legibility win** (Photoshop, Krita, GIMP). It reinforces "what you see is a preview, not the file yet."

---

## 4. Mapping to OUR constraints (which patterns fit, which don't)

Our constraints: inspector panel, **not** modal-happy today; **single preview slot** is a deliberate backend simplification (keep it); **sequential-apply accepted**; **tool-parity law** (UI = thin client over the ops layer; two equal clients: agent CLI + site page).

- **Adopt Pattern A's *semantics*, drop its *modality*.** Keep discrete op → discrete commit, but host it in the inspector. Each tool = its own **named block** (header = the op name), its own preview, its own Apply. This directly kills sin #1 ("looks like one process with settings"): two headers, two procedures, self-evidently two things.
- **Single preview slot survives cleanly IF mutual exclusion is made visible.** The single slot is fine — it *is* the "only one uncommitted preview alive" rule that modality enforces elsewhere. The failure is that we transition between owners silently. Two industry-blessed fixes, both pre-approved by the lead:
  - **Lock:** while op A has an unapplied preview, op B's controls are disabled (single-open accordion collapses/greys B). Cheapest, zero data loss, matches modality's guarantee.
  - **Announce:** allow the switch but show an explicit, non-silent "op A preview was reset" signal (dirty-state marker clears + inline notice). Matches the lead's «явно говорить что было сброшено состояние».
- **Reject Pattern B (Lightroom stack) for these ops.** It contradicts the single-slot/one-shot backend and re-creates the "just settings of one thing" reading. Keep it only as the contrast that explains *why* we are NOT a settings stack. (If a future non-destructive re-editable pipeline is ever wanted, that's a different backend and a different task — not this fix.)
- **Pattern C is the conceptual anchor for wording:** treat each op as a destructive bake with an explicit **Apply** that is clearly distinct from a live/settings adjustment. Verb-first labels ("Apply Quantize" / "Apply Denoise") over a generic shared "Apply" reduce the one-blob reading.
- **Tool-parity check:** all of the above is presentation-layer only. Locking, dirty-state, and per-op Apply map to the *same* underlying one-shot ops the agent CLI already calls (preview(op, params) → apply(op)); no new op semantics, no divergence between the two clients. The single shared preview slot is exactly the CLI's model too.

**Not our job to pick the final layout** (blocks vs single-open accordion vs per-op Apply buttons) — that's the design agent. The evidence says any of them works *provided* the two invariants below hold.

---

## 5. Single most load-bearing takeaway

**Every mainstream editor makes "two independent destructive ops" legible by giving each op its own named, self-contained block with its own explicit commit, and by guaranteeing only ONE uncommitted preview is ever alive — modality is just the cheap way most of them enforce that guarantee.** Our two sins map 1:1: (1) the ops aren't presented as two named procedures (fix: two named blocks, verb-first Apply, not shared sliders under "Cleanup"); (2) the single-slot handoff is silent (fix: either lock the other op while one is uncommitted, or announce the reset — both are exactly what the lead pre-approved, and both are what modal apps do implicitly). Keep the single preview slot and sequential-apply; they are correct — they only need to be made *visible* rather than *silent*.

---

## Sources
- Adobe — Apply filters / Filter dialog & Preview: https://helpx.adobe.com/photoshop/desktop/effects-filters/get-started-with-filters/apply-filters.html ; Smart Filters: https://helpx.adobe.com/photoshop/using/applying-smart-filters.html ; Elements filters: https://helpx.adobe.com/photoshop-elements/using/filters.html
- GIMP 2.10 — Common filter features (Presets/Preview/Split view/Blending/Clipping): https://docs.gimp.org/2.10/en/gimp-filters-common.html ; Apply Canvas example: https://docs.gimp.org/2.10/en/gimp-filter-apply-canvas.html
- Aseprite — Despeckle (median) command API: https://www.aseprite.org/api/command/Despeckle ; Color Mode / quantization: https://www.aseprite.org/docs/color-mode/
- Krita — Filters Dialog wiki: https://community.kde.org/Krita/Filters_Dialog ; Filter Masks: https://docs.krita.org/en/reference_manual/layers_and_masks/filter_masks.html ; Filter Layers: https://docs.krita.org/en/reference_manual/layers_and_masks/filter_layers.html ; "How to Add a Filter in Krita": https://www.virtualcuriosities.com/articles/1289/how-to-add-a-filter-in-krita
- paint.net — Effects menu docs: https://www.getpaint.net/doc/latest/EffectsMenu.html
- Affinity Photo — Applying filters: https://s3-eu-west-1.amazonaws.com/affinity-docs/help/photo/en-US.lproj/pages/Filters/filters_applying.html ; Live filter layers help: https://www.affinity.studio/help/layers-livefilters/
- Lightroom Classic — Develop module (non-destructive parametric): https://helpx.adobe.com/lightroom-classic/help/develop-module-options.html
- Canva Magic Eraser: https://www.canva.com/features/magic-eraser/ ; Recraft upscaler/enhance: https://www.recraft.ai/image-upscaler
- NN/g — Cancel vs Close: https://www.nngroup.com/articles/cancel-vs-close/
- Oracle ADF — Warning on Unsaved Changes pattern: https://www.oracle.com/application-development/technologies/adf/unsaveddatawarning.html
- Baymard — Accordion & Tab design: https://baymard.com/blog/accordion-and-tab-design ; Onething — Tabs vs Accordions: https://www.onething.design/post/tabs-vs-accordions
