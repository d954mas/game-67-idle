# T0253 — Canvas Site: Visual-UI Review (dimension 2 of 3)

Scope: visual design + UI consistency of `ai_studio/assets/canvas/site/` — judged from
`canvas.css` (2219 lines), the DOM built in `inspector.js` / `layers_panel.js` /
`chat_panel.js` / `home.js`, and the 2D chrome in `workspace.js`. Read-only.
Live evidence: booted a TEST server on :8781 (never touched the lead's :8780 / pid 3368),
headless-Chrome (SwiftShader) rendered the real `demo` project (66 elements, recipe +
style cards) and drove a CDP selection to render the fully-populated inspector.
Screenshots in scratchpad: `canvas_full.png`, `canvas_home.png`, `insp_full.png`.

Method note: this is the aesthetic/consistency pass. Interaction bugs, a11y semantics, and
op-parity are dimensions 1 and 3 — I flag only where a visual choice is the root cause.

---

## Strengths

1. **The base token set is coherent and the dark theme is genuinely calm.** `:root`
   (canvas.css:1-16) defines a tight, tasteful 12-swatch palette (bg/panel/panel-soft/line/
   text/muted + 6 accents). Panels, separators, muted text, and inputs all pull from it
   (`var(--line)` 39x, `var(--muted)` 39x, `var(--panel-soft)` 20x). The result reads as one
   surface, not a patchwork — confirmed in every screenshot.

2. **Layout-stability discipline is real and shows the lead's scars healing.** The rotation
   Reset is rendered-but-**disabled** at 0°, never hidden, precisely so the −90/+90 buttons
   don't jump under the cursor (inspector.js:1397-1425, comment cites "нажал на -90 …кнопка
   -90 уехала"). Region-delete and layer eye targets reserve their width so nothing shifts on
   hover (canvas.css:1785-1804). `scrollbar-gutter: stable` on every scroll pane
   (canvas.css:812, 1706, 1047). This is the single best habit in the codebase.

3. **Collapsed panels degrade to a clear re-open control, not a dead strip.** The layers
   panel collapses to a 34px rail with a vertical "Layers" label + hamburger
   (canvas.css:324-365, canvas.html:96-100) — a named affordance, not a mystery sliver.

4. **On-canvas card typing (Recipe/Style) reads at a glance.** Dashed frame + accent name
   pill + a type chip ("Recipe" amber / "Style" cyan) makes a workshop card distinct from a
   plain group even before selection (workspace.js:742-826). Visible and legible in
   `canvas_full.png`. The layers tree mirrors it with `.layer-card-chip` (canvas.css:526-544).

5. **Feedback layer is well-considered.** Toasts use a left-border accent keyed to severity
   (info/success/error/pinned/progress), a pure-CSS spinner, and are inset past the 264px
   inspector so they never cover the Export button (canvas.css:1427-1576). Clean system.

6. **Home grid + empty states are quiet and correct.** Dashed "New project" card, checker
   covers, two-step in-place delete confirm (no `confirm()`); the empty inspector is one
   centered line + one honest primary ("Export project (N screens)"). See `canvas_home.png`.

---

## Weaknesses (ranked)

### W1 — The inspector is a ~5-screen scroll with every section expanded by default (top issue)
**What:** A single minted image element renders **9-11 stacked sections**, all expanded on
first view. Measured live on `demo`'s "Recipe card codex": **9 sections, inspector
scrollHeight 3823px vs 793px visible ≈ 4.8 screens of vertical scroll** (`insp_full.png` +
CDP readout). Section list for a minted node can reach 11: Position&Size, Align, Regions,
Alpha, Cleanup, Slice-9, Extracted prompts, Generation, Provenance, Meta, Export
(inspector.js:1642-1683, +1516 Slice-9).
**Evidence:** `collapsible()` seeds collapsed state only from persisted localStorage
(inspector.js:229-232, 203-213) — there is **no default-collapsed set**, so a fresh eye sees
all 3823px. Order is workflow-sane (Position → Regions → Alpha → Cleanup → … → Export last,
inspector.js:1647-1682) — the problem is length, not order.
**Why:** The lead has to scroll past low-frequency provenance sections (Meta / Generation /
Extracted prompts / Slice-9) to reach the Export button on every element. That is the exact
"scrolls into noise" failure this review was asked to find.
**Fix (M):** Ship a default-collapsed set for low-frequency sections (Meta, Generation,
Extracted prompts, Provenance, Slice-9) — collapse them unless the user expands. One
constant + one check in `collapsible()`/`isCollapsed()`. Cuts first-view height by ~50%.

### W2 — Two teal accents mean two different things; amber means two different things
**What:** The on-canvas accent set is *mostly* a system but has two collisions that a sharp
eye will catch:
- **Two cyans.** `--cyan #3fc7ba` = region-edit isolation, region badges, marquee-for-region
  (workspace.js:382,846; canvas.css:1675,2018,2076) **vs** `--style-accent #4aa9d7` = Style
  card frame/chip/ref (workspace.js:728,365; canvas.css:15,541). Two near-identical teals
  with unrelated meanings. In `canvas_full.png` the "Style" chip (blue-teal) sits meters from
  region-cyan chrome — confusable.
- **Amber is overloaded.** `--amber #d7a14a` is BOTH the Recipe-card accent AND a plain
  group's *selected* stroke/name-pill (workspace.js:750,779). Distinguished only by
  dashed-vs-solid + line width. A selected plain group and a recipe card both go amber.
**Why:** The stated brief — "do the accents form a legible system or a rainbow?" — the answer
is "a legible system with two overloaded hues." Selection=blue, region=cyan, recipe=amber,
style=cyan#2, snap=pink is 5 hues where 4 would read cleaner.
**Fix (M):** Either (a) pull Style off the cyan family onto a clearly distinct hue (e.g. a
violet) so region-cyan owns teal alone, or (b) accept the amber/selection overload as
intentional but document it in one place. Cheapest: nudge `--style-accent` toward violet.

### W3 — The primary-blue button is section-local, so the panel has no primary anchor — and the disabled primary can out-shout the real next step
**What:** `.primary` (blue #2764bd) is applied per-section: Slice (Regions), Apply (Cleanup),
Export, Generate (Recipe), Render group. So scrolling the inspector you meet a *ladder* of
equally-loud blue buttons; none is "the" action. Worse, in the Regions section at 0 regions,
**Slice is styled primary-blue but disabled**, while the actual next step (Detect) is a plain
secondary button (inspector.js:488-504, detectBtn:483). The eye lands on the disabled loud
button, not the enabled quiet one (visible in `insp_full.png`).
**Why:** "Can you find the primary action at a glance?" — locally yes, panel-wide no; and the
one place it matters (empty Regions) the emphasis is inverted.
**Fix (S):** In Regions, when `regions.length === 0`, make **Detect** the primary and drop
Slice to secondary (swap the two `className`s conditionally). Panel-wide anchor is a larger
call — see Do-differently.

### W4 — A whole "interaction blue" palette lives as raw hex outside `:root` (token gap)
**What:** The documented-deliberate duplication is only workspace.js's bare-2D context
(RECIPE_ACCENT/STYLE_ACCENT/pink — fair, no CSS-var access there). But **canvas.css itself**
hardcodes an entire interaction family that has no token and repeats:
- `#2764bd` primary-action fill — **3x**, untokenized (canvas.css:70,284; the single most
  important interactive color in the app).
- `#91bbff` accent/hover border — **7x**, untokenized (66,72,286,127,689,978,1199).
- `#24405f` selected-row background — **3x** (454,1758,1687).
- `#77a7ff` — written raw **8x** even though it IS `--blue` (626,429,768,1011,1030,1038,2033…).
  So the same value is both `var(--blue)` (14x) and raw (8x).
- plus one-offs `#4a3b1e` (selected group bg), `#ffca6a` (a *third* amber), `#f8fbff`,
  `#a8c4ff`, raw `#4aa9d7`/`#3fc7ba` despite tokens existing.
**Evidence:** hex census (canvas.css): `#77a7ff`×8, `#91bbff`×7, `#2764bd`×3, `#24405f`×3.
**Why:** The bare-2D rationale does not cover pure CSS — these are gratuitous. If the lead
ever asks to retune the interaction blue (very likely for a "figma-like" tool), it's a
find-and-replace across 21 raw sites instead of one token edit.
**Fix (M):** Add `--accent-primary:#2764bd; --accent-border:#91bbff; --sel-bg:#24405f;
--on-accent:#f8fbff;` to `:root` and replace the raw sites; replace the 8 raw `#77a7ff` with
`var(--blue)`. Zero visual change, one-time.

### W5 — Focus-visible styling is inconsistent (only 2 of many interactive surfaces)
**What:** Custom focus styling exists on exactly `.insp-align-btn:focus-visible` (cyan
outline, canvas.css:1970) and `#chat-input:focus` (canvas.css:1197). Every other button,
`.insp-input`, select, `.inline-input`, layer row, history row, context item relies on the UA
default ring. `#stage` / `.text-edit-overlay` / `#chat-input` explicitly kill their outline.
**Why:** Keyboard focus reads differently (or not at all on some dark-on-dark controls) across
the app. Low user-visible severity today (mouse-first tool) but an easy consistency win and an
a11y floor.
**Fix (S):** One global rule: `button:focus-visible, .insp-input:focus-visible,
select:focus-visible { outline: 1px solid var(--accent-border); outline-offset: 1px; }`.

### W6 — Sub-24px hit targets on several icon controls
**What:** Chat-close / history-close ≈ 20px (canvas.css:1011-1015,726-730), layer eye 22px
(546-554), region-del 22px (1787-1799), toast-close 20px (1559-1570). Tool rail (34px, 273),
align (28px, 1954), color (34x30) are fine.
**Why:** The lead once flagged mis-hits ("кнопка уехала и я не попал"). 20-22px targets are
below the comfortable ~24-28px floor for repeated clicks, especially the eye toggle used
constantly in the layers tree.
**Fix (S):** Bump the eye and the two close glyphs to 24-26px box (padding only; icon
unchanged). No layout ripple — they're already `flex:none` with reserved width.

### W7 — At Fit zoom, card name-pills + type-chips pile up and overlap
**What:** In `canvas_full.png` (6% fit of a large canvas) the top cluster stacks
"A full-body centered blocky game-avatar" / "Recipe card" pills and "Recipe" chips on top of
each other, unreadable. Labels are drawn at fixed 12px screen size regardless of zoom
(workspace.js:774), so at Fit they don't shrink with content and collide.
**Why:** Minor and zoom-dependent (fine at working zoom), but Fit is the default landing view
of a busy project, so first impression is "cluttered."
**Fix (S, optional):** Hide the prompt-preview and/or the type chip below a zoom threshold
(e.g. `vp.scale < 0.25`), keeping only the name pill; or fade pills that overlap. Low priority.

### W8 — Minor one-offs / inconsistencies (not broken, worth a sweep)
- `--cyan` appears both as the text color of `.thumb-text` **and** hardcoded as a fallback
  `var(--cyan, #3fc7ba)` in the same rule (canvas.css:476) — belt-and-suspenders fallback that
  will silently drift if `--cyan` ever changes.
- Three ambers now exist: `--amber #d7a14a`, bright `#ffca6a` (selected region number,
  1692/1773), and the `rgba(215,161,74,…)` tints. Defensible (emphasis variants) but
  undocumented.
- `.insp-seg-btn.primary` reuses the primary token for the *active segment* of a segmented
  control (Denoise 1/2/3, Base Source/Canvas) — so "primary" means both "the main action" and
  "the selected segment." Semantic overload of one class (inspector.js:702,1136).
- Section-title convention is consistent (uppercase 11px muted, canvas.css:868-874) — good —
  but count badges vary: Regions uses cyan `.badge`, Align uses muted `.insp-align-badge`,
  layers member-count uses cyan `.badge` again. Two badge languages for "a number in a header."

---

## Do-differently (alternatives + tradeoffs)

**D1 — Inspector: default-collapse + optional "pin frequent."**
Rather than only persisting user collapses, ship an opinionated default: Position&Size +
(context section: Regions/Alpha/Cleanup) + Export open; Meta/Generation/Extracted/Provenance/
Slice-9 collapsed. *Tradeoff:* a power user who wants Meta open pays one click (persisted after
that). Cheap, high payoff. Alternative considered — a tabbed inspector (Design | Prep |
Export) — rejected: heavier, breaks the single-scroll Figma mental model the tool already
commits to, and the lead's flow is top-to-bottom per element.

**D2 — Give the panel one primary anchor.** Keep section run-buttons but demote them to a
"filled-quiet" weight (tinted, not solid blue) and reserve solid `--accent-primary` for the
one true commit per context (Export for an element, Render group for a group). *Tradeoff:*
touches ~6 button classNames and needs a new "filled-secondary" style; benefit is the eye
finally has an anchor. Alternative — a sticky Export bar pinned to the inspector bottom —
stronger but more layout work; worth it if D1 alone doesn't tame the scroll.

**D3 — Accent system: 4 hues, documented.** Blue=selection/plain-group, Cyan=region/edit,
Amber=recipe, Violet=style, Pink=snap(transient). Move Style off teal (W2), and write the
5-line accent legend into `canvas.css` `:root` as a comment so future sections don't invent a
6th hue. *Tradeoff:* one accent value changes (users re-learn "style is violet") — but it's an
internal tool and the current two-teal ambiguity is worse.

**D4 — Tokenize the interaction palette (W4) before adding any more sections.** This is
pure hygiene with zero visual delta; doing it now stops the raw-hex count from growing with
every new T02xx section. Half-hour job.

---

## Top-10 fixes (ranked, sized)

| # | Fix | Files (evidence) | Size | Impact |
|---|-----|------------------|------|--------|
| 1 | Default-collapse low-frequency inspector sections (Meta/Generation/Extracted/Provenance/Slice-9) | inspector.js:229-232, 203-213, 1660-1679 | M | Cuts 3823px first-view ≈ in half |
| 2 | Tokenize `--accent-primary #2764bd`, `--accent-border #91bbff`, `--sel-bg #24405f`, `--on-accent #f8fbff`; replace 21 raw sites; swap 8 raw `#77a7ff`→`var(--blue)` | canvas.css:1-16,66-72,284,454,689,978,1199,1687,1758 | M | Retune-in-one-place; no visual change |
| 3 | Move `--style-accent` off the cyan family (→ violet) so region-cyan is unambiguous | canvas.css:15; workspace.js:728,365 | S | Kills the two-teal confusion |
| 4 | Regions at 0 regions: make Detect primary, Slice secondary | inspector.js:483-504 | S | Fixes inverted emphasis on empty state |
| 5 | Global `:focus-visible` rule for buttons/inputs/selects | canvas.css (new rule) | S | Consistent keyboard focus + a11y floor |
| 6 | Bump eye / chat-close / history-close / toast-close to 24-26px | canvas.css:546-554,726-730,1011-1015,1559-1570 | S | Fewer mis-hits on repeated controls |
| 7 | Demote per-section run buttons to filled-quiet; reserve solid primary for Export/Render (panel anchor) | inspector.js:490,749,1231,2103; canvas.css:69-73 | M | One clear primary per context |
| 8 | Unify header count-badges to one class (cyan `.badge` vs muted `.insp-align-badge`) | canvas.css:514-521,1986-1993 | S | One "number in a header" language |
| 9 | Hide prompt-preview/type-chip on cards below ~0.25 zoom to stop Fit-view pileup | workspace.js:792-826,774 | S | Cleaner default landing view |
| 10 | Document the accent legend + the 3 ambers as a `:root` comment; drop the `var(--cyan,#3fc7ba)` double-fallback | canvas.css:1-16,476 | S | Prevents a 6th hue creeping in |

---

*No dead CSS found in an 8-class spot-check (all referenced in exactly 1 JS file each).
`.hidden !important`, custom scrollbars, and the studio-shell integration are all clean.*
