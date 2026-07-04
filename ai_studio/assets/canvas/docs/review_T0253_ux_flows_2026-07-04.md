# T0253 — Canvas UX-Flows Review (dimension 1 of 3)

Scope: user-facing flows + interaction design of `ai_studio/assets/canvas/site/*.js` +
`canvas.html`, judged from the code. Benchmark: Figma. Lead taste: explicit over magic,
loud errors, minimal ceremony, works in pixels daily. Method: traced actual code paths;
every finding cites `file:line`. Not booted — the JS is the source of truth for gestures
and flows and was read end-to-end.

---

## Strengths (what genuinely works, why)

- **Feedback layer is excellent and matches the lead's taste.** Toast kinds are principled:
  info auto-hides, errors PERSIST until dismissed and are never swallowed, exports pin with
  download links, long ops show a spinner that resolves in place and can be cancelled while
  queued (`toasts.js:1-17,213-255`). The long-op limiter caps python spawns at 2 and keeps
  the canvas interactive (`toasts.js:191`, `long_op_queue.mjs`). This is the backbone of the
  whole tool and it is done right.
- **One-gesture / one-undo discipline is everywhere.** Multi-delete, multi-move, paste,
  duplicate, batch alpha, batch reorder all collapse to ONE journaled op so a single Ctrl+Z
  steps a whole gesture (`actions.js:173-204,357-378,473-483`; `workspace.js:1917-1986`).
  Plus a Photoshop history palette with click-to-jump (`history_panel.js`, `actions.js:1154`).
  Undo/redo are intentionally toast-free (high frequency, already visible) — the right call
  (`actions.js:1128-1134`).
- **Explicit-over-magic refusals with IN-PANEL reasons.** Rotated/flipped elements gray out
  Detect/Slice/Alpha/Cleanup and show the SAME sentence the op layer refuses with
  (`inspector.js:445,503-508,563-568,640-645`); Distribute is disabled with an always-visible
  "Select 3+ objects" hint, not just a tooltip (`inspector.js:1355-1360`); Generate/Expand
  disable on empty prompt and say why (`inspector.js:1889-1902`); the rotation Reset button
  holds reserved space so the -90/+90 buttons never shift under a second click
  (`inspector.js:1393-1421`). This is exactly the lead's "loud, explained" stance.
- **Figma-faithful selection/scope model.** Enter/exit group scope, marquee at the current
  scope, hover affordance showing what a click will grab, scope breadcrumb "Screen ▸ Button —
  Esc to exit", Esc unwinds one level at a time (`workspace.js:183-202,1307-1318,2132-2136`;
  `canvas.js:249-280`). Sophisticated and coherent.
- **Live cleanup preview is a highlight.** Debounced (~350ms) non-destructive on-canvas
  preview, a stale-request guard, a Hold-to-compare button, nothing written until Apply
  (`inspector.js:628-865`, `workspace.js:244-287`). Best-in-class interaction in this file.
- **Minimal ceremony where it counts.** Instant project create with no name prompt
  (`home.js:136-144`), inline two-step deletes instead of `confirm()` (`home.js:76-100`),
  auto-width live text editing (`workspace.js:526-551`), export Size control redesigned to be
  typing-primary with unit conversion (`inspector.js:976-1112`).
- **Tool parity + agent handoff.** Page, CLI, and chat all drive the same ops layer; "Copy ID"
  yields a `canvas://` ref for pasting into chat (`context_menu.js:90-158`). Smart guides on
  move with Ctrl-bypass (`workspace.js:1738-1746`). Keyboard uses `event.code`, so Ctrl+Z
  survives a Cyrillic layout (`canvas.js:125-131`) — a real, thoughtful detail.

---

## Weaknesses ranked by user pain

### 1. No arrow-key nudge of the selection (pixel muscle-memory miss). HIGH
`onKeyDown` handles V/H/T, undo/redo, group, copy/paste/dup, z-order, zoom, Esc, Delete —
but has NO ArrowLeft/Right/Up/Down branch (`canvas.js:125-302`, verified by grep: no arrow
handling). For a lead who "works with pixels daily," the absence of 1px/Shift=10px nudge is
the single biggest Figma-muscle break: precise positioning forces the mouse or the inspector
X/Y number fields.

### 2. The headline AI feature (recipe/style cards) is discoverable ONLY by right-clicking empty canvas. HIGH
`createRecipeCardAction`/`createStyleCardAction` are wired to exactly two places: the empty-
canvas context menu (`context_menu.js:278-281`; grep confirms no other caller). There is no
tool-rail icon, no top-bar button, no inspector entry. The top bar is Add image / Undo / Redo
/ History / Chat / Fit / 100% (`canvas.html:66-78`); the tool rail is select/pan/text only
(`canvas.html:82-92`). A whole generation workflow hangs off a right-click on blank space.

### 3. Multi-select export cannot set shared export settings. HIGH
Select N sliced sprites → `renderMulti` shows an Align row, an optional batch-Alpha, a note
"Each element exports its own settings (1x png by default)", and one Export button
(`inspector.js:2214-2237`). There is NO shared row editor — you cannot say "export all these
at 2x png." Exporting a spritesheet's sprites at 2x means opening each sprite and editing its
Export rows one at a time. Figma applies export settings across a multi-selection; this is a
daily-loop tax.

### 4. The Regions empty-state instruction contradicts actual behavior. HIGH (cheap)
The Regions section tells the user "No regions yet. Double-click the image to draw one."
(`inspector.js:467`). But `onDblClick` only enters region-edit when the image ALREADY has
regions; a no-region image just gets selected (`workspace.js:2287-2296`) — a deliberate change
so a stray tap can't create an accidental region. So the ONE instruction shown for the
first-region case is wrong; the real path is right-click → "Edit regions"
(`context_menu.js:197-206`) or Detect. First-region drawing is a dead end for anyone who
follows the on-screen text.

### 5. A recipe card's references are invisible during setup. MEDIUM-HIGH
`renderStyle` lists every image member with a "ref" badge / "Make ref" button
(`inspector.js:1978-2010`), so a style card shows exactly what travels. `renderRecipe` shows
Prompt / Engine / Style / Generate / Expand but NO members/refs list (`inspector.js:1842-1947`).
The images that will be sent as refs are only visible AFTER generation, in the minted
element's read-only Generation → References list (`inspector.js:1540-1565`). During setup the
question "what am I about to send?" is unanswerable from the Recipe panel.

### 6. Copy/Duplicate are keyboard-only; no Ctrl+A, no Ctrl+Shift+G. MEDIUM
The element context menu is Edit regions / Flip / (Group) / Order / Copy ID / Delete
(`context_menu.js:183-230`) — there is no "Copy" or "Duplicate" item (grep confirms only
"Copy ID", the agent-ref action). Duplicate is Ctrl+D only, copy is Ctrl+C only
(`canvas.js:182-193`). No select-all (Ctrl+A) and no Ctrl+Shift+G ungroup (ungroup is
context-menu-only, `context_menu.js:256`). A mouse-first user cannot duplicate; Figma users
will reflexively hit Ctrl+Shift+G and Ctrl+A and get nothing.

### 7. Multi-minute generation shows only a static spinner. MEDIUM
`generateFromRecipeAction` runs a `runLongOp` with the fixed label "Generating… (codex/agy,
minutes)" (`actions.js:928-949`); dual-plate-generate is "~2-4 min" (`actions.js:513-524`).
The toast never updates — no step, no elapsed, no streamed progress. Meanwhile the CHAT panel
DOES stream progress messages into its pending bubble (`chat_panel.js:338-341`). So the same
codex work is legible in one surface and an opaque multi-minute spinner in the other. On a
2-4 minute op the user cannot tell "working" from "stuck."

### 8. Group resize grows from a fixed top-left regardless of handle. MEDIUM
A scale drag pins a group's origin at grab-time x/y and only tracks w/h, so dragging a group's
NW/SW/NE handle still grows it from the top-left (self-documented skeleton limitation,
`workspace.js:1870-1882`). Against Figma this reads as broken — the handle you grab doesn't
behave like the anchor.

### 9. The single-image inspector is a long stack; Export (the common terminal action) is last. MEDIUM
`renderElement` emits Name, Position & Size, (Align), Regions, Alpha, Cleanup, Extracted
prompts, Generation, Provenance, Meta, Export — in that order (`inspector.js:1642-1683`). The
everyday add→slice→export loop scrolls past three AI sections (Extracted/Generation/Cleanup)
to reach Export at the bottom. Per-section collapse persists globally (`inspector.js:201-224`),
which helps a returning user, but the default is a tall scroll and the ordering buries the
most-used action.

### 10. Home has no sort/search; random default titles worsen the scan; no in-workspace switcher. MEDIUM
`render` lists `state.projects` in API order with a "+ New project" card first
(`home.js:160-166`); there is no sort, filter, or search, and `loadProjects` returns the array
untouched (`app.js:396-399`). New projects get a random default title (`home.js:133-135`), so a
lead who doesn't rename accumulates a wall of random names with no way to sort by recent. And
switching projects is always a round-trip: Back to Projects → home grid → click
(`canvas.js:70-81`, no switcher in `workspace.js`). Fine at 5 projects; painful at 50.

### 11. The chat panel's niche vs direct manipulation is unsignposted. MEDIUM
Chat acts on the selected refs through the same ops layer the buttons use, with the placeholder
"Ask the agent to act on the selection…" (`canvas.html:169`) and empty state "ask the agent to
act on the selection" (`chat_panel.js:256`). That overlaps the direct buttons almost exactly
("alpha cutout these 3" = the batch-Alpha button OR a chat sentence). The chat's real value
(multi-step / conditional / cross-object requests) is never surfaced — no examples, no
capability hints — so it risks being ignored or used for things a button does more directly.

### 12. Snap applies to move but not to scale/rotate. LOW-MEDIUM
Smart guides run on element/group/selection MOVE drags (`workspace.js:1738-1812`) but scale and
rotate deliberately carry no snap candidates (`workspace.js:884-890,1838-1841`). Figma snaps
resize edges to neighbors; here a resize never aligns to another object.

### 13. Alt-peek clip ghost is fully hidden. LOW
Holding Alt reveals the clipped-away portion of a selected element (`canvas.js:141-144`,
`workspace.js:671-685`) — a genuinely useful "where did my sprite go" aid with zero
discoverability (no hint, no tooltip, no menu). Also Ctrl-drag-bypasses-snap and
Shift/Alt scale modifiers have no on-screen hints (Figma-standard, but invisible here).

### 14. One toast is in Russian while the UI is English. LOW
The export-cancel toast is "Отмена в диалоге — экспорт отменён." (`actions.js:670`) amid an
otherwise-English surface. Harmless to the lead, but inconsistent.

### 15. No next-action signposting after Detect. LOW
Detect resolves to "Detected N region(s)." (`actions.js:418`) with no nudge toward the next
step (double-click to review, or Slice). The loop relies on the user already knowing the path.

---

## Do-differently (design alternatives, each with a tradeoff)

- **Add arrow-nudge + Ctrl+A + Ctrl+Shift+G to `onKeyDown`.** Closes the three biggest Figma
  muscle-memory gaps. Tradeoff: arrow-nudge is a new batched moveNodes gesture (must coalesce
  repeats into few journal entries, like the drag commit) — a little plumbing, not a redesign.
- **Give recipe/style cards a real entry point.** A tool-rail "card" icon or a top-bar "+ Card"
  splits the AI loop out of the right-click. Tradeoff: adds one control to a deliberately lean
  rail; mitigate by keeping the context-menu path too (parity, not replacement).
- **Add a shared export-settings editor to the multi-selection inspector** (edit rows once →
  `setExportRows` applied to every selected element, one op). Tradeoff: "each element its own
  settings" is a defensible stance, but the 2x-batch case is common enough to warrant a
  "apply to all" affordance; keep per-element overrides underneath.
- **Mirror the Style card's Members list in the Recipe card** so refs are visible before
  Generate. Tradeoff: near-zero — reuse `renderStyle`'s plate-thumb rows; only question is
  whether recipe refs are "all image members" or a curated subset (decide, then show it).
- **Stream generation progress into the long-op toast** (route codex step messages the way the
  chat panel already does). Tradeoff: needs a progress channel from the generate op to
  `runLongOp`; the chat SSE reader (`chat_panel.js:107-128`) is a working template.
- **Fix the Regions empty-state hint and add a "Draw region" button** to the Regions section
  (enters mode B + Rect tool) so the first region has a mouse-discoverable path. Tradeoff: the
  lead removed Edit/+Add as "redundant" (`inspector.js:473-474`) — but that removal is exactly
  what left the first-region case with only a wrong instruction; one button ≠ the old clutter.
- **Reorder the single-image inspector** so the daily loop (Regions/Alpha/Export) sits above
  the AI sections (Extracted/Generation), or gate the AI sections behind a disclosure.
  Tradeoff: breaks strict Figma "Export always last" convention — but this isn't Figma's
  content model, and the everyday action shouldn't be the longest scroll.
- **Add a lightweight recent/sort to home + an in-workspace project switcher.** Tradeoff: more
  chrome on a currently-clean home; a sort-by-updated toggle is the cheap 80%.

---

## Top-10 prioritized fixes

| # | Fix | Size | Files |
|---|-----|------|-------|
| 1 | Add arrow-key nudge (1px / Shift=10px) for the selection, coalesced into few undo entries | increment | `site/canvas.js` (onKeyDown), `site/actions.js` (moveNodesTo) |
| 2 | Fix the Regions empty-state text and add a "Draw region" button that enters mode B + Rect tool | quick-win | `site/inspector.js:467,471-509` |
| 3 | Add a shared export-settings editor to the multi-selection inspector ("apply to all") | increment | `site/inspector.js:2214-2237`, `site/actions.js:634` |
| 4 | Show the recipe card's refs/members list in the Recipe section (mirror Style card) | quick-win | `site/inspector.js:1842-1947` (reuse 1978-2010) |
| 5 | Give recipe/style cards a top-bar or tool-rail entry point (keep the menu path) | increment | `site/canvas.html:66-92`, `site/workspace.js` (init), `site/actions.js:867,957` |
| 6 | Add Ctrl+A (select all), Ctrl+Shift+G (ungroup), and Copy/Duplicate context-menu items | quick-win | `site/canvas.js:125-302`, `site/context_menu.js:183-230` |
| 7 | Stream generation/codex progress into the long-op toast (not a static spinner) | increment | `site/actions.js:928-949,513-524`, `site/toasts.js:213-255` |
| 8 | Fix group resize so the grabbed handle is the anchor (not always top-left) | increment | `site/workspace.js:1867-1893`, `site/viewport.mjs` (mapItemBox) |
| 9 | Reorder the single-image inspector so Regions/Alpha/Export precede the AI sections | quick-win | `site/inspector.js:1642-1683` |
| 10 | Home: sort-by-updated (+ optional search) and an in-workspace project switcher | increment | `site/home.js:160-166`, `site/app.js:396-399`, `site/canvas.js:70-114` |

Cheap high-value extras (below the top 10): scale/rotate snap parity
(`workspace.js:884-890`); an Alt-peek / snap-bypass hint somewhere discoverable; a "now review
or Slice" nudge after Detect (`actions.js:418`); English-ize the export-cancel toast
(`actions.js:670`).
